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
            "%s: %s %s -> line=%ld (GuardID=%03d)",
            formatRef(i),
            lirNames[i->opcode()],
            i->oprnd1() ? formatRef(i->oprnd1()) : "",
            (long)x->line,
            i->record()->profGuardID);
}
#endif

typedef int32_t (FASTCALL *RetInt)();
typedef double (FASTCALL *RetFloat)();
typedef GuardRecord* (FASTCALL *RetGuard)();

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

#define CI(name, args) \
    {(uintptr_t) (&name), args, /*_cse*/0, /*_fold*/0, nanojit::ABI_CDECL \
     DEBUG_ONLY_NAME(name)}

#define FN(name, args) \
    {#name, CI(name, args)}

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
    void assembleRandom(int nIns);
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

    void assembleRandomFragment(int nIns);

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

// Meaning: arg 'm' of 'n' has size 'sz'.
static int argMask(int sz, int m, int n)
{
    // Order examples, from MSB to LSB:  
    // - 3 args: 000 | 000 | 000 | 000 | 000 | arg1| arg2| arg3| ret
    // - 8 args: arg1| arg2| arg3| arg4| arg5| arg6| arg7| arg8| ret
    // If the mask encoding reversed the arg order the 'n' parameter wouldn't
    // be necessary, as argN would always be in the same place in the
    // bitfield.
    return sz << ((1 + n - m) * ARGSIZE_SHIFT);
}

// Return value has size 'sz'.
static int retMask(int sz)
{
    return sz;
}

// 'sin' is overloaded on some platforms, so taking its address
// doesn't quite work. Provide a do-nothing function here
// that's not overloaded.
double sinFn(double d) {
    return sin(d);
}
#define sin sinFn

Function functions[] = {
    FN(puts,   argMask(PTRARG, 1, 1) | retMask(I32)),
    FN(sin,    argMask(F64,    1, 1) | retMask(F64)),
    FN(malloc, argMask(PTRARG, 1, 1) | retMask(PTRRET)),
    FN(free,   argMask(PTRARG, 1, 1) | retMask(I32))
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
    : mParent(parent), mFragName(fragmentName),
      mBufWriter(NULL), mCseFilter(NULL), mExprFilter(NULL), mVerboseWriter(NULL)
{
    mFragment = new Fragment(NULL verbose_only(, (mParent.mLogc.lcbits &
                                                  nanojit::LC_FragProfile) ?
                                                  sProfId++ : 0));
    mFragment->lirbuf = mParent.mLirbuf;
    mFragment->root = mFragment;
    mParent.mFragments[mFragName].fragptr = mFragment;

    mLir = mBufWriter  = new LirBufWriter(mParent.mLirbuf);
    mLir = mCseFilter  = new CseFilter(mLir, mParent.mAlloc);
    mLir = mExprFilter = new ExprFilter(mLir);

#ifdef DEBUG
    if (mParent.mVerbose) {
        mLir = mVerboseWriter = new VerboseWriter(mParent.mAlloc, mLir,
                                                  mParent.mLirbuf->names,
                                                  &mParent.mLogc);
    }
#endif

    mReturnTypeBits = 0;
    mLir->ins0(LIR_start);
    for (int i = 0; i < nanojit::NumSavedRegs; ++i)
        mLir->insParam(i, 1);

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

/* ------------------ Support for --random -------------------------- */

// Returns a positive integer in the range 0..(lim-1).
static inline size_t
rnd(size_t lim)
{
    size_t i = size_t(rand());
    return i % lim;
}

// Returns an int32_t in the range -RAND_MAX..RAND_MAX.
static inline int32_t
rndI32()
{
    return (rnd(2) ? 1 : -1) * rand();
}

// The maximum number of live values (per type, ie. B/I/Q/F) that are
// available to be used as operands.  If we make it too high we're prone to
// run out of stack space due to spilling.  If the stack size increases (see
// bug 473769) this situation will improve.
const size_t maxLiveValuesPerType = 20;

// Returns a uint32_t in the range 0..(RAND_MAX*2).
static inline uint32_t
rndU32()
{
    return uint32_t(rnd(2) ? 0 : RAND_MAX) + uint32_t(rand());
}

template<typename t> t
rndPick(vector<t> &v)
{
    assert(!v.empty());
    return v[rnd(v.size())];
}

// Add the operand, and retire an old one if we have too many.
template<typename t> void
addOrReplace(vector<t> &v, t x)
{
    if (v.size() > maxLiveValuesPerType) {
        v[rnd(v.size())] = x;    // we're full:  overwrite an existing element
    } else {
        v.push_back(x);             // add to end
    }
}

// Returns a 4-aligned address within the scratch space.
static int32_t rndOffset32(size_t scratchSzB)
{
    return int32_t(rnd(scratchSzB)) & ~3;
}

// Returns an 8-aligned address within the scratch space.
static int32_t rndOffset64(size_t scratchSzB)
{
    return int32_t(rnd(scratchSzB)) & ~7;
}

static int32_t f_I_I1(int32_t a)
{
    return a;
}

static int32_t f_I_I6(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    return a + b + c + d + e + f;
}

static uint64_t f_Q_Q2(uint64_t a, uint64_t b)
{
    return a + b;
}

static uint64_t f_Q_Q7(uint64_t a, uint64_t b, uint64_t c, uint64_t d,
                       uint64_t e, uint64_t f, uint64_t g)
{
    return a + b + c + d + e + f + g;
}

static double f_F_F3(double a, double b, double c)
{
    return a + b + c;
}

static double f_F_F8(double a, double b, double c, double d,
                     double e, double f, double g, double h)
{
    return a + b + c + d + e + f + g + h;
}

static void f_N_IQF(int32_t a, uint64_t b, double c)
{
    return;     // no need to do anything
}

const CallInfo ci_I_I1 = CI(f_I_I1, argMask(I32, 1, 1) |
                                    retMask(I32));

const CallInfo ci_I_I6 = CI(f_I_I6, argMask(I32, 1, 6) |
                                    argMask(I32, 2, 6) |
                                    argMask(I32, 3, 6) |
                                    argMask(I32, 4, 6) |
                                    argMask(I32, 5, 6) |
                                    argMask(I32, 6, 6) |
                                    retMask(I32));

const CallInfo ci_Q_Q2 = CI(f_Q_Q2, argMask(I64, 1, 2) |
                                    argMask(I64, 2, 2) |
                                    retMask(I64));

const CallInfo ci_Q_Q7 = CI(f_Q_Q7, argMask(I64, 1, 7) |
                                    argMask(I64, 2, 7) |
                                    argMask(I64, 3, 7) |
                                    argMask(I64, 4, 7) |
                                    argMask(I64, 5, 7) |
                                    argMask(I64, 6, 7) |
                                    argMask(I64, 7, 7) |
                                    retMask(I64));

const CallInfo ci_F_F3 = CI(f_F_F3, argMask(F64, 1, 3) |
                                    argMask(F64, 2, 3) |
                                    argMask(F64, 3, 3) |
                                    retMask(F64));

const CallInfo ci_F_F8 = CI(f_F_F8, argMask(F64, 1, 8) |
                                    argMask(F64, 2, 8) |
                                    argMask(F64, 3, 8) |
                                    argMask(F64, 4, 8) |
                                    argMask(F64, 5, 8) |
                                    argMask(F64, 6, 8) |
                                    argMask(F64, 7, 8) |
                                    argMask(F64, 8, 8) |
                                    retMask(F64));

const CallInfo ci_N_IQF = CI(f_N_IQF, argMask(I32, 1, 3) |
                                      argMask(I64, 2, 3) |
                                      argMask(F64, 3, 3) |
                                      retMask(ARGSIZE_NONE));

// Generate a random block containing nIns instructions, plus a few more
// setup/shutdown ones at the start and end.
//
// Basic operation:
// - We divide LIR into numerous classes, mostly according to their type.
//   (See LInsClasses.tbl for details.) Each time around the loop we choose
//   the class randomly, but there is weighting so that some classes are more
//   common than others, in an attempt to reflect the structure of real code.
// - Each instruction that produces a value is put in a buffer of the
//   appropriate type, for possible use as an operand of a later instruction.
//   This buffer is trimmed when its size exceeds 'maxLiveValuesPerType'.
// - If not enough operands are present in a buffer for the particular
//   instruction, we don't add it.
// - Skips aren't explicitly generated, but they do occcur if the fragment is
//   sufficiently big that it's spread across multiple chunks.
//
// The following instructions aren't generated yet:
// - iparam/qparam (hard to test beyond what is auto-generated in fragment
//   prologues)
// - ialloc/qalloc (except for the load/store scratch space;  hard to do so
//   long as the stack is only 1024 bytes, see bug 473769)
// - live/flive
// - callh
// - x/xt/xf/xtbl (hard to test without having multiple fragments;  when we
//   only have one fragment we don't really want to leave it early)
// - ret/fret (hard to test without having multiple fragments)
// - j/jt/jf/ji/label (ji is not implemented in NJ)
// - ov (takes an arithmetic (int or FP) value as operand, and must
//   immediately follow it to be safe... not that that really matters in
//   randomly generated code)
// - file/line (#ifdef VTUNE only)
// - fmod (not implemented in NJ)
//
void
FragmentAssembler::assembleRandomFragment(int nIns)
{
    vector<LIns*> Bs;
    vector<LIns*> Is;
    vector<LIns*> Qs;
    vector<LIns*> Fs;

    vector<LOpcode> I_I_ops;
    I_I_ops.push_back(LIR_neg);
    I_I_ops.push_back(LIR_not);

    // Nb: there are no Q_Q_ops.

    vector<LOpcode> F_F_ops;
    F_F_ops.push_back(LIR_fneg);

    vector<LOpcode> I_II_ops;
    I_II_ops.push_back(LIR_add);
    I_II_ops.push_back(LIR_iaddp);
    I_II_ops.push_back(LIR_sub);
    I_II_ops.push_back(LIR_mul);
    I_II_ops.push_back(LIR_div);
#if defined NANOJIT_IA32 || defined NANOJIT_X64
    I_II_ops.push_back(LIR_mod);
#endif
    I_II_ops.push_back(LIR_and);
    I_II_ops.push_back(LIR_or);
    I_II_ops.push_back(LIR_xor);
    I_II_ops.push_back(LIR_lsh);
    I_II_ops.push_back(LIR_rsh);
    I_II_ops.push_back(LIR_ush);

    vector<LOpcode> Q_QQ_ops;
    Q_QQ_ops.push_back(LIR_qiadd);
    Q_QQ_ops.push_back(LIR_qaddp);
    Q_QQ_ops.push_back(LIR_qiand);
    Q_QQ_ops.push_back(LIR_qior);
    Q_QQ_ops.push_back(LIR_qxor);
    Q_QQ_ops.push_back(LIR_qilsh);
    Q_QQ_ops.push_back(LIR_qirsh);
    Q_QQ_ops.push_back(LIR_qursh);

    vector<LOpcode> F_FF_ops;
    F_FF_ops.push_back(LIR_fadd);
    F_FF_ops.push_back(LIR_fsub);
    F_FF_ops.push_back(LIR_fmul);
    F_FF_ops.push_back(LIR_fdiv);

    vector<LOpcode> I_BII_ops;
    I_BII_ops.push_back(LIR_cmov);

    vector<LOpcode> Q_BQQ_ops;
    Q_BQQ_ops.push_back(LIR_qcmov);

    vector<LOpcode> B_II_ops;
    B_II_ops.push_back(LIR_eq);
    B_II_ops.push_back(LIR_lt);
    B_II_ops.push_back(LIR_gt);
    B_II_ops.push_back(LIR_le);
    B_II_ops.push_back(LIR_ge);
    B_II_ops.push_back(LIR_ult);
    B_II_ops.push_back(LIR_ugt);
    B_II_ops.push_back(LIR_ule);
    B_II_ops.push_back(LIR_uge);

    vector<LOpcode> B_QQ_ops;
    B_QQ_ops.push_back(LIR_qeq);
    B_QQ_ops.push_back(LIR_qlt);
    B_QQ_ops.push_back(LIR_qgt);
    B_QQ_ops.push_back(LIR_qle);
    B_QQ_ops.push_back(LIR_qge);
    B_QQ_ops.push_back(LIR_qult);
    B_QQ_ops.push_back(LIR_qugt);
    B_QQ_ops.push_back(LIR_qule);
    B_QQ_ops.push_back(LIR_quge);

    vector<LOpcode> B_FF_ops;
    B_FF_ops.push_back(LIR_feq);
    B_FF_ops.push_back(LIR_flt);
    B_FF_ops.push_back(LIR_fgt);
    B_FF_ops.push_back(LIR_fle);
    B_FF_ops.push_back(LIR_fge);

    vector<LOpcode> Q_I_ops;
    Q_I_ops.push_back(LIR_i2q);
    Q_I_ops.push_back(LIR_u2q);

    vector<LOpcode> F_I_ops;
    F_I_ops.push_back(LIR_i2f);
    F_I_ops.push_back(LIR_u2f);

    vector<LOpcode> I_F_ops;
    I_F_ops.push_back(LIR_qlo);
    I_F_ops.push_back(LIR_qhi);

    vector<LOpcode> F_II_ops;
    F_II_ops.push_back(LIR_qjoin);

    vector<LOpcode> I_loads;
    I_loads.push_back(LIR_ld);          // weight LIR_ld the heaviest
    I_loads.push_back(LIR_ld);
    I_loads.push_back(LIR_ld);
    I_loads.push_back(LIR_ldc);
    I_loads.push_back(LIR_ldcb);
    I_loads.push_back(LIR_ldcs);

    vector<LOpcode> QorF_loads;
    QorF_loads.push_back(LIR_ldq);      // weight LIR_ldq the heaviest
    QorF_loads.push_back(LIR_ldq);
    QorF_loads.push_back(LIR_ldqc);

    enum LInsClass {
#define CLASS(name, only64bit, relFreq)     name,
#include "LInsClasses.tbl"
#undef CLASS
        LLAST
    };

    int relFreqs[LLAST];
    memset(relFreqs, 0, sizeof(relFreqs));
#if defined NANOJIT_64BIT
#define CLASS(name, only64bit, relFreq)     relFreqs[name] = relFreq;
#else
#define CLASS(name, only64bit, relFreq)     relFreqs[name] = only64bit ? 0 : relFreq;
#endif
#include "LInsClasses.tbl"
#undef CLASS

    int relFreqsSum = 0;    // the sum of the individual relative frequencies
    for (int c = 0; c < LLAST; c++) {
        relFreqsSum += relFreqs[c];
    }

    // The number of times each LInsClass value appears in classGenerator[]
    // matches 'relFreqs' (see LInsClasses.tbl).  Eg. if relFreqs[LIMM_I] ==
    // 10, then LIMM_I appears in classGenerator[] 10 times.
    LInsClass* classGenerator = new LInsClass[relFreqsSum];
    int j = 0;
    for (int c = 0; c < LLAST; c++) {
        for (int i = 0; i < relFreqs[c]; i++) {
            classGenerator[j++] = LInsClass(c);
        }
    }

    // An area on the stack in which we do our loads and stores.
    // NJ_MAX_STACK_ENTRY entries has a size of NJ_MAX_STACK_ENTRY*4 bytes, so
    // we use a quarter of the maximum stack size.
    const size_t scratchSzB = NJ_MAX_STACK_ENTRY;
    LIns *scratch = mLir->insAlloc(scratchSzB);

    int n = 0;
    while (n < nIns) {

        LIns *ins;

        switch (classGenerator[rnd(relFreqsSum)]) {

        case LFENCE:
            if (rnd(2)) {
                mLir->ins0(LIR_regfence);
            } else {
                mLir->insGuard(LIR_xbarrier, NULL, createGuardRecord(createSideExit()));
            }
            n++;
            break;

        // For the immediates, we bias towards smaller numbers, especially 0
        // and 1 and small multiples of 4 which are common due to memory
        // addressing.  This puts some realistic stress on CseFilter.
        case LIMM_I: {
            int32_t imm32 = 0;      // shut gcc up
            switch (rnd(5)) {
            case 0: imm32 = 0;                  break;
            case 1: imm32 = 1;                  break;
            case 2: imm32 = 4 * (rnd(256) + 1); break;  // 4, 8, ..., 1024
            case 3: imm32 = rnd(19999) - 9999;  break;  // -9999..9999
            case 4: imm32 = rndI32();           break;  // -RAND_MAX..RAND_MAX
            }
            ins = mLir->insImm(imm32);
            addOrReplace(Is, ins);
            n++;
            break;
        }

        case LIMM_Q: {
            uint64_t imm64 = 0;
            switch (rnd(5)) {
            case 0: imm64 = 0;                                      break;
            case 1: imm64 = 1;                                      break;
            case 2: imm64 = 4 * (rnd(256) + 1);                     break;  // 4, 8, ..., 1024
            case 3: imm64 = rnd(19999) - 9999;                      break;  // -9999..9999
            case 4: imm64 = uint64_t(rndU32()) << 32 | rndU32();    break;  // possibly big!
            }
            ins = mLir->insImmq(imm64);
            addOrReplace(Qs, ins);
            n++;
            break;
        }

        case LIMM_F: {
            // We don't explicitly generate infinities and NaNs here, but they
            // end up occurring due to ExprFilter evaluating expressions like
            // fdiv(1,0) and fdiv(Infinity,Infinity).
            double imm64f = 0;
            switch (rnd(5)) {
            case 0: imm64f = 0.0;                                           break;
            case 1: imm64f = 1.0;                                           break;
            case 2:
            case 3: imm64f = double(rnd(1000));                             break;  // 0.0..9999.0
            case 4:
                union {
                    double d;
                    uint64_t q;
                } u;
                u.q = uint64_t(rndU32()) << 32 | rndU32();
                imm64f = u.d;
                break;
            }
            ins = mLir->insImmf(imm64f);
            addOrReplace(Fs, ins);
            n++;
            break;
        }

        case LOP_I_I:
            if (!Is.empty()) {
                ins = mLir->ins1(rndPick(I_I_ops), rndPick(Is));
                addOrReplace(Is, ins);
                n++;
            }
            break;

        // case LOP_Q_Q:  no instruction in this category

        case LOP_F_F:
            if (!Fs.empty()) {
                ins = mLir->ins1(rndPick(F_F_ops), rndPick(Fs));
                addOrReplace(Fs, ins);
                n++;
            }
            break;

        case LOP_I_II:
            if (!Is.empty()) {
                LOpcode op = rndPick(I_II_ops);
                LIns* lhs = rndPick(Is);
                LIns* rhs = rndPick(Is);
                if (op == LIR_div || op == LIR_mod) {
                    // XXX: ExprFilter can't fold a div/mod with constant
                    // args, due to the horrible semantics of LIR_mod.  So we
                    // just don't generate anything if we hit that case.
                    if (!lhs->isconst() || !rhs->isconst()) {
                        // If the divisor is positive, no problems.  If it's zero, we get an
                        // exception.  If it's -1 and the dividend is -2147483648 (-2^31) we get
                        // an exception (and this has been encountered in practice).  So we only
                        // allow positive divisors, ie. compute:  lhs / (rhs > 0 ? rhs : -k),
                        // where k is a random number in the range 2..100 (this ensures we have
                        // some negative divisors).
                        LIns* gt0  = mLir->ins2i(LIR_gt, rhs, 0);
                        LIns* rhs2 = mLir->ins3(LIR_cmov, gt0, rhs, mLir->insImm(-rnd(99) - 2));
                        LIns* div  = mLir->ins2(LIR_div, lhs, rhs2);
                        if (op == LIR_div) {
                            ins = div;
                            addOrReplace(Is, ins);
                            n += 5;
                        } else {
                            ins = mLir->ins1(LIR_mod, div);
                            // Add 'div' to the operands too so it might be used again, because
                            // the code generated is different as compared to the case where 'div'
                            // isn't used again.
                            addOrReplace(Is, div);
                            addOrReplace(Is, ins);
                            n += 6;
                        }
                    }
                } else {
                    ins = mLir->ins2(op, lhs, rhs);
                    addOrReplace(Is, ins);
                    n++;
                }
            }
            break;

        case LOP_Q_QQ:
            if (!Qs.empty()) {
                ins = mLir->ins2(rndPick(Q_QQ_ops), rndPick(Qs), rndPick(Qs));
                addOrReplace(Qs, ins);
                n++;
            }
            break;

        case LOP_F_FF:
            if (!Fs.empty()) {
                ins = mLir->ins2(rndPick(F_FF_ops), rndPick(Fs), rndPick(Fs));
                addOrReplace(Fs, ins);
                n++;
            }
            break;

        case LOP_I_BII:
            if (!Bs.empty() && !Is.empty()) {
                ins = mLir->ins3(rndPick(I_BII_ops), rndPick(Bs), rndPick(Is), rndPick(Is));
                addOrReplace(Is, ins);
                n++;
            }
            break;

        case LOP_Q_BQQ:
            if (!Bs.empty() && !Qs.empty()) {
                ins = mLir->ins3(rndPick(Q_BQQ_ops), rndPick(Bs), rndPick(Qs), rndPick(Qs));
                addOrReplace(Qs, ins);
                n++;
            }
            break;

        case LOP_B_II:
           if (!Is.empty()) {
               ins = mLir->ins2(rndPick(B_II_ops), rndPick(Is), rndPick(Is));
               addOrReplace(Bs, ins);
               n++;
           }
            break;

        case LOP_B_QQ:
            if (!Qs.empty()) {
                ins = mLir->ins2(rndPick(B_QQ_ops), rndPick(Qs), rndPick(Qs));
                addOrReplace(Bs, ins);
                n++;
            }
            break;

        case LOP_B_FF:
            if (!Fs.empty()) {
                ins = mLir->ins2(rndPick(B_FF_ops), rndPick(Fs), rndPick(Fs));
                // XXX: we don't push the result, because most (all?) of the
                // backends currently can't handle cmovs/qcmovs that take
                // float comparisons for the test (see bug 520944).  This means
                // that all B_FF values are dead, unfortunately.
                //addOrReplace(Bs, ins);
                n++;
            }
            break;

        case LOP_Q_I:
            if (!Is.empty()) {
                ins = mLir->ins1(rndPick(Q_I_ops), rndPick(Is));
                addOrReplace(Qs, ins);
                n++;
            }
            break;

        case LOP_F_I:
            if (!Is.empty()) {
                ins = mLir->ins1(rndPick(F_I_ops), rndPick(Is));
                addOrReplace(Fs, ins);
                n++;
            }
            break;

        case LOP_I_F:
// XXX: NativeX64 doesn't implement qhi yet (and it may not need to).
#if !defined NANOJIT_X64
            if (!Fs.empty()) {
                ins = mLir->ins1(rndPick(I_F_ops), rndPick(Fs));
                addOrReplace(Is, ins);
                n++;
            }
#endif
            break;

        case LOP_F_II:
// XXX: NativeX64 doesn't implement qhi yet (and it may not need to).
#if !defined NANOJIT_X64
            if (!Is.empty()) {
                ins = mLir->ins2(rndPick(F_II_ops), rndPick(Is), rndPick(Is));
                addOrReplace(Fs, ins);
                n++;
            }
#endif
            break;

        case LLD_I:
            ins = mLir->insLoad(rndPick(I_loads), scratch, rndOffset32(scratchSzB));
            addOrReplace(Is, ins);
            n++;
            break;

        case LLD_QorF:
            ins = mLir->insLoad(rndPick(QorF_loads), scratch, rndOffset64(scratchSzB));
            addOrReplace((rnd(2) ? Qs : Fs), ins);
            n++;
            break;

        case LST_I:
            if (!Is.empty()) {
                mLir->insStorei(rndPick(Is), scratch, rndOffset32(scratchSzB));
                n++;
            }
            break;

        case LST_QorF:
            if (!Fs.empty()) {
                mLir->insStorei(rndPick(Fs), scratch, rndOffset64(scratchSzB));
                n++;
            }
            break;

        case LCALL_I_I1:
            if (!Is.empty()) {
                LIns* args[1] = { rndPick(Is) };
                ins = mLir->insCall(&ci_I_I1, args);
                addOrReplace(Is, ins);
                n++;
            }
            break;

        case LCALL_I_I6:
            if (!Is.empty()) {
                LIns* args[6] = { rndPick(Is), rndPick(Is), rndPick(Is),
                                  rndPick(Is), rndPick(Is), rndPick(Is) };
                ins = mLir->insCall(&ci_I_I6, args);
                addOrReplace(Is, ins);
                n++;
            }
            break;

        case LCALL_Q_Q2:
            if (!Qs.empty()) {
                LIns* args[2] = { rndPick(Qs), rndPick(Qs) };
                ins = mLir->insCall(&ci_Q_Q2, args);
                addOrReplace(Qs, ins);
                n++;
            }
            break;

        case LCALL_Q_Q7:
            if (!Qs.empty()) {
                LIns* args[7] = { rndPick(Qs), rndPick(Qs), rndPick(Qs), rndPick(Qs),
                                  rndPick(Qs), rndPick(Qs), rndPick(Qs) };
                ins = mLir->insCall(&ci_Q_Q7, args);
                addOrReplace(Qs, ins);
                n++;
            }
            break;

        case LCALL_F_F3:
            if (!Fs.empty()) {
                LIns* args[3] = { rndPick(Fs), rndPick(Fs), rndPick(Fs) };
                ins = mLir->insCall(&ci_F_F3, args);
                addOrReplace(Fs, ins);
                n++;
            }
            break;

        case LCALL_F_F8:
            if (!Fs.empty()) {
                LIns* args[8] = { rndPick(Fs), rndPick(Fs), rndPick(Fs), rndPick(Fs),
                                  rndPick(Fs), rndPick(Fs), rndPick(Fs), rndPick(Fs) };
                ins = mLir->insCall(&ci_F_F8, args);
                addOrReplace(Fs, ins);
                n++;
            }
            break;

        case LCALL_N_IQF:
            if (!Is.empty() && !Qs.empty() && !Fs.empty()) {
                // Nb: args[] holds the args in reverse order... sigh.
                LIns* args[3] = { rndPick(Fs), rndPick(Qs), rndPick(Is) };
                ins = mLir->insCall(&ci_N_IQF, args);
                n++;
            }
            break;

        case LLABEL:
            // Although no jumps are generated yet, labels are important
            // because they delimit areas where CSE can be applied.  Without
            // them, CSE can be applied over very long regions, which leads to
            // values that have very large live ranges, which leads to stack
            // overflows.
            mLir->ins0(LIR_label);
            n++;
            break;

        default:
            NanoAssert(0);
            break;
        }
    }

    delete[] classGenerator;

    // End with a vanilla exit.
    mReturnTypeBits |= RT_GUARD;
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
        mLogc.lcbits = LC_Assembly | LC_RegAlloc | LC_Activation;
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
Lirasm::assembleRandom(int nIns)
{
    string name = "main";
    FragmentAssembler assembler(*this, name);
    assembler.assembleRandomFragment(nIns);
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

void
usageAndQuit(const string& progname)
{
    cout <<
        "usage: " << progname << " [options] [filename]\n"
        "Options:\n"
        "  -h --help        print this message\n"
        "  -v --verbose     print LIR and assembly code\n"
        "  --execute        execute LIR\n"
        "  --random [N]     generate a random LIR block of size N (default=100)\n"
        "  --sse            use SSE2 instructions (x86 only)\n"
        ;
    exit(0);
}

void
errMsgAndQuit(const string& progname, const string& msg)
{
    cerr << progname << ": " << msg << endl;
    exit(1);
}

struct CmdLineOptions {
    string  progname;
    bool    verbose;
    bool    execute;
    int     random;
    string  filename;
};

static void
processCmdLine(int argc, char **argv, CmdLineOptions& opts)
{
    opts.progname = argv[0];
    opts.verbose  = false;
    opts.execute  = false;
    opts.random   = 0;
    bool sse      = false;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];

        if (arg == "-h" || arg == "--help")
            usageAndQuit(opts.progname);
        else if (arg == "-v" || arg == "--verbose")
            opts.verbose = true;
        else if (arg == "--execute")
            opts.execute = true;
        else if (arg == "--random") {
            const int defaultSize = 100;
            if (i == argc - 1) {
                opts.random = defaultSize;      // no numeric argument, use default
            } else {
                char* endptr;
                int res = strtol(argv[i+1], &endptr, 10);
                if ('\0' == *endptr) {
                    // We don't bother checking for overflow.
                    if (res <= 0)
                        errMsgAndQuit(opts.progname, "--random argument must be greater than zero");
                    opts.random = res;          // next arg is a number, use that for the size
                    i++;
                } else {
                    opts.random = defaultSize;  // next arg is not a number
                }
            }
        }
        else if (arg == "--sse") {
            sse = true;
        }
        else if (arg[0] != '-') {
            if (opts.filename.empty())
                opts.filename = arg;
            else
                errMsgAndQuit(opts.progname, "you can only specify one filename");
        }
        else
            errMsgAndQuit(opts.progname, "bad option: " + arg);
    }

    if ((!opts.random && opts.filename.empty()) || (opts.random && !opts.filename.empty()))
        errMsgAndQuit(opts.progname,
                      "you must specify either a filename or --random (but not both)");

#if defined NANOJIT_ARM
    avmplus::AvmCore::config.arch = 7;
    avmplus::AvmCore::config.vfp = true;
#endif

#if defined NANOJIT_IA32
    avmplus::AvmCore::config.use_cmov = avmplus::AvmCore::config.sse2 = sse;
    avmplus::AvmCore::config.fixed_esp = true;
#else
    if (sse)
        errMsgAndQuit(opts.progname, "--sse is only allowed on x86");
#endif
}

int
main(int argc, char **argv)
{
    CmdLineOptions opts;
    processCmdLine(argc, argv, opts);

    Lirasm lasm(opts.verbose);
    if (opts.random) {
        lasm.assembleRandom(opts.random);
    } else {
        ifstream in(opts.filename.c_str());
        if (!in)
            errMsgAndQuit(opts.progname, "unable to open file " + opts.filename);
        lasm.assemble(in);
    }

    Fragments::const_iterator i;
    if (opts.execute) {
        i = lasm.mFragments.find("main");
        if (i == lasm.mFragments.end())
            errMsgAndQuit(opts.progname, "error: at least one fragment must be named 'main'");
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
