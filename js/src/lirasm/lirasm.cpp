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

#ifdef AVMPLUS_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif

#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <assert.h>

#include "nanojit/nanojit.h"

using namespace nanojit;
using namespace std;

/* Allocator SPI implementation. */

void*
nanojit::Allocator::allocChunk(size_t nbytes)
{
    void *p = malloc(nbytes);
    if (!p)
        exit(1);
    return p;
}

void
nanojit::Allocator::freeChunk(void *p) {
    free(p);
}

void
nanojit::Allocator::postReset() {
}


struct LasmSideExit : public SideExit {
    size_t line;
};


/* LIR SPI implementation */

void
nanojit::StackFilter::getTops(LIns* guard, int& spTop, int& rpTop)
{
    spTop = 0;
    rpTop = 0;
}

#if defined NJ_VERBOSE
void
nanojit::LirNameMap::formatGuard(LIns *i, char *out)
{
    LasmSideExit *x;

    x = (LasmSideExit *)i->record()->exit;
    sprintf(out,
            "%s: %s %s -> line=%d (GuardID=%03d)",
            formatRef(i),
            lirNames[i->opcode()],
            i->oprnd1() ? formatRef(i->oprnd1()) : "",
            x->line,
            i->record()->profGuardID);
}
#endif

typedef FASTCALL int32_t (*RetInt)();
typedef FASTCALL double (*RetFloat)();
typedef FASTCALL GuardRecord* (*RetGuard)();

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

enum LirTokenType {
    NAME, NUMBER, PUNCT, NEWLINE
};

struct LirToken {
    LirTokenType type;
    string data;
    int lineno;
};

inline bool
startsWith(const string &s, const string &prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.length(), prefix) == 0;
}

// LIR files must be ASCII, for simplicity.
class LirTokenStream {
public:
    LirTokenStream(istream &in) : mIn(in), mLineno(0) {}

    bool get(LirToken &token) {
        if (mLine.empty()) {
            if (!getline(mIn, mLine))
                return false;
            mLine += '\n';
            mLineno++;
        }
        mLine.erase(0, mLine.find_first_not_of(" \t\v\r"));
        char c = mLine[0];
        size_t e = mLine.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_$.+-");
        if (startsWith(mLine, "->")) {
            mLine.erase(0, 2);
            token.type = PUNCT;
            token.data = "->";
        } else if (e > 0) {
            string s = mLine.substr(0, e);
            mLine.erase(0, e);
            if (e > 1 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
                token.type = NUMBER;
            else if (isdigit(s[0]) || (e > 1 && s[0] == '.' && isdigit(s[1])))
                token.type = NUMBER;
            else
                token.type = NAME;
            token.data = s;
        } else if (strchr(":,=[]()", c)) {
            token.type = PUNCT;
            token.data = c;
            mLine.erase(0, 1);
        } else if (c == ';' || c == '\n') {
            token.type = NEWLINE;
            token.data.clear();
            mLine.clear();
        } else {
            cerr << mLineno << ": error: Unrecognized character in file." << endl;
            return false;
        }

        token.lineno = mLineno;
        return true;
    }

    bool eat(LirTokenType type, const char *exact = NULL) {
        LirToken token;
        return (get(token) && token.type == type && (exact == NULL || token.data == exact));
    }

    bool getName(string &name) {
        LirToken t;
        if (get(t) && t.type == NAME) {
            name = t.data;
            return true;
        }
        return false;
    }

private:
    istream &mIn;
    string mLine;
    int mLineno;
};

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
    Lirasm(bool verbose);
    ~Lirasm();

    void assemble(istream &in);
    void lookupFunction(const string &name, CallInfo *&ci);

    LirBuffer *mLirbuf;
    verbose_only( LabelMap *mLabelMap; )
    LogControl mLogc;
    avmplus::AvmCore mCore;
    Allocator mAlloc;
    CodeAlloc mCodeAlloc;
    bool mVerbose;
    Fragments mFragments;
    Assembler mAssm;
    map<string, pair<LOpcode, size_t> > mOpMap;

    void bad(const string &msg) {
        cerr << "error: " << msg << endl;
        exit(1);
    }

private:
    void handlePatch(LirTokenStream &in);
};

class FragmentAssembler {
public:
    FragmentAssembler(Lirasm &parent, const string &fragmentName);
    ~FragmentAssembler();

    void assembleFragment(LirTokenStream &in,
                          bool implicitBegin,
                          const LirToken *firstToken);

private:
    static uint32_t sProfId;
    // Prohibit copying.
    FragmentAssembler(const FragmentAssembler &);
    FragmentAssembler & operator=(const FragmentAssembler &);
    LasmSideExit *createSideExit();
    GuardRecord *createGuardRecord(LasmSideExit *exit);

    Lirasm &mParent;
    const string mFragName;
    Fragment *mFragment;
    vector<CallInfo*> mCallInfos;
    map<string, LIns*> mLabels;
    LirWriter *mLir;
    LirBufWriter *mBufWriter;
    LirWriter *mCseFilter;
    LirWriter *mExprFilter;
    LirWriter *mVerboseWriter;
    multimap<string, LIns *> mFwdJumps;

    size_t mLineno;
    LOpcode mOpcode;
    size_t mOpcount;

    char mReturnTypeBits;
    vector<string> mTokens;

    void tokenizeLine(LirTokenStream &in, LirToken &token);
    void need(size_t);
    LIns *ref(const string &);
    LIns *assemble_call(const string &);
    LIns *assemble_general();
    LIns *assemble_guard();
    LIns *assemble_jump();
    LIns *assemble_load();
    void bad(const string &msg);
    void extract_any_label(string &lab, char lab_delim);
    void endFragment();
};

Function functions[] = {
    FN(puts, I32 | (PTRARG<<2)),
    FN(sin, F64 | (F64<<2)),
    FN(malloc, PTRRET | (PTRARG<<2)),
    FN(free, I32 | (PTRARG<<2))
};

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
imm(const string &s)
{
    stringstream tmp(s);
    int32_t ret;
    if ((s.find("0x") == 0 || s.find("0X") == 0) &&
        (tmp >> hex >> ret && tmp.eof())) {
        return ret;
    }
    return lexical_cast<int32_t>(s);
}

uint64_t
quad(const string &s)
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
    // FIXME: Disabled until we work out a sane way to walk through
    // code chunks under the new CodeAlloc regime.
/*
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
*/
}



uint32_t
FragmentAssembler::sProfId = 0;

FragmentAssembler::FragmentAssembler(Lirasm &parent, const string &fragmentName)
    : mParent(parent), mFragName(fragmentName)
{
    mFragment = new Fragment(NULL verbose_only(, sProfId++));
    mFragment->lirbuf = mParent.mLirbuf;
    mFragment->root = mFragment;
    mParent.mFragments[mFragName].fragptr = mFragment;

    mBufWriter = new LirBufWriter(mParent.mLirbuf);
    mCseFilter = new CseFilter(mBufWriter, mParent.mAlloc);
    mExprFilter = new ExprFilter(mCseFilter);
    mVerboseWriter = NULL;
    mLir = mExprFilter;

#ifdef DEBUG
    if (mParent.mVerbose) {
        mVerboseWriter = new VerboseWriter(mParent.mAlloc,
                                           mExprFilter,
                                           mParent.mLirbuf->names,
                                           &mParent.mLogc);
        mLir = mVerboseWriter;
    }
#endif

    mReturnTypeBits = 0;
    mLir->ins0(LIR_start);

    mLineno = 0;
}

FragmentAssembler::~FragmentAssembler()
{
    delete mVerboseWriter;
    delete mExprFilter;
    delete mCseFilter;
    delete mBufWriter;
}


void
FragmentAssembler::bad(const string &msg)
{
    cerr << "instruction " << mLineno << ": " <<  msg << endl;
    exit(1);
}

void
FragmentAssembler::need(size_t n)
{
    if (mTokens.size() != n) {
        bad("need " + lexical_cast<string>(n)
            + " tokens, have " + lexical_cast<string>(mTokens.size()));
    }
}

LIns *
FragmentAssembler::ref(const string &lab)
{
    if (mLabels.find(lab) == mLabels.end())
        bad("unknown label '" + lab + "'");
    return mLabels.find(lab)->second;
}

LIns *
FragmentAssembler::assemble_jump()
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

LIns *
FragmentAssembler::assemble_load()
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

LIns *
FragmentAssembler::assemble_call(const string &op)
{
    CallInfo *ci = new (mParent.mAlloc) CallInfo();
    mCallInfos.push_back(ci);
    LIns *args[MAXARGS];

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

    AbiKind _abi = ABI_CDECL;
    if (abi == "fastcall") {
#ifdef NO_FASTCALL
        bad("no fastcall support");
#endif
        _abi = ABI_FASTCALL;
    }
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
        mParent.lookupFunction(func, ci);
        if (ci == NULL)
            bad("invalid function reference " + func);
        if (_abi != ci->_abi)
            bad("invalid calling convention for " + func);
    }

    ci->_argtypes = 0;

    for (size_t i = 0; i < mTokens.size(); ++i) {
        args[i] = ref(mTokens[mTokens.size() - (i+1)]);
        ci->_argtypes |= args[i]->isQuad() ? ARGSIZE_F : ARGSIZE_LO;
        ci->_argtypes <<= ARGSIZE_SHIFT;
    }

    // Select return type from opcode.
    // FIXME: callh needs special treatment currently
    // missing from here.
    if (mOpcode == LIR_icall)
        ci->_argtypes |= ARGSIZE_LO;
    else
        ci->_argtypes |= ARGSIZE_F;

    return mLir->insCall(ci, args);
}

LasmSideExit*
FragmentAssembler::createSideExit()
{
    LasmSideExit* exit = new (mParent.mAlloc) LasmSideExit();
    memset(exit, 0, sizeof(LasmSideExit));
    exit->from = mFragment;
    exit->target = NULL;
    exit->line = mLineno;
    return exit;
}

GuardRecord*
FragmentAssembler::createGuardRecord(LasmSideExit *exit)
{
    GuardRecord *rec = new (mParent.mAlloc) GuardRecord();
    memset(rec, 0, sizeof(GuardRecord));
    rec->exit = exit;
    exit->addGuard(rec);
    return rec;
}


LIns *
FragmentAssembler::assemble_guard()
{
    GuardRecord* guard = createGuardRecord(createSideExit());

    need(mOpcount);

    mReturnTypeBits |= RT_GUARD;

    LIns *ins_cond;
    if (mOpcode == LIR_xt || mOpcode == LIR_xf)
        ins_cond = ref(pop_front(mTokens));
    else
        ins_cond = NULL;

    if (!mTokens.empty())
        bad("too many arguments");

    return mLir->insGuard(mOpcode, ins_cond, guard);
}

LIns *
FragmentAssembler::assemble_general()
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
FragmentAssembler::endFragment()
{
    if (mReturnTypeBits == 0) {
        cerr << "warning: no return type in fragment '"
             << mFragName << "'" << endl;
    }
    if (mReturnTypeBits != RT_INT32 && mReturnTypeBits != RT_FLOAT &&
        mReturnTypeBits != RT_GUARD) {
        cerr << "warning: multiple return types in fragment '"
             << mFragName << "'" << endl;
    }

    mFragment->lastIns =
        mLir->insGuard(LIR_x, NULL, createGuardRecord(createSideExit()));

    ::compile(&mParent.mAssm, mFragment, mParent.mAlloc
              verbose_only(, mParent.mLabelMap));

    if (mParent.mAssm.error() != nanojit::None) {
        cerr << "error during assembly: ";
        switch (mParent.mAssm.error()) {
          case nanojit::StackFull: cerr << "StackFull"; break;
          case nanojit::UnknownBranch: cerr << "UnknownBranch"; break;
          case nanojit::None: cerr << "None"; break;
        }
        cerr << endl;
        std::exit(1);
    }

    LirasmFragment *f;
    f = &mParent.mFragments[mFragName];

    switch (mReturnTypeBits) {
    case RT_GUARD:
        f->rguard = (RetGuard)((uintptr_t)mFragment->code());
        f->mReturnType = RT_GUARD;
        break;
    case RT_FLOAT:
        f->rfloat = (RetFloat)((uintptr_t)mFragment->code());
        f->mReturnType = RT_FLOAT;
        break;
    default:
        f->rint = (RetInt)((uintptr_t)mFragment->code());
        f->mReturnType = RT_INT32;
        break;
    }

    mParent.mFragments[mFragName].mLabels = mLabels;
}

void
FragmentAssembler::tokenizeLine(LirTokenStream &in, LirToken &token)
{
    mTokens.clear();
    mTokens.push_back(token.data);

    while (in.get(token)) {
        if (token.type == NEWLINE)
            break;
        mTokens.push_back(token.data);
    }
}

void
FragmentAssembler::extract_any_label(string &lab, char lab_delim)
{
    if (mTokens.size() > 2 && mTokens[1].size() == 1 && mTokens[1][0] == lab_delim) {
        lab = pop_front(mTokens);
        pop_front(mTokens);  // remove punctuation

        if (mLabels.find(lab) != mLabels.end())
            bad("duplicate label");
    }
}

void
FragmentAssembler::assembleFragment(LirTokenStream &in, bool implicitBegin, const LirToken *firstToken)
{
    LirToken token;
    while (true) {
        if (firstToken) {
            token = *firstToken;
            firstToken = NULL;
        } else if (!in.get(token)) {
            if (!implicitBegin)
                bad("unexpected end of file in fragment '" + mFragName + "'");
            break;
        }
        if (token.type == NEWLINE)
            continue;
        if (token.type != NAME)
            bad("unexpected token '" + token.data + "'");

        string op = token.data;
        if (op == ".begin")
            bad("nested fragments are not supported");
        if (op == ".end") {
            if (implicitBegin)
                bad(".end without .begin");
            if (!in.eat(NEWLINE))
                bad("extra junk after .end");
            break;
        }

        mLineno = token.lineno;
        tokenizeLine(in, token);

        string lab;
        LIns *ins = NULL;
        extract_any_label(lab, ':');

        /* Save label and do any back-patching of deferred forward-jumps. */
        if (!lab.empty()) {
            ins = mLir->ins0(LIR_label);
            typedef multimap<string, LIns *> mulmap;
            typedef mulmap::const_iterator ci;
            pair<ci, ci> range = mFwdJumps.equal_range(lab);
            for (ci i = range.first; i != range.second; ++i) {
                i->second->setTarget(ins);
            }
            mFwdJumps.erase(lab);
            lab.clear();
        }
        extract_any_label(lab, '=');

        assert(!mTokens.empty());
        op = pop_front(mTokens);
        if (mParent.mOpMap.find(op) == mParent.mOpMap.end())
            bad("unknown instruction '" + op + "'");

        pair<LOpcode, size_t> entry = mParent.mOpMap[op];
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
            bad("skip instruction is deprecated");
            break;

          case LIR_xt:
          case LIR_xf:
          case LIR_x:
          case LIR_xbarrier:
            ins = assemble_guard();
            break;

          case LIR_icall:
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
    endFragment();
}

Lirasm::Lirasm(bool verbose) :
    mAssm(mCodeAlloc, mAlloc, &mCore, &mLogc)
{
    mVerbose = verbose;
    nanojit::AvmCore::config.tree_opt = true;
    mLogc.lcbits = 0;

    mLirbuf = new (mAlloc) LirBuffer(mAlloc);
#ifdef DEBUG
    if (mVerbose) {
        mLogc.lcbits = LC_Assembly;
        mLabelMap = new (mAlloc) LabelMap(mAlloc, &mLogc);
        mLirbuf->names = new (mAlloc) LirNameMap(mAlloc, mLabelMap);
    }
#endif

    // Populate the mOpMap table.
#define OPDEF(op, number, args, repkind) \
    mOpMap[#op] = make_pair(LIR_##op, args);
#define OPDEF64(op, number, args, repkind) \
    mOpMap[#op] = make_pair(LIR_##op, args);
#include "nanojit/LIRopcode.tbl"
#undef OPDEF
#undef OPDEF64

    // TODO - These should alias to the appropriate platform-specific LIR opcode.
    mOpMap["alloc"] = mOpMap["ialloc"];
    mOpMap["param"] = mOpMap["iparam"];
}

Lirasm::~Lirasm()
{
    Fragments::iterator i;
    for (i = mFragments.begin(); i != mFragments.end(); ++i) {
        delete i->second.fragptr;
    }
}


void
Lirasm::lookupFunction(const string &name, CallInfo *&ci)
{
    const size_t nfuns = sizeof(functions) / sizeof(functions[0]);
    for (size_t i = 0; i < nfuns; i++) {
        if (name == functions[i].name) {
            *ci = functions[i].callInfo;
            return;
        }
    }

    Fragments::const_iterator func = mFragments.find(name);
    if (func != mFragments.end()) {
        if (func->second.mReturnType == RT_FLOAT) {
            CallInfo target = {(uintptr_t) func->second.rfloat,
                               ARGSIZE_F, 0, 0,
                               nanojit::ABI_FASTCALL
                               verbose_only(, func->first.c_str()) };
            *ci = target;

        } else {
            CallInfo target = {(uintptr_t) func->second.rint,
                               ARGSIZE_LO, 0, 0,
                               nanojit::ABI_FASTCALL
                               verbose_only(, func->first.c_str()) };
            *ci = target;
        }
    } else {
        ci = NULL;
    }
}

void
Lirasm::assemble(istream &in)
{
    LirTokenStream ts(in);
    bool first = true;

    LirToken token;
    while (ts.get(token)) {

        if (token.type == NEWLINE)
            continue;
        if (token.type != NAME)
            bad("unexpected token '" + token.data + "'");

        const string &op = token.data;
        if (op == ".patch") {
            handlePatch(ts);
        } else if (op == ".begin") {
            string name;
            if (!ts.getName(name))
                bad("expected fragment name after .begin");
            if (!ts.eat(NEWLINE))
                bad("extra junk after .begin " + name);

            FragmentAssembler assembler(*this, name);
            assembler.assembleFragment(ts, false, NULL);
            first = false;
        } else if (op == ".end") {
            bad(".end without .begin");
        } else if (first) {
            FragmentAssembler assembler(*this, "main");
            assembler.assembleFragment(ts, true, &token);
            break;
        } else {
            bad("unexpected stray opcode '" + op + "'");
        }
    }
}

void
Lirasm::handlePatch(LirTokenStream &in)
{
    string src, fragName, guardName, destName;

    if (!in.getName(src) || !in.eat(PUNCT, "->") || !in.getName(destName))
        bad("incorrect syntax");

    // Break the src at '.'. This is awkward but the syntax looks nice.
    size_t j = src.find('.');
    if (j == string::npos || j == 0 || j == src.size() - 1)
        bad("incorrect syntax");
    fragName = src.substr(0, j);
    guardName = src.substr(j + 1);

    Fragments::iterator i;
    if ((i=mFragments.find(fragName)) == mFragments.end())
        bad("invalid fragment reference");
    LirasmFragment *frag = &i->second;
    if (frag->mLabels.find(guardName) == frag->mLabels.end())
        bad("invalid guard reference");
    LIns *ins = frag->mLabels.find(guardName)->second;
    if ((i=mFragments.find(destName)) == mFragments.end())
        bad("invalid guard reference");
    ins->record()->exit->target = i->second.fragptr;

    mAssm.patch(ins->record()->exit);
}

bool
has_flag(vector<string> &args, const string &flag)
{
    for (vector<string>::iterator i = args.begin(); i != args.end(); ++i) {
        if (*i == flag) {
            args.erase(i);
            return true;
        }
    }
    return false;
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
    lasm.assemble(in);

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
