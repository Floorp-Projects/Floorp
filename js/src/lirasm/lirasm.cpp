/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is LIR Assembler code, released 2009.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Graydon Hoare <graydon@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <vector>
#include <algorithm>
#include <map>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <cassert>

#ifdef AVMPLUS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include <stdlib.h>
#include <math.h>

#include "nanojit/nanojit.h"
#include "jstracer.h"

using namespace nanojit;
using namespace std;

static avmplus::GC gc;

struct LasmSideExit : public SideExit {
    size_t line;
};

typedef JS_FASTCALL int32_t (*RetInt)();
typedef JS_FASTCALL double (*RetFloat)();
typedef JS_FASTCALL GuardRecord* (*RetGuard)();

struct Function {
    const char *name;
    struct nanojit::CallInfo callInfo;
};

enum ReturnType {
    RT_INT32 = 1,
    RT_FLOAT = 2,
    RT_GUARD = 4
};

#ifdef DEBUG
#define DEBUG_ONLY_NAME(name)   ,#name
#else
#define DEBUG_ONLY_NAME(name)
#endif

#define FN(name, args) \
    {#name, \
     {(uintptr_t) (&name), args, 0, 0, nanojit::ABI_CDECL \
      DEBUG_ONLY_NAME(name)}}

const int I32 = nanojit::ARGSIZE_LO;
const int I64 = nanojit::ARGSIZE_Q;
const int F64 = nanojit::ARGSIZE_F;
const int PTRARG = nanojit::ARGSIZE_LO;

const int PTRRET =
#if defined AVMPLUS_64BIT
    nanojit::ARGSIZE_Q
#else
    nanojit::ARGSIZE_LO
#endif
    ;

class LirasmFragment {
public:
    union {
        RetFloat rfloat;
        RetInt rint;
        RetGuard rguard;
    };
    ReturnType mReturnType;
    Fragment *fragptr;
    map<string, LIns*> mLabels;
};

typedef map<string, LirasmFragment> Fragments;

class Lirasm {
public:
    Fragmento *mFragmento;
    LirBuffer *mLirbuf;
    LogControl mLogc;
    bool mVerbose;
    avmplus::AvmCore s_core;
    Fragments mFragments;
    vector<CallInfo*> mCallInfos;

    Lirasm(bool verbose);
    ~Lirasm();
};

class LirasmAssembler {
private:
    Lirasm *mParent;
    Fragment *mFragment;
    string mFragName;
    map<string, LIns*> mLabels;
    LirWriter *mLir;
    LirBufWriter *mBufWriter;
    LirWriter *mCseFilter;
    LirWriter *mExprFilter;
    LirWriter *mVerboseWriter;
    multimap<string, LIns *> mFwdJumps;
    map<string,pair<LOpcode,size_t> > op_map;

    size_t mLineno;
    LOpcode mOpcode;
    size_t mOpcount;

    bool mInFrag;

    char mReturnTypeBits;
    vector<string> mTokens;

    void lookupFunction(const char*, CallInfo *&);
    void need(size_t);
    istream& read_and_tokenize_line(istream&);
    void tokenize(string const &tok_sep);
    LIns *ref(string const &);
    LIns *do_skip(size_t);
    LIns *assemble_call(string &);
    LIns *assemble_general();
    LIns *assemble_guard();
    LIns *assemble_jump();
    LIns *assemble_load();
    void bad(string const &msg);
    void beginFragment();
    void endFragment();
    void extract_any_label(string &op, string &lab, char lab_delim);
    void patch();

public:
    LirasmAssembler(Lirasm &);
    void assemble(istream &);
};

Function functions[] = {
    FN(puts, I32 | (PTRARG<<2)),
    FN(sin, F64 | (F64<<2)),
    FN(malloc, PTRRET | (PTRARG<<2)),
    FN(free, I32 | (PTRARG<<2))
};

void
LirasmAssembler::lookupFunction(const char *name, CallInfo *&ci)
{
    const size_t nfuns = sizeof(functions) / sizeof(functions[0]);
    for (size_t i = 0; i < nfuns; i++)
        if (strcmp(name, functions[i].name) == 0) {
            *ci = functions[i].callInfo;
            return;
        }

    Fragments::const_iterator func = mParent->mFragments.find(name);
    if (func != mParent->mFragments.end()) {
        if (func->second.mReturnType == RT_FLOAT) {
            CallInfo target = {(uintptr_t) func->second.rfloat, ARGSIZE_F, 0,
                               0, nanojit::ABI_FASTCALL, func->first.c_str()};
            *ci = target;

        } else {
            CallInfo target = {(uintptr_t) func->second.rint, ARGSIZE_LO, 0,
                               0, nanojit::ABI_FASTCALL, func->first.c_str()};
            *ci = target;
        }
    } else {
    ci = NULL;
    }
}

istream &
LirasmAssembler::read_and_tokenize_line(istream &in)
{
    char buf[1024];
    string tok_sep(" \n\t");

    mTokens.clear();

    if (in.getline(buf,sizeof(buf))) {
        ++mLineno;
        string line(buf);

        size_t comment = line.find("//");
        if (comment != string::npos)
            line.resize(comment);

        line += '\n';

        size_t start = 0;
        size_t end = 0;
        while((start = line.find_first_not_of(tok_sep, end)) != string::npos &&
              (end = line.find_first_of(tok_sep, start)) != string::npos) {
            string ss = line.substr(start, (end-start));
            if (ss == "=") {
                mTokens[mTokens.size()-1] += ss;
                continue;
            }
            mTokens.push_back(ss);
        }
    }
    return in;
}

template<typename out, typename in> out
lexical_cast(in arg)
{
    stringstream tmp;
    out ret;
    if ((tmp << arg && tmp >> ret && tmp.eof()))
        return ret;
    cerr << "bad lexical cast from " << arg << endl;
    exit(1);
}

int32_t
imm(string const &s)
{
    stringstream tmp(s);
    int32_t ret;
    if ((s.find("0x") == 0 || s.find("0X") == 0) &&
        (tmp >> hex >> ret && tmp.eof()))
        return ret;
    return lexical_cast<int32_t>(s);
}

uint64_t
quad(string const &s)
{
    stringstream tmp1(s), tmp2(s);
    union {
        uint64_t u64;
        double d;
    } pun;
    if ((s.find("0x") == 0 || s.find("0X") == 0) &&
        (tmp1 >> hex >> pun.u64 && tmp1.eof()))
        return pun.u64;
    if ((s.find(".") != string::npos) &&
        (tmp2 >> pun.d && tmp2.eof()))
        return pun.u64;
    return lexical_cast<uint64_t>(s);
}

template<typename t> t
pop_front(vector<t> &vec)
{
    if (vec.empty()) {
        cerr << "pop_front of empty vector" << endl;
        exit(1);
    }
    t tmp = vec[0];
    vec.erase(vec.begin());
    return tmp;
}

void
LirasmAssembler::bad(string const &msg)
{
    cerr << "instruction " << mLineno << ": " <<  msg << endl;
    exit(1);
}

void
LirasmAssembler::need(size_t n)
{
    if (mTokens.size() != n)
        bad("need " + lexical_cast<string>(n)
            + " tokens, have " + lexical_cast<string>(mTokens.size()));
}

LIns*
LirasmAssembler::ref(string const &lab)
{
    if (mLabels.find(lab) == mLabels.end())
        bad("unknown label '" + lab + "'");
    return mLabels.find(lab)->second;
}

LIns*
LirasmAssembler::do_skip(size_t i)
{
    LIns *s = mLir->insSkip(i);
    memset(s->payload(), 0xba, i);
    return s;
}

LIns*
LirasmAssembler::assemble_jump()
{
    LIns *target = NULL;
    LIns *condition = NULL;

    if (mOpcode == LIR_j) {
        need(1);
    } else {
        need(2);
        string cond = pop_front(mTokens);
        condition = ref(cond);
    }
    string name = pop_front(mTokens);
    if (mLabels.find(name) != mLabels.end()) {
        target = ref(name);
        return mLir->insBranch(mOpcode, condition, target);
    } else {
        LIns *ins = mLir->insBranch(mOpcode, condition, target);
        mFwdJumps.insert(make_pair(name, ins));
        return ins;
    }
}

LIns*
LirasmAssembler::assemble_load()
{
    // Support implicit immediate-as-second-operand modes
    // since, unlike sti/stqi, no immediate-displacement
    // load opcodes were defined in LIR.
    need(2);
    if (mTokens[1].find("0x") == 0 ||
        mTokens[1].find("0x") == 0 ||
        mTokens[1].find_first_of("0123456789") == 0) {
        return mLir->insLoad(mOpcode,
                            ref(mTokens[0]),
                            imm(mTokens[1]));
    }
    bad("immediate offset required for load");
    return NULL;  // not reached
}

LIns*
LirasmAssembler::assemble_call(string &op)
{
    CallInfo *ci = new CallInfo();
    mParent->mCallInfos.push_back(ci);
    LIns* args[MAXARGS];

    // Assembler syntax for a call:
    //
    //   call 0x1234 fastcall a b c
    //
    // requires at least 2 args,
    // fn address immediate and ABI token.

    if (mTokens.size() < 2)
        bad("need at least address and ABI code for " + op);

    string func = pop_front(mTokens);
    string abi = pop_front(mTokens);

    AbiKind _abi;
    if (abi == "fastcall")
        _abi = ABI_FASTCALL;
    else if (abi == "stdcall")
        _abi = ABI_STDCALL;
    else if (abi == "thiscall")
        _abi = ABI_THISCALL;
    else if (abi == "cdecl")
        _abi = ABI_CDECL;
    else
        bad("call abi name '" + abi + "'");
    ci->_abi = _abi;

    if (mTokens.size() > MAXARGS)
    bad("too many args to " + op);

    if (func.find("0x") == 0) {
        ci->_address = imm(func);

        ci->_cse = 0;
        ci->_fold = 0;

#ifdef DEBUG
        ci->_name = "fn";
#endif

    } else {
        lookupFunction(func.c_str(), ci);
        if (ci == NULL)
            bad("invalid function reference " + func);
        if (_abi != ci->_abi)
            bad("invalid calling convention for " + func);
    }

    ci->_argtypes = 0;

    for (size_t i = 0; i < mTokens.size(); ++i) {
        args[i] = ref(mTokens[mTokens.size() - (i+1)]);
        ci->_argtypes |= args[i]->isQuad() ? ARGSIZE_F : ARGSIZE_LO;
        ci->_argtypes <<= 2;
    }

    // Select return type from opcode.
    // FIXME: callh needs special treatment currently
    // missing from here.
    if (mOpcode == LIR_call)
        ci->_argtypes |= ARGSIZE_LO;
    else
        ci->_argtypes |= ARGSIZE_F;

    return mLir->insCall(ci, args);
}

LIns*
LirasmAssembler::assemble_guard()
{
    LIns *exitIns = do_skip(sizeof(LasmSideExit));
    LasmSideExit* exit = (LasmSideExit*) exitIns->payload();
    memset(exit, 0, sizeof(LasmSideExit));
    exit->from = mFragment;
    exit->target = NULL;
    exit->line = mLineno;

    LIns *guardRec = do_skip(sizeof(GuardRecord));
    GuardRecord *rec = (GuardRecord*) guardRec->payload();
    memset(rec, 0, sizeof(GuardRecord));
    rec->exit = exit;
    exit->addGuard(rec);

    need(mOpcount);

    if (mOpcode != LIR_loop)
        mReturnTypeBits |= RT_GUARD;

    LIns *ins_cond;
    if (mOpcode == LIR_xt || mOpcode == LIR_xf)
        ins_cond = ref(pop_front(mTokens));
    else
        ins_cond = mLir->insImm(1);

    if (!mTokens.empty())
        bad("too many arguments");

    return mLir->insGuard(mOpcode, ins_cond, guardRec);
}

LIns*
LirasmAssembler::assemble_general()
{
    if (mOpcount == 0) {
        // 0-ary ops may, or may not, have an immediate
        // thing wedged in them; depends on the op. We
        // are lax and set it if it's provided.
        LIns *ins = mLir->ins0(mOpcode);
        if (mTokens.size() > 0) {
            assert(mTokens.size() == 1);
            ins->initLInsI(mOpcode, imm(mTokens[0]));
        }
        return ins;
    } else {
        need(mOpcount);
        if (mOpcount == 1) {
            if (mOpcode == LIR_ret)
                mReturnTypeBits |= RT_INT32;
            if (mOpcode == LIR_fret)
                mReturnTypeBits |= RT_FLOAT;

            return mLir->ins1(mOpcode,
                             ref(mTokens[0]));
        } else if (mOpcount == 2) {
            return mLir->ins2(mOpcode,
                             ref(mTokens[0]),
                             ref(mTokens[1]));
        } else {
            bad("too many operands");
        }
    }
    // Never get here.
    return NULL;
}

void
dep_u8(char *&buf, uint8_t byte, uint32_t &cksum)
{
    sprintf(buf, "%2.2X", byte);
    cksum += byte;
    buf += 2;
}

void
dep_u32(char *&buf, uint32_t word, uint32_t &cksum)
{
    dep_u8(buf, (uint8_t)((word >> 24) & 0xff), cksum);
    dep_u8(buf, (uint8_t)((word >> 16) & 0xff), cksum);
    dep_u8(buf, (uint8_t)((word >> 8) & 0xff), cksum);
    dep_u8(buf, (uint8_t)((word) & 0xff), cksum);
}

void
dump_srecords(ostream &out, Fragment *frag)
{
    // Write S-records. Can only do 4-byte addresses at the moment.

    // FIXME: this presently dumps out the entire set of code pages
    // written-to, which means it often dumps *some* bytes on the last
    // page that are not necessarily initialized at all; they're
    // beyond the last instruction written. Fix this to terminate
    // s-record writing early.

    assert(sizeof(uintptr_t) == 4);
    for (Page *page = frag->pages(); page; page = page->next) {
        size_t step = 32;
        uintptr_t p0 = (uintptr_t) &(page->code);
        for (uintptr_t p = p0; p < p0 + sizeof(page->code); p += step) {
            char buf[1024];

            // S-record type S3: 8-char / 4-byte address.
            //
            //     +2 char code 'S3'.
            //     +2 char / 1 byte count of remaining bytes (37 = addr, payload, cksum).
            //     +8 char / 4 byte addr.
            //    ---
            //    +64 char / 32 byte payload.
            //    ---
            //     +2 char / 1 byte checksum.

            uint32_t cksum = 0;
            size_t count = sizeof(p) + step + 1;

            sprintf(buf, "S3");

            char *b = buf + 2; // 2 chars for the "S3" code.

            dep_u8(b, (uint8_t) count, cksum); // Count of data bytes
            dep_u32(b, p, cksum); // Address of the data byte being emitted
            uint8_t *c = (uint8_t*) p;
            for (size_t i = 0; i < step; ++i) { // Actual object code being emitted
                dep_u8(b, c[i], cksum);
            }
            dep_u8(b, (uint8_t)((~cksum) & 0xff), cksum);
            out << string(buf) << endl;
        }
    }
}

void
LirasmAssembler::extract_any_label(string &op,
                                   string &lab,
                                   char lab_delim)
{
    if (op.size() > 1 &&
        op[op.size()-1] == lab_delim &&
        !mTokens.empty()) {

        lab = op;
        op = pop_front(mTokens);
        lab.erase(lab.size()-1);

        if (mLabels.find(lab) != mLabels.end())
            bad("duplicate label");
    }
}

void
LirasmAssembler::beginFragment()
{
    mFragment = new (&gc) Fragment(NULL);
    mFragment->lirbuf = mParent->mLirbuf;
    mFragment->anchor = mFragment;
    mFragment->root = mFragment;
    mParent->mFragments[mFragName].fragptr = mFragment;

    mBufWriter = new (&gc) LirBufWriter(mParent->mLirbuf);
    mCseFilter = new (&gc) CseFilter(mBufWriter, &gc);
    mExprFilter = new (&gc) ExprFilter(mCseFilter);
    mVerboseWriter = NULL;
    mLir = mExprFilter;

#ifdef DEBUG
    if (mParent->mVerbose) {
        mVerboseWriter = new (&gc) VerboseWriter(&gc, mExprFilter, mParent->mLirbuf->names, &mParent->mLogc);
        mLir = mVerboseWriter;
    }
#endif

    mInFrag = true;
    mReturnTypeBits = 0;
    mLir->ins0(LIR_start);
}

void
LirasmAssembler::endFragment()
{
    mInFrag = false;

    if (mReturnTypeBits == 0)
        cerr << "warning: no return type in fragment '"
             << mFragName << "'" << endl;
    if (mReturnTypeBits != RT_INT32 && mReturnTypeBits != RT_FLOAT
        && mReturnTypeBits != RT_GUARD)
        cerr << "warning: multiple return types in fragment '"
             << mFragName << "'" << endl;

    LIns *exitIns = do_skip(sizeof(SideExit));
    SideExit* exit = (SideExit*) exitIns->payload();
    memset(exit, 0, sizeof(SideExit));
    exit->guards = NULL;
    exit->from = exit->target = mFragment;
    mFragment->lastIns = mLir->insGuard(LIR_loop, NULL, exitIns);

    ::compile(mParent->mFragmento->assm(), mFragment);

    if (mParent->mFragmento->assm()->error() != nanojit::None) {
        cerr << "error during assembly: ";
        switch (mParent->mFragmento->assm()->error()) {
        case nanojit::OutOMem: cerr << "OutOMem"; break;
        case nanojit::StackFull: cerr << "StackFull"; break;
        case nanojit::RegionFull: cerr << "RegionFull"; break;
        case nanojit::MaxLength: cerr << "MaxLength"; break;
        case nanojit::MaxExit: cerr << "MaxExit"; break;
        case nanojit::MaxXJump: cerr << "MaxXJump"; break;
        case nanojit::UnknownPrim: cerr << "UnknownPrim"; break;
        case nanojit::UnknownBranch: cerr << "UnknownBranch"; break;
        case nanojit::None: cerr << "None"; break;
        }
        cerr << endl;
        std::exit(1);
    }

    LirasmFragment *f;
    f = &mParent->mFragments[mFragName];

    switch (mReturnTypeBits) {
      case RT_FLOAT:
      default:
        f->rfloat = reinterpret_cast<RetFloat>(mFragment->code());
        f->mReturnType = RT_FLOAT;
        break;
      case RT_INT32:
        f->rint = reinterpret_cast<RetInt>(mFragment->code());
        f->mReturnType = RT_INT32;
        break;
      case RT_GUARD:
        f->rguard = reinterpret_cast<RetGuard>(mFragment->code());
        f->mReturnType = RT_GUARD;
        break;
    }

    delete mVerboseWriter;
    delete mExprFilter;
    delete mCseFilter;
    delete mBufWriter;
    for (size_t i = 0; i < mParent->mCallInfos.size(); ++i)
        delete mParent->mCallInfos[i];
    mParent->mCallInfos.clear();

    mParent->mFragments[mFragName].mLabels = mLabels;
    mLabels.clear();
}

void
LirasmAssembler::assemble(istream &in)
{
#define OPDEF(op, number, args, repkind) \
    op_map[#op] = make_pair(LIR_##op, args);
#define OPDEF64(op, number, args, repkind) \
    op_map[#op] = make_pair(LIR_##op, args);
#include "nanojit/LIRopcode.tbl"
#undef OPDEF
#undef OPDEF64

    op_map["alloc"] = op_map["ialloc"];
    op_map["param"] = op_map["iparam"];

        bool singleFrag = false;
        bool first = true;
        while(read_and_tokenize_line(in)) {

        if (mParent->mLirbuf->outOMem()) {
            cerr << "lirbuf out of memory" << endl;
            exit(1);
        }

        if (mTokens.empty())
            continue;

        string op = pop_front(mTokens);

        if (op == ".patch") {
            tokenize(".");
            patch();
            continue;
        }

        if (!singleFrag) {
            if (op == ".begin") {
                if (mTokens.size() != 1)
                    bad("missing fragment name");
                if (mInFrag)
                    bad("nested fragments are not supported");

                mFragName = pop_front(mTokens);

                beginFragment();
                first = false;
                continue;
            } else if (op == ".end") {
                if (!mInFrag)
                    bad("expecting .begin before .end");
                if (!mTokens.empty())
                    bad("too many tokens");
                endFragment();
                continue;
            }
        }
        if (first) {
            first = false;
            singleFrag = true;
            mFragName = "main";

            beginFragment();
        }

        string lab;
        LIns *ins = NULL;
        extract_any_label(op, lab, ':');

        /* Save label and do any back-patching of deferred forward-jumps. */
        if (!lab.empty()) {
            ins = mLir->ins0(LIR_label);
            typedef multimap<string,LIns*> mulmap;
            typedef mulmap::const_iterator ci;
            pair<ci,ci> range = mFwdJumps.equal_range(lab);
            for (ci i = range.first; i != range.second; ++i) {
                i->second->setTarget(ins);
            }
            mFwdJumps.erase(lab);
            lab.clear();
        }
        extract_any_label(op, lab, '=');

        if (op_map.find(op) == op_map.end())
            bad("unknown instruction '" + op + "'");

        pair<LOpcode,size_t> entry = op_map[op];
        mOpcode = entry.first;
        mOpcount = entry.second;

        switch (mOpcode) {
        // A few special opcode cases.

        case LIR_j:
        case LIR_jt:
        case LIR_jf:
        case LIR_ji:
            ins = assemble_jump();
            break;

        case LIR_int:
            need(1);
            ins = mLir->insImm(imm(mTokens[0]));
            break;

        case LIR_quad:
            need(1);
            ins = mLir->insImmq(quad(mTokens[0]));
            break;

        case LIR_sti:
        case LIR_stqi:
            need(3);
            ins = mLir->insStorei(ref(mTokens[0]),
                                  ref(mTokens[1]),
                                  imm(mTokens[2]));
            break;

        case LIR_ld:
        case LIR_ldc:
        case LIR_ldq:
        case LIR_ldqc:
        case LIR_ldcb:
        case LIR_ldcs:
            ins = assemble_load();
            break;

        case LIR_iparam:
            need(2);
            ins = mLir->insParam(imm(mTokens[0]),
                                 imm(mTokens[1]));
            break;

        case LIR_ialloc:
            need(1);
            ins = mLir->insAlloc(imm(mTokens[0]));
            break;

        case LIR_skip:
            need(1);
            {
                int32_t count = imm(mTokens[0]);
                if (uint32_t(count) > NJ_MAX_SKIP_PAYLOAD_SZB)
                    bad("oversize skip");
                ins = do_skip(count);
            }
            break;

        case LIR_xt:
        case LIR_xf:
        case LIR_x:
        case LIR_xbarrier:
        case LIR_loop:
            ins = assemble_guard();
            break;

        case LIR_call:
        case LIR_callh:
        case LIR_fcall:
            ins = assemble_call(op);
            break;
        default:
            ins = assemble_general();
            break;
        }

        assert(ins);
        if (!lab.empty())
            mLabels.insert(make_pair(lab, ins));
    }
    if (mInFrag && singleFrag)
        endFragment();

    if (mInFrag)
        bad("unexpected EOF");
    if (mParent->mLirbuf->outOMem()) {
        cerr << "lirbuf out of memory" << endl;
        exit(1);
    }
}

bool
has_flag(vector<string> &args, string const &flag)
{
    for (vector<string>::iterator i = args.begin();
         i != args.end(); ++i) {
        if (*i == flag) {
            args.erase(i);
            return true;
        }
    }
    return false;
}


Lirasm::Lirasm(bool verbose)
{
    mVerbose = verbose;
    nanojit::AvmCore::config.tree_opt = true;
    mLogc.lcbits = 0;
    mFragmento = new (&gc) Fragmento(&s_core, &mLogc, 32);
    mFragmento->labels = NULL;
    mLirbuf = new (&gc) LirBuffer(mFragmento);
#ifdef DEBUG
    if (mVerbose) {
        mLogc.lcbits = LC_Assembly;
        mFragmento->labels = new (&gc) LabelMap(&s_core);
        mLirbuf->names = new (&gc) LirNameMap(&gc, mFragmento->labels);
    }
#endif
}

Lirasm::~Lirasm()
{
    Fragments::iterator i;
    for (i = mFragments.begin(); i != mFragments.end(); ++i) {
        i->second.fragptr->releaseCode(mFragmento);
        delete i->second.fragptr;
    }
    delete mLirbuf;
    delete mFragmento->labels;
    delete mFragmento;
}

LirasmAssembler::LirasmAssembler(Lirasm &lasm)
{
    mParent = &lasm;
    mInFrag = false;
    mLineno = 0;
}

void
LirasmAssembler::tokenize(string const &tok_sep)
{
    vector<string>::iterator i;
    for (i = mTokens.begin(); i < mTokens.end(); i++)
    {
        string line = *i;
        size_t start = 0;
        size_t end = 0;
        while((start = line.find_first_not_of(tok_sep, end)) != string::npos &&
              (end = line.find_first_of(tok_sep, start)) != string::npos) {
            const string ss = line.substr(start, (end-start));
            i->erase(start, end-start+1);
            mTokens.insert(i++, ss);
            mTokens.insert(i++, tok_sep);
        }
    }
}

void
LirasmAssembler::patch()
{
    if (mTokens[1] != "." || mTokens[3] != "->")
        bad("incorrect syntax");
    Fragments::iterator i;
    if ((i=mParent->mFragments.find(mTokens[0])) == mParent->mFragments.end())
        bad("invalid fragment reference");
    LirasmFragment *frag = &i->second;
    if (frag->mLabels.find(mTokens[2]) == frag->mLabels.end())
        bad("invalid guard reference");
    LIns *ins = frag->mLabels.find(mTokens[2])->second;
    if ((i=mParent->mFragments.find(mTokens[4])) == mParent->mFragments.end())
        bad("invalid guard reference");
    ins->record()->exit->target = i->second.fragptr;

    mParent->mFragmento->assm()->patch(ins->record()->exit);
}

int
main(int argc, char **argv)
{
    string prog(*argv);
    vector<string> args;
    while (--argc)
        args.push_back(string(*++argv));

#if defined NANOJIT_IA32
    avmplus::AvmCore::config.use_cmov =
        avmplus::AvmCore::config.sse2 =
        has_flag(args, "--sse");
#endif
    bool execute = has_flag(args, "--execute");
    bool verbose = has_flag(args, "-v");

#if defined NANOJIT_IA32
    if (verbose && !execute) {
        cerr << "usage: " << prog << " [--sse | --execute [-v]] <filename>" << endl;
        exit(1);
    }
#endif

    if (args.empty()) {
#if defined NANOJIT_IA32
        cerr << "usage: " << prog << " [--sse | --execute [-v]] <filename>" << endl;
#else
        cerr << "usage: " << prog << " <filename>" << endl;
#endif
        exit(1);
    }

    ifstream in(args[0].c_str());
    if (!in) {
        cerr << prog << ": error: unable to open file " << args[0] << endl;
        exit(1);
    }

    Lirasm lasm(verbose);
    LirasmAssembler(lasm).assemble(in);

    Fragments::const_iterator i;
    if (execute) {
        i = lasm.mFragments.find("main");
        if (i == lasm.mFragments.end()) {
            cerr << prog << ": error: atleast one fragment must be named main" << endl;
            exit(1);
        }
        switch (i->second.mReturnType) {
          case RT_FLOAT:
            cout << "Output is: " << i->second.rfloat() << endl;
            break;
          case RT_INT32:
            cout << "Output is: " << i->second.rint() << endl;
            break;
          case RT_GUARD:
            LasmSideExit *ls = (LasmSideExit*) i->second.rguard()->exit;
            cout << "Output is: " << ls->line << endl;
            break;
        }
    } else {
        for (i = lasm.mFragments.begin(); i != lasm.mFragments.end(); i++)
            dump_srecords(cout, i->second.fragptr);
    }
}
