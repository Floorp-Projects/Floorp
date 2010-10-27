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

// We lump everything into a single access region for lirasm.
static const AccSet ACCSET_OTHER = (1 << 0);
static const uint8_t LIRASM_NUM_USED_ACCS = 1;

#if defined NJ_VERBOSE
void
nanojit::LInsPrinter::formatGuard(InsBuf *buf, LIns *ins)
{
    RefBuf b1, b2;
    LasmSideExit *x = (LasmSideExit *)ins->record()->exit;
    VMPI_snprintf(buf->buf, buf->len,
            "%s: %s %s -> line=%ld (GuardID=%03d)",
            formatRef(&b1, ins),
            lirNames[ins->opcode()],
            ins->oprnd1() ? formatRef(&b2, ins->oprnd1()) : "",
            (long)x->line,
            ins->record()->profGuardID);
}

void
nanojit::LInsPrinter::formatGuardXov(InsBuf *buf, LIns *ins)
{
    RefBuf b1, b2, b3;
    LasmSideExit *x = (LasmSideExit *)ins->record()->exit;
    VMPI_snprintf(buf->buf, buf->len,
            "%s = %s %s, %s -> line=%ld (GuardID=%03d)",
            formatRef(&b1, ins),
            lirNames[ins->opcode()],
            formatRef(&b2, ins->oprnd1()),
            formatRef(&b3, ins->oprnd2()),
            (long)x->line,
            ins->record()->profGuardID);
}

const char*
nanojit::LInsPrinter::accNames[] = {
    "o",    // (1 << 0) == ACCSET_OTHER
    "?", "?", "?", "?", "?", "?", "?", "?", "?", "?",   //  1..10 (unused)
    "?", "?", "?", "?", "?", "?", "?", "?", "?", "?",   // 11..20 (unused)
    "?", "?", "?", "?", "?", "?", "?", "?", "?", "?",   // 21..30 (unused)
    "?"                                                 //     31 (unused)
};
#endif

#ifdef DEBUG
void ValidateWriter::checkAccSet(LOpcode op, LIns* base, int32_t disp, AccSet accSet)
{
    (void)op;
    (void)base;
    (void)disp;
    NanoAssert(accSet == ACCSET_OTHER);
}
#endif

typedef int32_t (FASTCALL *RetInt)();
typedef int64_t (FASTCALL *RetQuad)();
typedef double (FASTCALL *RetDouble)();
typedef GuardRecord* (FASTCALL *RetGuard)();

struct Function {
    const char *name;
    struct nanojit::CallInfo callInfo;
};

enum ReturnType {
    RT_INT = 1,
#ifdef NANOJIT_64BIT
    RT_QUAD = 2,
#endif
    RT_DOUBLE = 4,
    RT_GUARD = 8
};

#ifdef DEBUG
#define DEBUG_ONLY_NAME(name)   ,#name
#else
#define DEBUG_ONLY_NAME(name)
#endif

#define CI(name, args) \
    {(uintptr_t) (&name), args, nanojit::ABI_CDECL, /*isPure*/0, ACCSET_STORE_ANY \
     DEBUG_ONLY_NAME(name)}

#define FN(name, args) \
    {#name, CI(name, args)}

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
        RetInt rint;
#ifdef NANOJIT_64BIT
        RetQuad rquad;
#endif
        RetDouble rdouble;
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
    LIns *assemble_jump_jov();
    void bad(const string &msg);
    void nyi(const string &opname);
    void extract_any_label(string &lab, char lab_delim);
    void resolve_forward_jumps(string &lab, LIns *ins);
    void endFragment();
};

// 'sin' is overloaded on some platforms, so taking its address
// doesn't quite work. Provide a do-nothing function here
// that's not overloaded.
double sinFn(double d) {
    return sin(d);
}
#define sin sinFn

double calld1(double x, double i, double y, double l, double x1, double i1, double y1, double l1) { 
    return x + i * y - l + x1 / i1 - y1 * l1; 
}

// The calling tests with mixed argument types are sensible for all platforms, but they highlight
// the differences between the supported ABIs on ARM.

double callid1(int i, double x, double y, int j, int k, double z) {
    return (x + y + z) / (double)(i + j + k);
}

double callid2(int i, int j, int k, double x) {
    return x / (double)(i + j + k);
}

double callid3(int i, int j, double x, int k, double y, double z) {
    return (x + y + z) / (double)(i + j + k);
}

// Simple print function for testing void calls.
void printi(int x) {
    cout << x << endl;
}

Function functions[] = {
    FN(puts,    CallInfo::typeSig1(ARGTYPE_I, ARGTYPE_P)),
    FN(sin,     CallInfo::typeSig1(ARGTYPE_D, ARGTYPE_D)),
    FN(malloc,  CallInfo::typeSig1(ARGTYPE_P, ARGTYPE_P)),
    FN(free,    CallInfo::typeSig1(ARGTYPE_V, ARGTYPE_P)),
    FN(calld1,  CallInfo::typeSig8(ARGTYPE_D, ARGTYPE_D, ARGTYPE_D, ARGTYPE_D,
                                   ARGTYPE_D, ARGTYPE_D, ARGTYPE_D, ARGTYPE_D, ARGTYPE_D)),
    FN(callid1, CallInfo::typeSig6(ARGTYPE_D, ARGTYPE_I, ARGTYPE_D, ARGTYPE_D,
                                   ARGTYPE_I, ARGTYPE_I, ARGTYPE_D)),
    FN(callid2, CallInfo::typeSig4(ARGTYPE_D, ARGTYPE_I, ARGTYPE_I, ARGTYPE_I, ARGTYPE_D)),
    FN(callid3, CallInfo::typeSig6(ARGTYPE_D, ARGTYPE_I, ARGTYPE_I, ARGTYPE_D,
                                   ARGTYPE_I, ARGTYPE_D, ARGTYPE_D)),
    FN(printi,  CallInfo::typeSig1(ARGTYPE_V, ARGTYPE_I)),
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
immI(const string &s)
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
immQ(const string &s)
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
immD(const string &s)
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
        mLir = mValidateWriter2 =
            new ValidateWriter(mLir, mFragment->lirbuf->printer, "end of writer pipeline");
    }
#endif
#ifdef DEBUG
    if (mParent.mVerbose) {
        mLir = mVerboseWriter = new VerboseWriter(mParent.mAlloc, mLir,
                                                  mParent.mLirbuf->printer,
                                                  &mParent.mLogc);
    }
#endif
    if (optimize) {
        mLir = mCseFilter = new CseFilter(mLir, LIRASM_NUM_USED_ACCS, mParent.mAlloc);
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
    mLir = mValidateWriter1 =
            new ValidateWriter(mLir, mFragment->lirbuf->printer, "start of writer pipeline");
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
                             immI(mTokens[1]), ACCSET_OTHER);
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
        ci->_abi = _abi;
        size_t argc = mTokens.size();
        ArgType argTypes[MAXARGS];
        for (size_t i = 0; i < argc; ++i) {
            NanoAssert(i < MAXARGS);    // should give a useful error msg if this fails
            args[i] = ref(mTokens[mTokens.size() - (i+1)]);
            if      (args[i]->isD()) argTypes[i] = ARGTYPE_D;
#ifdef NANOJIT_64BIT
            else if (args[i]->isQ()) argTypes[i] = ARGTYPE_Q;
#endif
            else                     argTypes[i] = ARGTYPE_I;
        }

        // Select return type from opcode.
        ArgType retType = ARGTYPE_P;
        if      (mOpcode == LIR_callv) retType = ARGTYPE_V;
        else if (mOpcode == LIR_calli) retType = ARGTYPE_I;
#ifdef NANOJIT_64BIT
        else if (mOpcode == LIR_callq) retType = ARGTYPE_Q;
#endif
        else if (mOpcode == LIR_calld) retType = ARGTYPE_D;
        else                           nyi("callh");
        ci->_typesig = CallInfo::typeSigN(retType, argc, argTypes);
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

LIns *
FragmentAssembler::assemble_jump_jov()
{
    need(3);

    LIns *a = ref(mTokens[0]);
    LIns *b = ref(mTokens[1]);
    string name = mTokens[2];

    if (mLabels.find(name) != mLabels.end()) {
        LIns *target = ref(name);
        return mLir->insBranchJov(mOpcode, a, b, target);
    } else {
        LIns *ins = mLir->insBranchJov(mOpcode, a, b, NULL);
#ifdef __SUNPRO_CC
        mFwdJumps.insert(make_pair<const string, LIns *>(name, ins));
#else
        mFwdJumps.insert(make_pair(name, ins));
#endif
        return ins;
    }
}

void
FragmentAssembler::endFragment()
{
    if (mReturnTypeBits == 0) {
        cerr << "warning: no return type in fragment '"
             << mFragName << "'" << endl;

    } else if (mReturnTypeBits != RT_INT && 
#ifdef NANOJIT_64BIT
               mReturnTypeBits != RT_QUAD &&
#endif
               mReturnTypeBits != RT_DOUBLE &&
               mReturnTypeBits != RT_GUARD)
    {
        cerr << "warning: multiple return types in fragment '"
             << mFragName << "'" << endl;
    }

    mFragment->lastIns =
        mLir->insGuard(LIR_x, NULL, createGuardRecord(createSideExit()));

    mParent.mAssm.compile(mFragment, mParent.mAlloc, optimize
              verbose_only(, mParent.mLirbuf->printer));

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
    case RT_INT:
        f->rint = (RetInt)((uintptr_t)mFragment->code());
        f->mReturnType = RT_INT;
        break;
#ifdef NANOJIT_64BIT
    case RT_QUAD:
        f->rquad = (RetQuad)((uintptr_t)mFragment->code());
        f->mReturnType = RT_QUAD;
        break;
#endif
    case RT_DOUBLE:
        f->rdouble = (RetDouble)((uintptr_t)mFragment->code());
        f->mReturnType = RT_DOUBLE;
        break;
    case RT_GUARD:
        f->rguard = (RetGuard)((uintptr_t)mFragment->code());
        f->mReturnType = RT_GUARD;
        break;
    default:
        NanoAssert(0);
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
FragmentAssembler::resolve_forward_jumps(string &lab, LIns *ins)
{
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
            resolve_forward_jumps(lab, ins);
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

          case LIR_livei:
          CASE64(LIR_liveq:)
          case LIR_lived:
          case LIR_negi:
          case LIR_negd:
          case LIR_noti:
          CASESF(LIR_dlo2i:)
          CASESF(LIR_dhi2i:)
          CASE64(LIR_q2i:)
          CASE64(LIR_i2q:)
          CASE64(LIR_ui2uq:)
          CASE64(LIR_dasq:)
          CASE64(LIR_qasd:)
          case LIR_i2d:
          case LIR_ui2d:
          case LIR_d2i:
#if defined NANOJIT_IA32 || defined NANOJIT_X64
          case LIR_modi:
#endif
            need(1);
            ins = mLir->ins1(mOpcode,
                             ref(mTokens[0]));
            break;

          case LIR_addi:
          case LIR_subi:
          case LIR_muli:
#if defined NANOJIT_IA32 || defined NANOJIT_X64
          case LIR_divi:
#endif
          case LIR_addd:
          case LIR_subd:
          case LIR_muld:
          case LIR_divd:
          CASE64(LIR_addq:)
          CASE64(LIR_subq:)
          case LIR_andi:
          case LIR_ori:
          case LIR_xori:
          CASE64(LIR_andq:)
          CASE64(LIR_orq:)
          CASE64(LIR_xorq:)
          case LIR_lshi:
          case LIR_rshi:
          case LIR_rshui:
          CASE64(LIR_lshq:)
          CASE64(LIR_rshq:)
          CASE64(LIR_rshuq:)
          case LIR_eqi:
          case LIR_lti:
          case LIR_gti:
          case LIR_lei:
          case LIR_gei:
          case LIR_ltui:
          case LIR_gtui:
          case LIR_leui:
          case LIR_geui:
          case LIR_eqd:
          case LIR_ltd:
          case LIR_gtd:
          case LIR_led:
          case LIR_ged:
          CASE64(LIR_eqq:)
          CASE64(LIR_ltq:)
          CASE64(LIR_gtq:)
          CASE64(LIR_leq:)
          CASE64(LIR_geq:)
          CASE64(LIR_ltuq:)
          CASE64(LIR_gtuq:)
          CASE64(LIR_leuq:)
          CASE64(LIR_geuq:)
          CASESF(LIR_ii2d:)
            need(2);
            ins = mLir->ins2(mOpcode,
                             ref(mTokens[0]),
                             ref(mTokens[1]));
            break;

          case LIR_cmovi:
          CASE64(LIR_cmovq:)
          case LIR_cmovd:
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

          case LIR_immi:
            need(1);
            ins = mLir->insImmI(immI(mTokens[0]));
            break;

#ifdef NANOJIT_64BIT
          case LIR_immq:
            need(1);
            ins = mLir->insImmQ(immQ(mTokens[0]));
            break;
#endif

          case LIR_immd:
            need(1);
            ins = mLir->insImmD(immD(mTokens[0]));
            break;

#if NJ_EXPANDED_LOADSTORE_SUPPORTED 
          case LIR_sti2c:
          case LIR_sti2s:
          case LIR_std2f:
#endif
          case LIR_sti:
          CASE64(LIR_stq:)
          case LIR_std:
            need(3);
            ins = mLir->insStore(mOpcode, ref(mTokens[0]),
                                  ref(mTokens[1]),
                                  immI(mTokens[2]), ACCSET_OTHER);
            break;

#if NJ_EXPANDED_LOADSTORE_SUPPORTED 
          case LIR_ldc2i:
          case LIR_lds2i:
          case LIR_ldf2d:
#endif
          case LIR_lduc2ui:
          case LIR_ldus2ui:
          case LIR_ldi:
          CASE64(LIR_ldq:)
          case LIR_ldd:
            ins = assemble_load();
            break;

          // XXX: insParam gives the one appropriate for the platform.  Eg. if
          // you specify qparam on x86 you'll end up with iparam anyway.  Fix
          // this.
          case LIR_paramp:
            need(2);
            ins = mLir->insParam(immI(mTokens[0]),
                                 immI(mTokens[1]));
            break;

          // XXX: similar to iparam/qparam above.
          case LIR_allocp:
            need(1);
            ins = mLir->insAlloc(immI(mTokens[0]));
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

          case LIR_addxovi:
          case LIR_subxovi:
          case LIR_mulxovi:
            ins = assemble_guard_xov();
            break;

          case LIR_addjovi:
          case LIR_subjovi:
          case LIR_muljovi:
          CASE64(LIR_addjovq:)
          CASE64(LIR_subjovq:)
            ins = assemble_jump_jov();
            break;

          case LIR_callv:
          case LIR_calli:
          CASESF(LIR_hcalli:)
          CASE64(LIR_callq:)
          case LIR_calld:
            ins = assemble_call(op);
            break;

          case LIR_reti:
            ins = assemble_ret(RT_INT);
            break;

#ifdef NANOJIT_64BIT
          case LIR_retq:
            ins = assemble_ret(RT_QUAD);
            break;
#endif

          case LIR_retd:
            ins = assemble_ret(RT_DOUBLE);
            break;

          case LIR_label:
            ins = mLir->ins0(LIR_label);
            if (!lab.empty()) {
                resolve_forward_jumps(lab, ins);
            }
            break;

          case LIR_file:
          case LIR_line:
          case LIR_xtbl:
          case LIR_jtbl:
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
static void f_V_IQF(int32_t, uint64_t, double)
{
    return;     // no need to do anything
}
#endif

const CallInfo ci_I_I1 = CI(f_I_I1, CallInfo::typeSig1(ARGTYPE_I, ARGTYPE_I));
const CallInfo ci_I_I6 = CI(f_I_I6, CallInfo::typeSig6(ARGTYPE_I, ARGTYPE_I, ARGTYPE_I, ARGTYPE_I,
                                                       ARGTYPE_I, ARGTYPE_I, ARGTYPE_I));

#ifdef NANOJIT_64BIT
const CallInfo ci_Q_Q2 = CI(f_Q_Q2, CallInfo::typeSig2(ARGTYPE_Q, ARGTYPE_Q, ARGTYPE_Q));
const CallInfo ci_Q_Q7 = CI(f_Q_Q7, CallInfo::typeSig7(ARGTYPE_Q, ARGTYPE_Q, ARGTYPE_Q, ARGTYPE_Q,
                                                       ARGTYPE_Q, ARGTYPE_Q, ARGTYPE_Q, ARGTYPE_Q));
#endif

const CallInfo ci_F_F3 = CI(f_F_F3, CallInfo::typeSig3(ARGTYPE_D, ARGTYPE_D, ARGTYPE_D, ARGTYPE_D));
const CallInfo ci_F_F8 = CI(f_F_F8, CallInfo::typeSig8(ARGTYPE_D, ARGTYPE_D, ARGTYPE_D, ARGTYPE_D,
                                                       ARGTYPE_D, ARGTYPE_D, ARGTYPE_D, ARGTYPE_D,
                                                       ARGTYPE_D));

#ifdef NANOJIT_64BIT
const CallInfo ci_V_IQF = CI(f_V_IQF, CallInfo::typeSig3(ARGTYPE_V, ARGTYPE_I, ARGTYPE_Q, ARGTYPE_D));
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
// - LIR_parami/LIR_paramq (hard to test beyond what is auto-generated in fragment
//   prologues)
// - LIR_livei/LIR_liveq/LIR_lived
// - LIR_hcalli
// - LIR_x/LIR_xt/LIR_xf/LIR_xtbl/LIR_addxovi/LIR_subxovi/LIR_mulxovi (hard to
//   test without having multiple fragments;  when we only have one fragment
//   we don't really want to leave it early)
// - LIR_reti/LIR_retq/LIR_retd (hard to test without having multiple fragments)
// - LIR_j/LIR_jt/LIR_jf/LIR_jtbl/LIR_label
// - LIR_file/LIR_line (#ifdef VTUNE only)
// - LIR_modd (not implemented in NJ backends)
//
// Other limitations:
// - Loads always use accSet==ACCSET_OTHER
// - Stores always use accSet==ACCSET_OTHER
//
void
FragmentAssembler::assembleRandomFragment(int nIns)
{
    vector<LIns*> Bs;       // boolean values, ie. 32-bit int values produced by tests
    vector<LIns*> Is;       // 32-bit int values
    vector<LIns*> Qs;       // 64-bit int values
    vector<LIns*> Ds;       // 64-bit double values
    vector<LIns*> M4s;      // 4 byte allocs
    vector<LIns*> M8ps;     // 8+ byte allocs

    vector<LOpcode> I_I_ops;
    I_I_ops.push_back(LIR_negi);
    I_I_ops.push_back(LIR_noti);

    // Nb: there are no Q_Q_ops.

    vector<LOpcode> D_D_ops;
    D_D_ops.push_back(LIR_negd);

    vector<LOpcode> I_II_ops;
    I_II_ops.push_back(LIR_addi);
    I_II_ops.push_back(LIR_subi);
    I_II_ops.push_back(LIR_muli);
#if defined NANOJIT_IA32 || defined NANOJIT_X64
    I_II_ops.push_back(LIR_divi);
    I_II_ops.push_back(LIR_modi);
#endif
    I_II_ops.push_back(LIR_andi);
    I_II_ops.push_back(LIR_ori);
    I_II_ops.push_back(LIR_xori);
    I_II_ops.push_back(LIR_lshi);
    I_II_ops.push_back(LIR_rshi);
    I_II_ops.push_back(LIR_rshui);

#ifdef NANOJIT_64BIT
    vector<LOpcode> Q_QQ_ops;
    Q_QQ_ops.push_back(LIR_addq);
    Q_QQ_ops.push_back(LIR_andq);
    Q_QQ_ops.push_back(LIR_orq);
    Q_QQ_ops.push_back(LIR_xorq);

    vector<LOpcode> Q_QI_ops;
    Q_QI_ops.push_back(LIR_lshq);
    Q_QI_ops.push_back(LIR_rshq);
    Q_QI_ops.push_back(LIR_rshuq);
#endif

    vector<LOpcode> D_DD_ops;
    D_DD_ops.push_back(LIR_addd);
    D_DD_ops.push_back(LIR_subd);
    D_DD_ops.push_back(LIR_muld);
    D_DD_ops.push_back(LIR_divd);

    vector<LOpcode> I_BII_ops;
    I_BII_ops.push_back(LIR_cmovi);

#ifdef NANOJIT_64BIT
    vector<LOpcode> Q_BQQ_ops;
    Q_BQQ_ops.push_back(LIR_cmovq);
#endif

    vector<LOpcode> D_BDD_ops;
    D_BDD_ops.push_back(LIR_cmovd);

    vector<LOpcode> B_II_ops;
    B_II_ops.push_back(LIR_eqi);
    B_II_ops.push_back(LIR_lti);
    B_II_ops.push_back(LIR_gti);
    B_II_ops.push_back(LIR_lei);
    B_II_ops.push_back(LIR_gei);
    B_II_ops.push_back(LIR_ltui);
    B_II_ops.push_back(LIR_gtui);
    B_II_ops.push_back(LIR_leui);
    B_II_ops.push_back(LIR_geui);

#ifdef NANOJIT_64BIT
    vector<LOpcode> B_QQ_ops;
    B_QQ_ops.push_back(LIR_eqq);
    B_QQ_ops.push_back(LIR_ltq);
    B_QQ_ops.push_back(LIR_gtq);
    B_QQ_ops.push_back(LIR_leq);
    B_QQ_ops.push_back(LIR_geq);
    B_QQ_ops.push_back(LIR_ltuq);
    B_QQ_ops.push_back(LIR_gtuq);
    B_QQ_ops.push_back(LIR_leuq);
    B_QQ_ops.push_back(LIR_geuq);
#endif

    vector<LOpcode> B_DD_ops;
    B_DD_ops.push_back(LIR_eqd);
    B_DD_ops.push_back(LIR_ltd);
    B_DD_ops.push_back(LIR_gtd);
    B_DD_ops.push_back(LIR_led);
    B_DD_ops.push_back(LIR_ged);

#ifdef NANOJIT_64BIT
    vector<LOpcode> Q_I_ops;
    Q_I_ops.push_back(LIR_i2q);
    Q_I_ops.push_back(LIR_ui2uq);

    vector<LOpcode> I_Q_ops;
    I_Q_ops.push_back(LIR_q2i);
#endif

    vector<LOpcode> D_I_ops;
#if !NJ_SOFTFLOAT_SUPPORTED
    // Don't emit LIR_{ui,i}2d for soft-float platforms because the soft-float filter removes them.
    D_I_ops.push_back(LIR_i2d);
    D_I_ops.push_back(LIR_ui2d);
#elif defined(NANOJIT_ARM)
    // The ARM back-end can detect FP support at run-time.
    if (avmplus::AvmCore::config.arm_vfp) {
        D_I_ops.push_back(LIR_i2d);
        D_I_ops.push_back(LIR_ui2d);
    }
#endif

    vector<LOpcode> I_D_ops;
#if NJ_SOFTFLOAT_SUPPORTED
    I_D_ops.push_back(LIR_dlo2i);
    I_D_ops.push_back(LIR_dhi2i);
#endif
#if !NJ_SOFTFLOAT_SUPPORTED
    // Don't emit LIR_d2i for soft-float platforms because the soft-float filter removes it.
    I_D_ops.push_back(LIR_d2i);
#elif defined(NANOJIT_ARM)
    // The ARM back-end can detect FP support at run-time.
    if (avmplus::AvmCore::config.arm_vfp) {
        I_D_ops.push_back(LIR_d2i);
    }
#endif

#ifdef NANOJIT_64BIT
    vector<LOpcode> Q_D_ops;
    Q_D_ops.push_back(LIR_dasq);

    vector<LOpcode> D_Q_ops;
    D_Q_ops.push_back(LIR_qasd);
#endif

    vector<LOpcode> D_II_ops;
#if NJ_SOFTFLOAT_SUPPORTED
    D_II_ops.push_back(LIR_ii2d);
#endif

    vector<LOpcode> I_loads;
    I_loads.push_back(LIR_ldi);          // weight LIR_ldi more heavily
    I_loads.push_back(LIR_ldi);
    I_loads.push_back(LIR_ldi);
    I_loads.push_back(LIR_lduc2ui);
    I_loads.push_back(LIR_ldus2ui);
#if NJ_EXPANDED_LOADSTORE_SUPPORTED 
    I_loads.push_back(LIR_ldc2i);
    I_loads.push_back(LIR_lds2i);
#endif

#ifdef NANOJIT_64BIT
    vector<LOpcode> Q_loads;
    Q_loads.push_back(LIR_ldq);
#endif

    vector<LOpcode> D_loads;
    D_loads.push_back(LIR_ldd);
#if NJ_EXPANDED_LOADSTORE_SUPPORTED
    // this loads a 32-bit float and expands it to 64-bit float
    D_loads.push_back(LIR_ldf2d);
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
    // LIR_allocp.  We then need to keep some reserve for spills as well.
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
            int32_t immI = 0;      // shut gcc up
            switch (rnd(5)) {
            case 0: immI = 0;                  break;
            case 1: immI = 1;                  break;
            case 2: immI = 4 * (rnd(256) + 1); break;  // 4, 8, ..., 1024
            case 3: immI = rnd(19999) - 9999;  break;  // -9999..9999
            case 4: immI = rndI32();           break;  // -RAND_MAX..RAND_MAX
            }
            ins = mLir->insImmI(immI);
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
            ins = mLir->insImmQ(imm64);
            addOrReplace(Qs, ins);
            n++;
            break;
        }
#endif

        case LIMM_D: {
            // We don't explicitly generate infinities and NaNs here, but they
            // end up occurring due to ExprFilter evaluating expressions like
            // divd(1,0) and divd(Infinity,Infinity).
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
            ins = mLir->insImmD(imm64f);
            addOrReplace(Ds, ins);
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

        case LOP_D_D:
            if (!Ds.empty()) {
                ins = mLir->ins1(rndPick(D_D_ops), rndPick(Ds));
                addOrReplace(Ds, ins);
                n++;
            }
            break;

        case LOP_I_II:
            if (!Is.empty()) {
                LOpcode op = rndPick(I_II_ops);
                LIns* lhs = rndPick(Is);
                LIns* rhs = rndPick(Is);
#if defined NANOJIT_IA32 || defined NANOJIT_X64
                if (op == LIR_divi || op == LIR_modi) {
                    // XXX: ExprFilter can't fold a div/mod with constant
                    // args, due to the horrible semantics of LIR_modi.  So we
                    // just don't generate anything if we hit that case.
                    if (!lhs->isImmI() || !rhs->isImmI()) {
                        // If the divisor is positive, no problems.  If it's zero, we get an
                        // exception.  If it's -1 and the dividend is -2147483648 (-2^31) we get
                        // an exception (and this has been encountered in practice).  So we only
                        // allow positive divisors, ie. compute:  lhs / (rhs > 0 ? rhs : -k),
                        // where k is a random number in the range 2..100 (this ensures we have
                        // some negative divisors).
                        LIns* gt0  = mLir->ins2ImmI(LIR_gti, rhs, 0);
                        LIns* rhs2 = mLir->ins3(LIR_cmovi, gt0, rhs, mLir->insImmI(-((int32_t)rnd(99)) - 2));
                        LIns* div  = mLir->ins2(LIR_divi, lhs, rhs2);
                        if (op == LIR_divi) {
                            ins = div;
                            addOrReplace(Is, ins);
                            n += 5;
                        } else {
                            ins = mLir->ins1(LIR_modi, div);
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

        case LOP_D_DD:
            if (!Ds.empty()) {
                ins = mLir->ins2(rndPick(D_DD_ops), rndPick(Ds), rndPick(Ds));
                addOrReplace(Ds, ins);
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

        case LOP_D_BDD:
            if (!Bs.empty() && !Ds.empty()) {
                ins = mLir->ins3(rndPick(D_BDD_ops), rndPick(Bs), rndPick(Ds), rndPick(Ds));
                addOrReplace(Ds, ins);
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

#ifdef NANOJIT_64BIT
        case LOP_B_QQ:
            if (!Qs.empty()) {
                ins = mLir->ins2(rndPick(B_QQ_ops), rndPick(Qs), rndPick(Qs));
                addOrReplace(Bs, ins);
                n++;
            }
            break;
#endif

        case LOP_B_DD:
            if (!Ds.empty()) {
                ins = mLir->ins2(rndPick(B_DD_ops), rndPick(Ds), rndPick(Ds));
                // XXX: we don't push the result, because most (all?) of the
                // backends currently can't handle cmovs/qcmovs that take
                // float comparisons for the test (see bug 520944).  This means
                // that all B_DD values are dead, unfortunately.
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

        case LOP_D_I:
            if (!Is.empty() && !D_I_ops.empty()) {
                ins = mLir->ins1(rndPick(D_I_ops), rndPick(Is));
                addOrReplace(Ds, ins);
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

        case LOP_I_D:
// XXX: NativeX64 doesn't implement qhi yet (and it may not need to).
#if !defined NANOJIT_X64
            if (!Ds.empty()) {
                ins = mLir->ins1(rndPick(I_D_ops), rndPick(Ds));
                addOrReplace(Is, ins);
                n++;
            }
#endif
            break;

#if defined NANOJIT_X64
        case LOP_Q_D:
            if (!Ds.empty()) {
                ins = mLir->ins1(rndPick(Q_D_ops), rndPick(Ds));
                addOrReplace(Qs, ins);
                n++;
            }
            break;

        case LOP_D_Q:
            if (!Qs.empty()) {
                ins = mLir->ins1(rndPick(D_Q_ops), rndPick(Qs));
                addOrReplace(Ds, ins);
                n++;
            }
            break;
#endif

        case LOP_D_II:
            if (!Is.empty() && !D_II_ops.empty()) {
                ins = mLir->ins2(rndPick(D_II_ops), rndPick(Is), rndPick(Is));
                addOrReplace(Ds, ins);
                n++;
            }
            break;

        case LLD_I: {
            vector<LIns*> Ms = rnd(2) ? M4s : M8ps;
            if (!Ms.empty()) {
                LIns* base = rndPick(Ms);
                ins = mLir->insLoad(rndPick(I_loads), base, rndOffset32(base->size()), ACCSET_OTHER);
                addOrReplace(Is, ins);
                n++;
            }
            break;
        }

#ifdef NANOJIT_64BIT
        case LLD_Q:
            if (!M8ps.empty()) {
                LIns* base = rndPick(M8ps);
                ins = mLir->insLoad(rndPick(Q_loads), base, rndOffset64(base->size()), ACCSET_OTHER);
                addOrReplace(Qs, ins);
                n++;
            }
            break;
#endif

        case LLD_D:
            if (!M8ps.empty()) {
                LIns* base = rndPick(M8ps);
                ins = mLir->insLoad(rndPick(D_loads), base, rndOffset64(base->size()), ACCSET_OTHER);
                addOrReplace(Ds, ins);
                n++;
            }
            break;

        case LST_I: {
            vector<LIns*> Ms = rnd(2) ? M4s : M8ps;
            if (!Ms.empty() && !Is.empty()) {
                LIns* base = rndPick(Ms);
                mLir->insStore(rndPick(Is), base, rndOffset32(base->size()), ACCSET_OTHER);
                n++;
            }
            break;
        }

#ifdef NANOJIT_64BIT
        case LST_Q:
            if (!M8ps.empty() && !Qs.empty()) {
                LIns* base = rndPick(M8ps);
                mLir->insStore(rndPick(Qs), base, rndOffset64(base->size()), ACCSET_OTHER);
                n++;
            }
            break;
#endif

        case LST_D:
            if (!M8ps.empty() && !Ds.empty()) {
                LIns* base = rndPick(M8ps);
                mLir->insStore(rndPick(Ds), base, rndOffset64(base->size()), ACCSET_OTHER);
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

        case LCALL_D_D3:
            if (!Ds.empty()) {
                LIns* args[3] = { rndPick(Ds), rndPick(Ds), rndPick(Ds) };
                ins = mLir->insCall(&ci_F_F3, args);
                addOrReplace(Ds, ins);
                n++;
            }
            break;

        case LCALL_D_D8:
            if (!Ds.empty()) {
                LIns* args[8] = { rndPick(Ds), rndPick(Ds), rndPick(Ds), rndPick(Ds),
                                  rndPick(Ds), rndPick(Ds), rndPick(Ds), rndPick(Ds) };
                ins = mLir->insCall(&ci_F_F8, args);
                addOrReplace(Ds, ins);
                n++;
            }
            break;

#ifdef NANOJIT_64BIT
        case LCALL_V_IQD:
            if (!Is.empty() && !Qs.empty() && !Ds.empty()) {
                // Nb: args[] holds the args in reverse order... sigh.
                LIns* args[3] = { rndPick(Ds), rndPick(Qs), rndPick(Is) };
                ins = mLir->insCall(&ci_V_IQF, args);
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
    mReturnTypeBits |= RT_INT;
    mLir->ins1(LIR_reti, mLir->insImmI(0));

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
        mLogc.lcbits = LC_ReadLIR | LC_AfterDCE | LC_Native | LC_RegAlloc | LC_Activation;
        mLirbuf->printer = new (mAlloc) LInsPrinter(mAlloc, LIRASM_NUM_USED_ACCS);
    }
#endif

    // Populate the mOpMap table.
#define OP___(op, number, repKind, retType, isCse) \
    mOpMap[#op] = LIR_##op;
#include "nanojit/LIRopcode.tbl"
#undef OP___

    // XXX: could add more pointer-sized synonyms here
    mOpMap["paramp"] = mOpMap[PTR_SIZE("parami", "paramq")];
    mOpMap["livep"]  = mOpMap[PTR_SIZE("livei", "liveq")];
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
        if (func->second.mReturnType == RT_DOUBLE) {
            CallInfo target = {(uintptr_t) func->second.rdouble,
                               0, ABI_FASTCALL, /*isPure*/0, ACCSET_STORE_ANY
                               verbose_only(, func->first.c_str()) };
            *ci = target;

        } else {
            CallInfo target = {(uintptr_t) func->second.rint,
                               0, ABI_FASTCALL, /*isPure*/0, ACCSET_STORE_ANY
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
        "  --word-size      prints the word size (32 or 64) for this build of lirasm and exits\n"
        "  --endianness     prints endianness (little-endian or big-endian) for this build of librasm and exits\n"
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
        else if (arg == "--word-size") {
            cout << sizeof(void*) * 8 << "\n";
            exit(0);
        }
        else if (arg == "--endianness") {
            int32_t x = 0x01020304;
            if (*(char*)&x == 0x1) {
              cout << "big-endian" << "\n";
            } else {
              cout << "little-endian" << "\n";
            }
            exit(0);
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
                if ((arm_arch < 4) || (arm_arch > 7)) {
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
    // Warn about untested configurations.
    if ( ((arm_arch == 5) && (arm_vfp)) || ((arm_arch >= 6) && (!arm_vfp)) ) {
        char const * vfp_string = (arm_vfp) ? ("VFP") : ("no VFP");
        cerr << "Warning: This configuration (ARMv" << arm_arch << ", " << vfp_string << ") " <<
                "is not regularly tested." << endl;
    }

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
          case RT_INT: {
            int res = i->second.rint();
            cout << "Output is: " << res << endl;
            break;
          }
#ifdef NANOJIT_64BIT
          case RT_QUAD: {
            int res = i->second.rquad();
            cout << "Output is: " << res << endl;
            break;
          }
#endif
          case RT_DOUBLE: {
            double res = i->second.rdouble();
            cout << "Output is: " << res << endl;
            break;
          }
          case RT_GUARD: {
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
