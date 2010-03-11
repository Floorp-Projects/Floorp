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

int
nanojit::StackFilter::getTop(LIns*)
{
    return 0;
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

void
nanojit::LirNameMap::formatGuardXov(LIns *i, char *out)
{
    LasmSideExit *x;

    x = (LasmSideExit *)i->record()->exit;
    sprintf(out,
            "%s = %s %s, %s -> line=%ld (GuardID=%03d)",
            formatRef(i),
            lirNames[i->opcode()],
            formatRef(i->oprnd1()),
            formatRef(i->oprnd2()),
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
    {(uintptr_t) (&name), args, nanojit::ABI_CDECL, /*isPure*/0, ACC_STORE_ANY \
     DEBUG_ONLY_NAME(name)}

#define FN(name, args) \
    {#name, CI(name, args)}

const int I32 = nanojit::ARGSIZE_LO;
#ifdef NANOJIT_64BIT
const int I64 = nanojit::ARGSIZE_Q;
#endif
const int F64 = nanojit::ARGSIZE_F;
const int PTR = nanojit::ARGSIZE_P;

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
            cerr << "line " << mLineno << ": error: Unrecognized character in file." << endl;
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

    void assemble(istream &in, bool optimize);
    void assembleRandom(int nIns, bool optimize);
    bool lookupFunction(const string &name, CallInfo *&ci);

    LirBuffer *mLirbuf;
    verbose_only( LabelMap *mLabelMap; )
    LogControl mLogc;
    avmplus::AvmCore mCore;
    Allocator mAlloc;
    CodeAlloc mCodeAlloc;
    bool mVerbose;
    Fragments mFragments;
    Assembler mAssm;
    map<string, LOpcode> mOpMap;

    void bad(const string &msg) {
        cerr << "error: " << msg << endl;
        exit(1);
    }

private:
    void handlePatch(LirTokenStream &in);
};

class FragmentAssembler {
public:
    FragmentAssembler(Lirasm &parent, const string &fragmentName, bool optimize);
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
    bool optimize;
    vector<CallInfo*> mCallInfos;
    map<string, LIns*> mLabels;
    LirWriter *mLir;
    LirBufWriter *mBufWriter;
    LirWriter *mCseFilter;
    LirWriter *mExprFilter;
    LirWriter *mSoftFloatFilter;
    LirWriter *mVerboseWriter;
    LirWriter *mValidateWriter1;
    LirWriter *mValidateWriter2;
    multimap<string, LIns *> mFwdJumps;

    size_t mLineno;
    LOpcode mOpcode;
    size_t mOpcount;

    char mReturnTypeBits;
    vector<string> mTokens;

    void tokenizeLine(LirTokenStream &in, LirToken &token);
    void need(size_t);
    LIns *ref(const string &);
    LIns *assemble_jump(bool isCond);
    LIns *assemble_load();
    LIns *assemble_call(const string &);
    LIns *assemble_ret(ReturnType rt);
    LIns *assemble_guard(bool isCond);
    LIns *assemble_guard_xov();
    void bad(const string &msg);
    void nyi(const string &opname);
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
    FN(puts,   argMask(PTR, 1, 1) | retMask(I32)),
    FN(sin,    argMask(F64, 1, 1) | retMask(F64)),
    FN(malloc, argMask(PTR, 1, 1) | retMask(PTR)),
    FN(free,   argMask(PTR, 1, 1) | retMask(I32))
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
lquad(const string &s)
{
    stringstream tmp(s);
    uint64_t ret;
    if ((s.find("0x") == 0 || s.find("0X") == 0) &&
        (tmp >> hex >> ret && tmp.eof())) {
        return ret;
    }
    return lexical_cast<uint64_t>(s);
}

double
immf(const string &s)
{
    return lexical_cast<double>(s);
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
dump_srecords(ostream &, Fragment *)
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

FragmentAssembler::FragmentAssembler(Lirasm &parent, const string &fragmentName, bool optimize)
    : mParent(parent), mFragName(fragmentName), optimize(optimize),
      mBufWriter(NULL), mCseFilter(NULL), mExprFilter(NULL), mSoftFloatFilter(NULL), mVerboseWriter(NULL),
      mValidateWriter1(NULL), mValidateWriter2(NULL)
{
    mFragment = new Fragment(NULL verbose_only(, (mParent.mLogc.lcbits &
                                                  nanojit::LC_FragProfile) ?
                                                  sProfId++ : 0));
    mFragment->lirbuf = mParent.mLirbuf;
    mParent.mFragments[mFragName].fragptr = mFragment;

    mLir = mBufWriter  = new LirBufWriter(mParent.mLirbuf, nanojit::AvmCore::config);
#ifdef DEBUG
    if (optimize) {     // don't re-validate if no optimization has taken place
        mLir = mValidateWriter2 = new ValidateWriter(mLir, "end of writer pipeline");
    }
#endif
#ifdef DEBUG
    if (mParent.mVerbose) {
        mLir = mVerboseWriter = new VerboseWriter(mParent.mAlloc, mLir,
                                                  mParent.mLirbuf->names,
                                                  &mParent.mLogc);
    }
#endif
    if (optimize) {
        mLir = mCseFilter = new CseFilter(mLir, mParent.mAlloc);
    }
#if NJ_SOFTFLOAT_SUPPORTED
    if (avmplus::AvmCore::config.soft_float) {
        mLir = new SoftFloatFilter(mLir);
    }
#endif
    if (optimize) {
        mLir = mExprFilter = new ExprFilter(mLir);
    }
#ifdef DEBUG
    mLir = mValidateWriter1 = new ValidateWriter(mLir, "start of writer pipeline");
#endif

    mReturnTypeBits = 0;
    mLir->ins0(LIR_start);
    for (int i = 0; i < nanojit::NumSavedRegs; ++i)
        mLir->insParam(i, 1);

    mLineno = 0;
}

FragmentAssembler::~FragmentAssembler()
{
    delete mValidateWriter1;
    delete mValidateWriter2;
    delete mVerboseWriter;
    delete mExprFilter;
    delete mSoftFloatFilter;
    delete mCseFilter;
    delete mBufWriter;
}


void
FragmentAssembler::bad(const string &msg)
{
    cerr << "line " << mLineno << ": " << msg << endl;
    exit(1);
}

void
FragmentAssembler::nyi(const string &opname)
{
    cerr << "line " << mLineno << ": '" << opname << "' not yet implemented, sorry" << endl;
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
FragmentAssembler::assemble_jump(bool isCond)
{
    LIns *condition;

    if (isCond) {
        need(2);
        string cond = pop_front(mTokens);
        condition = ref(cond);
    } else {
        need(1);
        condition = NULL;
    }
    string name = pop_front(mTokens);
    if (mLabels.find(name) != mLabels.end()) {
        LIns *target = ref(name);
        return mLir->insBranch(mOpcode, condition, target);
    } else {
        LIns *ins = mLir->insBranch(mOpcode, condition, NULL);
#ifdef __SUNPRO_CC
        mFwdJumps.insert(make_pair<const string, LIns *>(name, ins));
#else
        mFwdJumps.insert(make_pair(name, ins));
#endif
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
    CallInfo *ci = new (mParent.mAlloc) CallInfo;
    mCallInfos.push_back(ci);
    LIns *args[MAXARGS];
    memset(&args[0], 0, sizeof(args));

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

    if (mTokens.size() > MAXARGS)
    bad("too many args to " + op);

    bool isBuiltin = mParent.lookupFunction(func, ci);
    if (isBuiltin) {
        // Built-in:  use its CallInfo.  Also check (some) CallInfo details
        // against those from the call site.
        if (_abi != ci->_abi)
            bad("invalid calling convention for " + func);

        size_t i;
        for (i = 0; i < mTokens.size(); ++i) {
            args[i] = ref(mTokens[mTokens.size() - (i+1)]);
        }
        if (i != ci->count_args())
            bad("wrong number of arguments for " + func);

    } else {
        // User-defined function:  infer CallInfo details (ABI, arg types, ret
        // type) from the call site.
        int ty;

        ci->_abi = _abi;

        ci->_argtypes = 0;
        size_t argc = mTokens.size();
        for (size_t i = 0; i < argc; ++i) {
            args[i] = ref(mTokens[mTokens.size() - (i+1)]);
            if      (args[i]->isF64()) ty = ARGSIZE_F;
#ifdef NANOJIT_64BIT
            else if (args[i]->isI64()) ty = ARGSIZE_Q;
#endif
            else                       ty = ARGSIZE_I;
            // Nb: i+1 because argMask() uses 1-based arg counting.
            ci->_argtypes |= argMask(ty, i+1, argc);
        }

        // Select return type from opcode.
        ty = 0;
        if      (mOpcode == LIR_icall) ty = ARGSIZE_LO;
        else if (mOpcode == LIR_fcall) ty = ARGSIZE_F;
#ifdef NANOJIT_64BIT
        else if (mOpcode == LIR_qcall) ty = ARGSIZE_Q;
#endif
        else                           nyi("callh");
        ci->_argtypes |= retMask(ty);
    }

    return mLir->insCall(ci, args);
}

LIns *
FragmentAssembler::assemble_ret(ReturnType rt)
{
    need(1);
    mReturnTypeBits |= rt;
    return mLir->ins1(mOpcode, ref(mTokens[0]));
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
    GuardRecord *rec = new (mParent.mAlloc) GuardRecord;
    memset(rec, 0, sizeof(GuardRecord));
    rec->exit = exit;
    exit->addGuard(rec);
    return rec;
}


LIns *
FragmentAssembler::assemble_guard(bool isCond)
{
    GuardRecord* guard = createGuardRecord(createSideExit());

    LIns *ins_cond;
    if (isCond) {
        need(1);
        ins_cond = ref(pop_front(mTokens));
    } else {
        need(0);
        ins_cond = NULL;
    }

    mReturnTypeBits |= RT_GUARD;

    if (!mTokens.empty())
        bad("too many arguments");

    return mLir->insGuard(mOpcode, ins_cond, guard);
}

LIns*
FragmentAssembler::assemble_guard_xov()
{
    GuardRecord* guard = createGuardRecord(createSideExit());

    need(2);

    mReturnTypeBits |= RT_GUARD;

    return mLir->insGuardXov(mOpcode, ref(mTokens[0]), ref(mTokens[1]), guard);
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

    mParent.mAssm.compile(mFragment, mParent.mAlloc, optimize
              verbose_only(, mParent.mLabelMap));

    if (mParent.mAssm.error() != nanojit::None) {
        cerr << "error during assembly: ";
        switch (mParent.mAssm.error()) {
          case nanojit::ConditionalBranchTooFar: cerr << "ConditionalBranchTooFar"; break;
          case nanojit::StackFull: cerr << "StackFull"; break;
          case nanojit::UnknownBranch:  cerr << "UnknownBranch"; break;
          case nanojit::None: cerr << "None"; break;
          default: NanoAssert(0); break;
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
#ifdef __SUNPRO_CC
            typedef mulmap::iterator ci;
#else
            typedef mulmap::const_iterator ci;
#endif
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

        mOpcode = mParent.mOpMap[op];

        switch (mOpcode) {
          case LIR_start:
            bad("start instructions cannot be specified explicitly");
            break;

          case LIR_regfence:
            need(0);
            ins = mLir->ins0(mOpcode);
            break;

          case LIR_live:
          CASE64(LIR_qlive:)
          case LIR_flive:
          case LIR_neg:
          case LIR_fneg:
          case LIR_not:
          CASESF(LIR_qlo:)
          CASESF(LIR_qhi:)
          CASE64(LIR_q2i:)
          CASE64(LIR_i2q:)
          CASE64(LIR_u2q:)
          case LIR_i2f:
          case LIR_u2f:
          case LIR_f2i:
#if defined NANOJIT_IA32 || defined NANOJIT_X64
          case LIR_mod:
#endif
            need(1);
            ins = mLir->ins1(mOpcode,
                             ref(mTokens[0]));
            break;

          case LIR_addp:
          case LIR_add:
          case LIR_sub:
          case LIR_mul:
#if defined NANOJIT_IA32 || defined NANOJIT_X64
          case LIR_div:
#endif
          case LIR_fadd:
          case LIR_fsub:
          case LIR_fmul:
          case LIR_fdiv:
          CASE64(LIR_qiadd:)
          case LIR_and:
          case LIR_or:
          case LIR_xor:
          CASE64(LIR_qiand:)
          CASE64(LIR_qior:)
          CASE64(LIR_qxor:)
          case LIR_lsh:
          case LIR_rsh:
          case LIR_ush:
          CASE64(LIR_qilsh:)
          CASE64(LIR_qirsh:)
          CASE64(LIR_qursh:)
          case LIR_eq:
          case LIR_lt:
          case LIR_gt:
          case LIR_le:
          case LIR_ge:
          case LIR_ult:
          case LIR_ugt:
          case LIR_ule:
          case LIR_uge:
          case LIR_feq:
          case LIR_flt:
          case LIR_fgt:
          case LIR_fle:
          case LIR_fge:
          CASE64(LIR_qeq:)
          CASE64(LIR_qlt:)
          CASE64(LIR_qgt:)
          CASE64(LIR_qle:)
          CASE64(LIR_qge:)
          CASE64(LIR_qult:)
          CASE64(LIR_qugt:)
          CASE64(LIR_qule:)
          CASE64(LIR_quge:)
          CASESF(LIR_qjoin:)
            need(2);
            ins = mLir->ins2(mOpcode,
                             ref(mTokens[0]),
                             ref(mTokens[1]));
            break;

          case LIR_cmov:
          CASE64(LIR_qcmov:)
            need(3);
            ins = mLir->ins3(mOpcode,
                             ref(mTokens[0]),
                             ref(mTokens[1]),
                             ref(mTokens[2]));
            break;

          case LIR_j:
            ins = assemble_jump(/*isCond*/false);
            break;

          case LIR_jt:
          case LIR_jf:
            ins = assemble_jump(/*isCond*/true);
            break;

          case LIR_int:
            need(1);
            ins = mLir->insImm(imm(mTokens[0]));
            break;

#ifdef NANOJIT_64BIT
          case LIR_quad:
            need(1);
            ins = mLir->insImmq(lquad(mTokens[0]));
            break;
#endif

          case LIR_float:
            need(1);
            ins = mLir->insImmf(immf(mTokens[0]));
            break;

#if NJ_EXPANDED_LOADSTORE_SUPPORTED 
          case LIR_stb:
          case LIR_sts:
          case LIR_st32f:
#endif
          case LIR_sti:
          CASE64(LIR_stqi:)
          case LIR_stfi:
            need(3);
            ins = mLir->insStore(mOpcode, ref(mTokens[0]),
                                  ref(mTokens[1]),
                                  imm(mTokens[2]));
            break;

#if NJ_EXPANDED_LOADSTORE_SUPPORTED 
          case LIR_ldsb:
          case LIR_ldss:
          case LIR_ld32f:
#endif
          case LIR_ldzb:
          case LIR_ldzs:
          case LIR_ld:
          CASE64(LIR_ldq:)
          case LIR_ldf:
            ins = assemble_load();
            break;

          // XXX: insParam gives the one appropriate for the platform.  Eg. if
          // you specify qparam on x86 you'll end up with iparam anyway.  Fix
          // this.
          case LIR_param:
            need(2);
            ins = mLir->insParam(imm(mTokens[0]),
                                 imm(mTokens[1]));
            break;

          // XXX: similar to iparam/qparam above.
          case LIR_alloc:
            need(1);
            ins = mLir->insAlloc(imm(mTokens[0]));
            break;

          case LIR_skip:
            bad("skip instruction is deprecated");
            break;

          case LIR_x:
          case LIR_xbarrier:
            ins = assemble_guard(/*isCond*/false);
            break;

          case LIR_xt:
          case LIR_xf:
            ins = assemble_guard(/*isCond*/true);
            break;

          case LIR_addxov:
          case LIR_subxov:
          case LIR_mulxov:
            ins = assemble_guard_xov();
            break;

          case LIR_icall:
          CASESF(LIR_callh:)
          case LIR_fcall:
          CASE64(LIR_qcall:)
            ins = assemble_call(op);
            break;

          case LIR_ret:
            ins = assemble_ret(RT_INT32);
            break;

          case LIR_fret:
            ins = assemble_ret(RT_FLOAT);
            break;

          case LIR_label:
          case LIR_file:
          case LIR_line:
          case LIR_xtbl:
          case LIR_jtbl:
          CASE64(LIR_qret:)
            nyi(op);
            break;

          default:
            nyi(op);
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
// run out of stack space due to spilling.  Needs to be set in consideration
// with spillStackSzB.
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

// Returns a 4-aligned address within the given size.
static int32_t rndOffset32(size_t szB)
{
    return int32_t(rnd(szB)) & ~3;
}

// Returns an 8-aligned address within the give size.
static int32_t rndOffset64(size_t szB)
{
    return int32_t(rnd(szB)) & ~7;
}

static int32_t f_I_I1(int32_t a)
{
    return a;
}

static int32_t f_I_I6(int32_t a, int32_t b, int32_t c, int32_t d, int32_t e, int32_t f)
{
    return a + b + c + d + e + f;
}

#ifdef NANOJIT_64BIT
static uint64_t f_Q_Q2(uint64_t a, uint64_t b)
{
    return a + b;
}

static uint64_t f_Q_Q7(uint64_t a, uint64_t b, uint64_t c, uint64_t d,
                       uint64_t e, uint64_t f, uint64_t g)
{
    return a + b + c + d + e + f + g;
}
#endif

static double f_F_F3(double a, double b, double c)
{
    return a + b + c;
}

static double f_F_F8(double a, double b, double c, double d,
                     double e, double f, double g, double h)
{
    return a + b + c + d + e + f + g + h;
}

#ifdef NANOJIT_64BIT
static void f_N_IQF(int32_t, uint64_t, double)
{
    return;     // no need to do anything
}
#endif

const CallInfo ci_I_I1 = CI(f_I_I1, argMask(I32, 1, 1) |
                                    retMask(I32));

const CallInfo ci_I_I6 = CI(f_I_I6, argMask(I32, 1, 6) |
                                    argMask(I32, 2, 6) |
                                    argMask(I32, 3, 6) |
                                    argMask(I32, 4, 6) |
                                    argMask(I32, 5, 6) |
                                    argMask(I32, 6, 6) |
                                    retMask(I32));

#ifdef NANOJIT_64BIT
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
#endif

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

#ifdef NANOJIT_64BIT
const CallInfo ci_N_IQF = CI(f_N_IQF, argMask(I32, 1, 3) |
                                      argMask(I64, 2, 3) |
                                      argMask(F64, 3, 3) |
                                      retMask(ARGSIZE_NONE));
#endif

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
// - LIR_iparam/LIR_qparam (hard to test beyond what is auto-generated in fragment
//   prologues)
// - LIR_live/LIR_qlive/LIR_flive
// - LIR_callh
// - LIR_x/LIR_xt/LIR_xf/LIR_xtbl/LIR_addxov/LIR_subxov/LIR_mulxov (hard to
//   test without having multiple fragments;  when we only have one fragment
//   we don't really want to leave it early)
// - LIR_ret/LIR_qret/LIR_fret (hard to test without having multiple fragments)
// - LIR_j/LIR_jt/LIR_jf/LIR_jtbl/LIR_label
// - LIR_file/LIR_line (#ifdef VTUNE only)
// - LIR_fmod (not implemented in NJ backends)
//
// Other limitations:
// - Loads always use accSet==ACC_LOAD_ANY
// - Stores always use accSet==ACC_STORE_ANY
//
void
FragmentAssembler::assembleRandomFragment(int nIns)
{
    vector<LIns*> Bs;       // boolean values, ie. 32-bit int values produced by tests
    vector<LIns*> Is;       // 32-bit int values
    vector<LIns*> Qs;       // 64-bit int values
    vector<LIns*> Fs;       // 64-bit float values
    vector<LIns*> M4s;      // 4 byte allocs
    vector<LIns*> M8ps;     // 8+ byte allocs

    vector<LOpcode> I_I_ops;
    I_I_ops.push_back(LIR_neg);
    I_I_ops.push_back(LIR_not);

    // Nb: there are no Q_Q_ops.

    vector<LOpcode> F_F_ops;
    F_F_ops.push_back(LIR_fneg);

    vector<LOpcode> I_II_ops;
    I_II_ops.push_back(LIR_add);
#ifndef NANOJIT_64BIT
    I_II_ops.push_back(LIR_iaddp);
#endif
    I_II_ops.push_back(LIR_sub);
    I_II_ops.push_back(LIR_mul);
#if defined NANOJIT_IA32 || defined NANOJIT_X64
    I_II_ops.push_back(LIR_div);
    I_II_ops.push_back(LIR_mod);
#endif
    I_II_ops.push_back(LIR_and);
    I_II_ops.push_back(LIR_or);
    I_II_ops.push_back(LIR_xor);
    I_II_ops.push_back(LIR_lsh);
    I_II_ops.push_back(LIR_rsh);
    I_II_ops.push_back(LIR_ush);

#ifdef NANOJIT_64BIT
    vector<LOpcode> Q_QQ_ops;
    Q_QQ_ops.push_back(LIR_qiadd);
    Q_QQ_ops.push_back(LIR_qaddp);
    Q_QQ_ops.push_back(LIR_qiand);
    Q_QQ_ops.push_back(LIR_qior);
    Q_QQ_ops.push_back(LIR_qxor);

    vector<LOpcode> Q_QI_ops;
    Q_QI_ops.push_back(LIR_qilsh);
    Q_QI_ops.push_back(LIR_qirsh);
    Q_QI_ops.push_back(LIR_qursh);
#endif

    vector<LOpcode> F_FF_ops;
    F_FF_ops.push_back(LIR_fadd);
    F_FF_ops.push_back(LIR_fsub);
    F_FF_ops.push_back(LIR_fmul);
    F_FF_ops.push_back(LIR_fdiv);

    vector<LOpcode> I_BII_ops;
    I_BII_ops.push_back(LIR_cmov);

#ifdef NANOJIT_64BIT
    vector<LOpcode> Q_BQQ_ops;
    Q_BQQ_ops.push_back(LIR_qcmov);
#endif

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

#ifdef NANOJIT_64BIT
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
#endif

    vector<LOpcode> B_FF_ops;
    B_FF_ops.push_back(LIR_feq);
    B_FF_ops.push_back(LIR_flt);
    B_FF_ops.push_back(LIR_fgt);
    B_FF_ops.push_back(LIR_fle);
    B_FF_ops.push_back(LIR_fge);

#ifdef NANOJIT_64BIT
    vector<LOpcode> Q_I_ops;
    Q_I_ops.push_back(LIR_i2q);
    Q_I_ops.push_back(LIR_u2q);

    vector<LOpcode> I_Q_ops;
    I_Q_ops.push_back(LIR_q2i);
#endif

    vector<LOpcode> F_I_ops;
    F_I_ops.push_back(LIR_i2f);
    F_I_ops.push_back(LIR_u2f);

    vector<LOpcode> I_F_ops;
#if NJ_SOFTFLOAT_SUPPORTED
    I_F_ops.push_back(LIR_qlo);
    I_F_ops.push_back(LIR_qhi);
#endif
    I_F_ops.push_back(LIR_f2i);

    vector<LOpcode> F_II_ops;
#if NJ_SOFTFLOAT_SUPPORTED
    F_II_ops.push_back(LIR_qjoin);
#endif

    vector<LOpcode> I_loads;
    I_loads.push_back(LIR_ld);          // weight LIR_ld more heavily
    I_loads.push_back(LIR_ld);
    I_loads.push_back(LIR_ld);
    I_loads.push_back(LIR_ldzb);
    I_loads.push_back(LIR_ldzs);
#if NJ_EXPANDED_LOADSTORE_SUPPORTED 
    I_loads.push_back(LIR_ldsb);
    I_loads.push_back(LIR_ldss);
#endif

#ifdef NANOJIT_64BIT
    vector<LOpcode> Q_loads;
    Q_loads.push_back(LIR_ldq);
#endif

    vector<LOpcode> F_loads;
    F_loads.push_back(LIR_ldf);
#if NJ_EXPANDED_LOADSTORE_SUPPORTED
    // this loads a 32-bit float and expands it to 64-bit float
    F_loads.push_back(LIR_ld32f);
#endif

    enum LInsClass {
#define CL___(name, relFreq)     name,
#include "LInsClasses.tbl"
#undef CL___
        LLAST
    };

    int relFreqs[LLAST];
    memset(relFreqs, 0, sizeof(relFreqs));
#define CL___(name, relFreq)     relFreqs[name] = relFreq;
#include "LInsClasses.tbl"
#undef CL___

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

    // Used to keep track of how much stack we've explicitly used via
    // LIR_alloc.  We then need to keep some reserve for spills as well.
    const size_t stackSzB = NJ_MAX_STACK_ENTRY * 4;
    const size_t spillStackSzB = 1024;
    const size_t maxExplicitlyUsedStackSzB = stackSzB - spillStackSzB;
    size_t explicitlyUsedStackSzB = 0;

    // Do an 8-byte stack alloc right at the start so that loads and stores
    // can be done immediately.
    addOrReplace(M8ps, mLir->insAlloc(8));

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

        case LALLOC: {
            // The stack has a limited size, so we (a) don't want chunks to be
            // too big, and (b) have to stop allocating them after a while.
            size_t szB = 0;
            switch (rnd(3)) {
            case 0: szB = 4;                break;
            case 1: szB = 8;                break;
            case 2: szB = 4 * (rnd(6) + 3); break;  // 12, 16, ..., 32
            }
            if (explicitlyUsedStackSzB + szB <= maxExplicitlyUsedStackSzB) {
                ins = mLir->insAlloc(szB);
                // We add the result to Is/Qs so it can be used as an ordinary
                // operand, and to M4s/M8ps so that loads/stores can be done from
                // it.
#if defined NANOJIT_64BIT
                addOrReplace(Qs, ins);
#else
                addOrReplace(Is, ins);
#endif
                if (szB == 4)
                    addOrReplace(M4s, ins);
                else
                    addOrReplace(M8ps, ins);

                // It's possible that we will exceed maxExplicitlyUsedStackSzB
                // by up to 28 bytes.  Doesn't matter.
                explicitlyUsedStackSzB += szB;
                n++;
            }
            break;
        }

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

#ifdef NANOJIT_64BIT
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
#endif

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
#if defined NANOJIT_IA32 || defined NANOJIT_X64
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
                        LIns* rhs2 = mLir->ins3(LIR_cmov, gt0, rhs, mLir->insImm(-((int32_t)rnd(99)) - 2));
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
                } else
#endif
                {
                    ins = mLir->ins2(op, lhs, rhs);
                    addOrReplace(Is, ins);
                    n++;
                }
            }
            break;

#ifdef NANOJIT_64BIT
        case LOP_Q_QQ:
            if (!Qs.empty()) {
                ins = mLir->ins2(rndPick(Q_QQ_ops), rndPick(Qs), rndPick(Qs));
                addOrReplace(Qs, ins);
                n++;
            }
            break;

        case LOP_Q_QI:
            if (!Qs.empty() && !Is.empty()) {
                ins = mLir->ins2(rndPick(Q_QI_ops), rndPick(Qs), rndPick(Is));
                addOrReplace(Qs, ins);
                n++;
            }
            break;
#endif

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

#ifdef NANOJIT_64BIT
        case LOP_Q_BQQ:
            if (!Bs.empty() && !Qs.empty()) {
                ins = mLir->ins3(rndPick(Q_BQQ_ops), rndPick(Bs), rndPick(Qs), rndPick(Qs));
                addOrReplace(Qs, ins);
                n++;
            }
            break;
#endif

        case LOP_B_II:
           if (!Is.empty()) {
               ins = mLir->ins2(rndPick(B_II_ops), rndPick(Is), rndPick(Is));
               addOrReplace(Bs, ins);
               n++;
           }
            break;

#ifdef NANOJIT_64BIT
        case LOP_B_QQ:
            if (!Qs.empty()) {
                ins = mLir->ins2(rndPick(B_QQ_ops), rndPick(Qs), rndPick(Qs));
                addOrReplace(Bs, ins);
                n++;
            }
            break;
#endif

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

#ifdef NANOJIT_64BIT
        case LOP_Q_I:
            if (!Is.empty()) {
                ins = mLir->ins1(rndPick(Q_I_ops), rndPick(Is));
                addOrReplace(Qs, ins);
                n++;
            }
            break;
#endif

        case LOP_F_I:
            if (!Is.empty()) {
                ins = mLir->ins1(rndPick(F_I_ops), rndPick(Is));
                addOrReplace(Fs, ins);
                n++;
            }
            break;

#ifdef NANOJIT_64BIT
        case LOP_I_Q:
            if (!Qs.empty()) {
                ins = mLir->ins1(rndPick(I_Q_ops), rndPick(Qs));
                addOrReplace(Is, ins);
                n++;
            }
            break;
#endif

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
            if (!Is.empty() && !F_II_ops.empty()) {
                ins = mLir->ins2(rndPick(F_II_ops), rndPick(Is), rndPick(Is));
                addOrReplace(Fs, ins);
                n++;
            }
            break;

        case LLD_I: {
            vector<LIns*> Ms = rnd(2) ? M4s : M8ps;
            if (!Ms.empty()) {
                LIns* base = rndPick(Ms);
                ins = mLir->insLoad(rndPick(I_loads), base, rndOffset32(base->size()));
                addOrReplace(Is, ins);
                n++;
            }
            break;
        }

#ifdef NANOJIT_64BIT
        case LLD_Q:
            if (!M8ps.empty()) {
                LIns* base = rndPick(M8ps);
                ins = mLir->insLoad(rndPick(Q_loads), base, rndOffset64(base->size()));
                addOrReplace(Qs, ins);
                n++;
            }
            break;
#endif

        case LLD_F:
            if (!M8ps.empty()) {
                LIns* base = rndPick(M8ps);
                ins = mLir->insLoad(rndPick(F_loads), base, rndOffset64(base->size()));
                addOrReplace(Fs, ins);
                n++;
            }
            break;

        case LST_I: {
            vector<LIns*> Ms = rnd(2) ? M4s : M8ps;
            if (!Ms.empty() && !Is.empty()) {
                LIns* base = rndPick(Ms);
                mLir->insStorei(rndPick(Is), base, rndOffset32(base->size()));
                n++;
            }
            break;
        }

#ifdef NANOJIT_64BIT
        case LST_Q:
            if (!M8ps.empty() && !Qs.empty()) {
                LIns* base = rndPick(M8ps);
                mLir->insStorei(rndPick(Qs), base, rndOffset64(base->size()));
                n++;
            }
            break;
#endif

        case LST_F:
            if (!M8ps.empty() && !Fs.empty()) {
                LIns* base = rndPick(M8ps);
                mLir->insStorei(rndPick(Fs), base, rndOffset64(base->size()));
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

#ifdef NANOJIT_64BIT
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
#endif

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

#ifdef NANOJIT_64BIT
        case LCALL_N_IQF:
            if (!Is.empty() && !Qs.empty() && !Fs.empty()) {
                // Nb: args[] holds the args in reverse order... sigh.
                LIns* args[3] = { rndPick(Fs), rndPick(Qs), rndPick(Is) };
                ins = mLir->insCall(&ci_N_IQF, args);
                n++;
            }
            break;
#endif

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

    // Return 0.
    mReturnTypeBits |= RT_INT32;
    mLir->ins1(LIR_ret, mLir->insImm(0));

    endFragment();
}

Lirasm::Lirasm(bool verbose) :
    mAssm(mCodeAlloc, mAlloc, mAlloc, &mCore, &mLogc, nanojit::AvmCore::config)
{
    mVerbose = verbose;
    mLogc.lcbits = 0;

    mLirbuf = new (mAlloc) LirBuffer(mAlloc);
#ifdef DEBUG
    if (mVerbose) {
        mLogc.lcbits = LC_ReadLIR | LC_Assembly | LC_RegAlloc | LC_Activation;
        mLabelMap = new (mAlloc) LabelMap(mAlloc, &mLogc);
        mLirbuf->names = new (mAlloc) LirNameMap(mAlloc, mLabelMap);
    }
#endif

    // Populate the mOpMap table.
#define OP___(op, number, repKind, retType, isCse) \
    mOpMap[#op] = LIR_##op;
#include "nanojit/LIRopcode.tbl"
#undef OP___

    mOpMap["alloc"] = mOpMap[PTR_SIZE("ialloc", "qalloc")];
    mOpMap["param"] = mOpMap[PTR_SIZE("iparam", "qparam")];
}

Lirasm::~Lirasm()
{
    Fragments::iterator i;
    for (i = mFragments.begin(); i != mFragments.end(); ++i) {
        delete i->second.fragptr;
    }
}


bool
Lirasm::lookupFunction(const string &name, CallInfo *&ci)
{
    const size_t nfuns = sizeof(functions) / sizeof(functions[0]);
    for (size_t i = 0; i < nfuns; i++) {
        if (name == functions[i].name) {
            *ci = functions[i].callInfo;
            return true;
        }
    }

    Fragments::const_iterator func = mFragments.find(name);
    if (func != mFragments.end()) {
        // The ABI, arg types and ret type will be overridden by the caller.
        if (func->second.mReturnType == RT_FLOAT) {
            CallInfo target = {(uintptr_t) func->second.rfloat,
                               0, ABI_FASTCALL, /*isPure*/0, ACC_STORE_ANY
                               verbose_only(, func->first.c_str()) };
            *ci = target;

        } else {
            CallInfo target = {(uintptr_t) func->second.rint,
                               0, ABI_FASTCALL, /*isPure*/0, ACC_STORE_ANY
                               verbose_only(, func->first.c_str()) };
            *ci = target;
        }
        return false;

    } else {
        bad("invalid function reference " + name);
        return false;
    }
}

void
Lirasm::assemble(istream &in, bool optimize)
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

            FragmentAssembler assembler(*this, name, optimize);
            assembler.assembleFragment(ts, false, NULL);
            first = false;
        } else if (op == ".end") {
            bad(".end without .begin");
        } else if (first) {
            FragmentAssembler assembler(*this, "main", optimize);
            assembler.assembleFragment(ts, true, &token);
            break;
        } else {
            bad("unexpected stray opcode '" + op + "'");
        }
    }
}

void
Lirasm::assembleRandom(int nIns, bool optimize)
{
    string name = "main";
    FragmentAssembler assembler(*this, name, optimize);
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
        "  --[no-]optimize  enable or disable optimization of the LIR (default=off)\n"
        "  --random [N]     generate a random LIR block of size N (default=1000)\n"
        " i386-specific options:\n"
        "  --sse            use SSE2 instructions\n"
        " ARM-specific options:\n"
        "  --arch N         generate code for ARM architecture version N (default=7)\n"
        "  --[no]vfp        enable or disable the generation of ARM VFP code (default=on)\n"
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
    bool    optimize;
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
    opts.optimize = false;

    // Architecture-specific options.
#if defined NANOJIT_IA32
    bool            i386_sse = false;
#elif defined NANOJIT_ARM
    unsigned int    arm_arch = 7;
    bool            arm_vfp = true;
#endif

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];

        // Common flags for every architecture.
        if (arg == "-h" || arg == "--help")
            usageAndQuit(opts.progname);
        else if (arg == "-v" || arg == "--verbose")
            opts.verbose = true;
        else if (arg == "--execute")
            opts.execute = true;
        else if (arg == "--optimize")
            opts.optimize = true;
        else if (arg == "--no-optimize")
            opts.optimize = false;
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

        // Architecture-specific flags.
#if defined NANOJIT_IA32
        else if (arg == "--sse") {
            i386_sse = true;
        }
#elif defined NANOJIT_ARM
        else if ((arg == "--arch") && (i < argc-1)) {
            char* endptr;
            arm_arch = strtoul(argv[i+1], &endptr, 10);
            // Check that the argument was a number.
            if ('\0' == *endptr) {
                if ((arm_arch < 5) || (arm_arch > 7)) {
                    errMsgAndQuit(opts.progname, "Unsupported argument to --arm-arch.\n");
                }
            } else {
                errMsgAndQuit(opts.progname, "Unrecognized argument to --arm-arch.\n");
            }
            i++;
        } else if (arg == "--vfp") {
            arm_vfp = true;
        } else if (arg == "--novfp") {
            arm_vfp = false;
        }
#endif
        // Input file names.
        else if (arg[0] != '-') {
            if (opts.filename.empty())
                opts.filename = arg;
            else
                errMsgAndQuit(opts.progname, "you can only specify one filename");
        }
        // No matching flag found, so report the error.
        else
            errMsgAndQuit(opts.progname, "bad option: " + arg);
    }

    if ((!opts.random && opts.filename.empty()) || (opts.random && !opts.filename.empty()))
        errMsgAndQuit(opts.progname,
                      "you must specify either a filename or --random (but not both)");

    // Handle the architecture-specific options.
#if defined NANOJIT_IA32
    avmplus::AvmCore::config.i386_use_cmov = avmplus::AvmCore::config.i386_sse2 = i386_sse;
    avmplus::AvmCore::config.i386_fixed_esp = true;
#elif defined NANOJIT_ARM
    // Note that we don't check for sensible configurations here!
    avmplus::AvmCore::config.arm_arch = arm_arch;
    avmplus::AvmCore::config.arm_vfp = arm_vfp;
    avmplus::AvmCore::config.soft_float = !arm_vfp;
#endif
}

int
main(int argc, char **argv)
{
    CmdLineOptions opts;
    processCmdLine(argc, argv, opts);

    Lirasm lasm(opts.verbose);
    if (opts.random) {
        lasm.assembleRandom(opts.random, opts.optimize);
    } else {
        ifstream in(opts.filename.c_str());
        if (!in)
            errMsgAndQuit(opts.progname, "unable to open file " + opts.filename);
        lasm.assemble(in, opts.optimize);
    }

    Fragments::const_iterator i;
    if (opts.execute) {
        i = lasm.mFragments.find("main");
        if (i == lasm.mFragments.end())
            errMsgAndQuit(opts.progname, "error: at least one fragment must be named 'main'");
        switch (i->second.mReturnType) {
          case RT_FLOAT:
          {
            double res = i->second.rfloat();
            cout << "Output is: " << res << endl;
            break;
          }
          case RT_INT32:
          {
            int res = i->second.rint();
            cout << "Output is: " << res << endl;
            break;
          }
          case RT_GUARD:
          {
            LasmSideExit *ls = (LasmSideExit*) i->second.rguard()->exit;
            cout << "Exited block on line: " << ls->line << endl;
            break;
          }
        }
    } else {
        for (i = lasm.mFragments.begin(); i != lasm.mFragments.end(); i++)
            dump_srecords(cout, i->second.fragptr);
    }
}
