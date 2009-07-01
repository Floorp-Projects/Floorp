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

using namespace avmplus;
using namespace nanojit;
using namespace std;

static GC gc;
static avmplus::AvmCore s_core;
static avmplus::AvmCore* core = &s_core;
Fragment *frag;

typedef JS_FASTCALL int32_t (*RetInt)();
typedef JS_FASTCALL double (*RetFloat)();

struct Rfunc {
    union {
        RetFloat rfloat;
        RetInt rint;
    };
    bool isFloat;
    Fragment *fragptr;
};
typedef map<string, Rfunc> Fragments;

struct Pipeline {
    LirWriter *lir;
    LirBufWriter *w;
    LirWriter *cse_filter;
    LirWriter *expr_filter;
    LirWriter *verb;
};

struct Function {
    const char *name;
    struct nanojit::CallInfo callInfo;
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
Function functions[] = {
    FN(puts, I32 | (PTRARG<<2)),
    FN(sin, F64 | (F64<<2)),
    FN(malloc, PTRRET | (PTRARG<<2)),
    FN(free, I32 | (PTRARG<<2))
};

void
lookupFunction(const char *name,
               const Fragments &frags,
               CallInfo *&ci)
{
    const size_t nfuns = sizeof(functions) / sizeof(functions[0]);
    Fragments::const_iterator func;

    for (size_t i = 0; i < nfuns; i++)
        if (strcmp(name, functions[i].name) == 0) {
            *ci = functions[i].callInfo;
            return;
        }

    if ((func = frags.find(name)) != frags.end()) {
        if (func->second.isFloat) {
            CallInfo FragCall = {(uintptr_t) func->second.rfloat, ARGSIZE_F, 0,
                                 0, nanojit::ABI_FASTCALL, func->first.c_str()};
            *ci = FragCall;

        } else {
            CallInfo FragCall = {(uintptr_t) func->second.rint, ARGSIZE_LO, 0,
                                 0, nanojit::ABI_FASTCALL, func->first.c_str()};
            *ci = FragCall;
        }

        return;
    }
    ci = NULL;
}

istream &
read_and_tokenize_line(istream &in, vector<string> &toks, size_t &linenum)
{
    char buf[1024];
    string tok_sep(" \n\t");

    toks.clear();

    if (in.getline(buf,sizeof(buf))) {
        ++linenum;
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
                toks[toks.size()-1] += ss;
                continue;
            }
            toks.push_back(ss);
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
bad(string const &msg, size_t insn)
{
    cerr << "instruction " << insn << ": " <<  msg << endl;
    exit(1);
}

void
need(vector<string> const &toks, size_t need, size_t line)
{
    if (toks.size() != need)
        bad("need " + lexical_cast<string>(need)
            + " tokens, have " + lexical_cast<string>(toks.size()), line);
}

LIns*
ref(map<string,LIns*> const &labels,
    string const &lab,
    size_t line)
{
    if (labels.find(lab) == labels.end())
        bad("unknown label '" + lab + "'", line);
    return labels.find(lab)->second;
}

LIns*
do_skip(LirWriter *lir,
        size_t i)
{
    LIns *s = lir->insSkip(i);
    memset(s->payload(), 0xba, i);
    return s;
}

LIns*
assemble_jump(LOpcode opcode,
              vector<string> &toks,
              LirWriter *lir,
              map<string,LIns*> &labels,
              multimap<string,LIns*> &fwd_jumps,
              size_t line)
{
    LIns *target = NULL;
    LIns *condition = NULL;

    if (opcode == LIR_j) {
        need(toks, 1, line);
    } else {
        need(toks, 2, line);
        string cond = pop_front(toks);
        condition = ref(labels, cond, line);
    }
    string targ = pop_front(toks);
    if (labels.find(targ) != labels.end()) {
        target = ref(labels, targ, line);
        return lir->insBranch(opcode, condition, target);
    } else {
        LIns *ins = lir->insBranch(opcode, condition, target);
        fwd_jumps.insert(make_pair(targ, ins));
        return ins;
    }
}

LIns*
assemble_load(LOpcode opcode,
              vector<string> &toks,
              LirWriter *lir,
              map<string,LIns*> const &labels,
              size_t line)
{
    // Support implicit immediate-as-second-operand modes
    // since, unlike sti/stqi, no immediate-displacement
    // load opcodes were defined in LIR.
    need(toks, 2, line);
    if (toks[1].find("0x") == 0 ||
        toks[1].find("0x") == 0 ||
        toks[1].find_first_of("0123456789") == 0) {
        return lir->insLoad(opcode,
                            ref(labels, toks[0], line),
                            imm(toks[1]));
    } else {
        return lir->insLoad(opcode,
                            ref(labels, toks[0], line),
                            ref(labels, toks[1], line));
    }
}

LIns*
assemble_call(string const &op,
              LOpcode opcode,
              vector<string> &toks,
              LirWriter *lir,
              map<string,LIns*> const &labels,
              vector<CallInfo*> &callinfos,
              size_t line,
              const Fragments &frags)
{
    CallInfo *ci = new CallInfo();
    callinfos.push_back(ci);
    LIns* args[MAXARGS];

    // Assembler synax for a call:
    //
    //   call 0x1234 fastcall a b c
    //
    // requires at least 2 args,
    // fn address immediate and ABI token.
    //
    // FIXME: This should eventually be changed to handle calls to
    // real C runtime library functions registered with lirasm on
    // startup, when and if there's a load-and-go execution mode for
    // it. For the time being it only dumps out s-records for
    // diagnostics, so we don't bother linking in real function
    // pointers yet.

    if (toks.size() < 2)
        bad("need at least address and ABI code for " + op, line);

    string func = pop_front(toks);
    string abi = pop_front(toks);

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
        bad("call abi name '" + abi + "'", line);
    ci->_abi = _abi;

    if (toks.size() > MAXARGS)
    bad("too many args to " + op, line);

    if (func.find("0x") == 0) {
        ci->_address = imm(func);

        ci->_cse = 0;
        ci->_fold = 0;

#ifdef DEBUG
        ci->_name = "fn";
#endif

    } else {
        lookupFunction(func.c_str(), frags, ci);
        if (ci == NULL)
            bad("invalid function reference " + func, line);
        if (_abi != ci->_abi)
            bad("invalid calling convention for " + func, line);
    }

    ci->_argtypes = 0;

    for (size_t i = 0; i < toks.size(); ++i) {
        args[i] = ref(labels, toks[toks.size() - (i+1)], line);
        ci->_argtypes |= args[i]->isQuad() ? ARGSIZE_F : ARGSIZE_LO;
        ci->_argtypes <<= 2;
    }

    // Select return type from opcode.
    // FIXME: callh needs special treatment currently
    // missing from here.
    if (opcode == LIR_call)
        ci->_argtypes |= ARGSIZE_LO;
    else
        ci->_argtypes |= ARGSIZE_F;

    return lir->insCall(ci, args);
}

LIns*
assemble_general(size_t opcount,
                 LOpcode opcode,
                 vector<string> &toks,
                 LirWriter *lir,
                 map<string,LIns*> const &labels,
                 size_t line)
{
    if (opcount == 0) {
        // 0-ary ops may, or may not, have an immediate
        // thing wedged in them; depends on the op. We
        // are lax and set it if it's provided.
        LIns *ins = lir->ins0(opcode);
        if (toks.size() > 0) {
            assert(toks.size() == 1);
            ins->setImm(opcode, imm(toks[0]));
        }
        return ins;
    } else {
        need(toks, opcount, line);
        if (opcount == 1) {
            return lir->ins1(opcode,
                             ref(labels, toks[0], line));
        } else if (opcount == 2) {
            return lir->ins2(opcode,
                             ref(labels, toks[0], line),
                             ref(labels, toks[1], line));
        } else {
            bad("too many operands", line);
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
extract_any_label(string &op,
                  vector<string> &toks,
                  string &lab,
                  map<string,LIns*> const &labels,
                  size_t line,
                  char lab_delim)
{
    if (op.size() > 1 &&
        op[op.size()-1] == lab_delim &&
        !toks.empty()) {

        lab = op;
        op = pop_front(toks);
        lab.erase(lab.size()-1);

        if (labels.find(lab) != labels.end())
            bad("duplicate label", line);
    }
}

void
beginFragment(Pipeline &writer,
              LirBuffer *lirbuf,
              Fragmento *fragmento,
              LogControl *logc,
              bool verbose,
              bool &inFrag,
              bool &isFloat)
{
    frag = new (&gc) Fragment(NULL);
    frag->lirbuf = lirbuf;
    frag->anchor = frag;
    frag->root = frag;

    writer.w = new (&gc) LirBufWriter(lirbuf);
    writer.cse_filter = new (&gc) CseFilter(writer.w, &gc);
    writer.expr_filter = new (&gc) ExprFilter(writer.cse_filter);
#ifdef DEBUG
    writer.verb = new (&gc) VerboseWriter(&gc, writer.expr_filter, lirbuf->names, logc);
#endif

    writer.lir = writer.expr_filter;
#ifdef DEBUG
    if (verbose)
        writer.lir = writer.verb;
#endif
    inFrag = true;
    isFloat = false;
    writer.lir->ins0(LIR_start);
}

void
endFragment(const Pipeline &writer,
            Fragmento *fragmento,
            vector<CallInfo*> &callinfos,
            bool isFloat,
            map<string, LIns*> &labels,
            Fragments &frags,
            string fragname,
            bool &inFrag)
{

    inFrag = false;

    LIns *exitIns = do_skip(writer.lir, sizeof(VMSideExit));
    VMSideExit* exit = (VMSideExit*) exitIns->payload();
    memset(exit, 0, sizeof(VMSideExit));
    exit->guards = NULL;
    exit->from = exit->target = frag;
    frag->lastIns = writer.lir->insGuard(LIR_loop, writer.lir->insImm(1), exitIns);

    ::compile(fragmento->assm(), frag);

    for (size_t i = 0; i < callinfos.size(); ++i)
        delete callinfos[i];
    callinfos.clear();

    if (fragmento->assm()->error() != nanojit::None) {
        cerr << "error during assembly: ";
        switch (fragmento->assm()->error()) {
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

    Rfunc f;
    if (isFloat) {
        f.rfloat = reinterpret_cast<RetFloat>(frag->code());
        f.isFloat = true;
    } else {
        f.rint = reinterpret_cast<RetInt>(frag->code());
        f.isFloat = false;
    }
    f.fragptr = frag;

    if (fragname.empty())
        fragname = "main";
    frags[fragname] = f;

    delete writer.expr_filter;
    delete writer.cse_filter;
    delete writer.w;
    delete writer.verb;
    labels.clear();
}

void
assemble(istream &in,
         LirBuffer *lirbuf,
         Fragmento *fragmento,
         LogControl *logc,
         vector<CallInfo*> &callinfos,
         Fragments &frags,
         bool verbose)
{

    Pipeline writer;

    multimap<string,LIns*> fwd_jumps;
    map<string,LIns*> labels;
    map<string,pair<LOpcode,size_t> > op_map;

#define OPDEF(op, number, args) \
    op_map[#op] = make_pair(LIR_##op, args);
#define OPDEF64(op, number, args) \
    op_map[#op] = make_pair(LIR_##op, args);
#include "nanojit/LIRopcode.tbl"
#undef OPDEF
#undef OPDEF64

    vector<string> toks;
    size_t line = 0;

    bool isFloat = false;
    bool inFrag = false;
    bool singleFrag = false;
    bool first = true;
    string fragname;

    while(read_and_tokenize_line(in, toks, line)) {

        if (lirbuf->outOMem()) {
            cerr << "lirbuf out of memory" << endl;
            exit(1);
        }
        if (toks.empty())
            continue;

        string op = pop_front(toks);

        if (!singleFrag) {
            if (op == ".begin") {
                if (toks.size() != 1)
                    bad("missing fragment name", line);
                if (inFrag)
                    bad("nested fragments are not supported", line);

                fragname = pop_front(toks);

                beginFragment(writer, lirbuf, fragmento, logc,
                              verbose, inFrag, isFloat);
                first = false;
                continue;
            } else if (op == ".end") {
                if (!inFrag)
                    bad("expecting .begin before .end", line);
                if (!toks.empty())
                    bad("too many tokens", line);
                endFragment(writer, fragmento, callinfos, isFloat,
                            labels, frags, fragname, inFrag);
                continue;
            }
        }
        if (first) {
            first = false;
            singleFrag = true;

            beginFragment(writer, lirbuf, fragmento, logc,
                          verbose, inFrag, isFloat);
        }

        LirWriter *lir = writer.lir;
        string lab;
        LIns *ins = NULL;
        extract_any_label(op, toks, lab, labels, line, ':');

        /* Save label and do any back-patching of deferred forward-jumps. */
        if (!lab.empty()) {
            ins = lir->ins0(LIR_label);
            typedef multimap<string,LIns*> mulmap;
            typedef mulmap::const_iterator ci;
            pair<ci,ci> range = fwd_jumps.equal_range(lab);
            for (ci i = range.first; i != range.second; ++i) {
                i->second->setTarget(ins);
            }
            fwd_jumps.erase(lab);
            lab.clear();
        }
        extract_any_label(op, toks, lab, labels, line, '=');

        if (op_map.find(op) == op_map.end())
            bad("unknown instruction '" + op + "'", line);

        pair<LOpcode,size_t> entry = op_map[op];
        LOpcode opcode = entry.first;
        size_t opcount = entry.second;

        switch (opcode) {
        // A few special opcode cases.

        case LIR_j:
        case LIR_jt:
        case LIR_jf:
        case LIR_ji:
            ins = assemble_jump(opcode, toks, lir,
                                labels, fwd_jumps, line);
            break;

        case LIR_int:
            need(toks, 1, line);
            ins = lir->insImm(imm(toks[0]));
            break;

        case LIR_quad:
            need(toks, 1, line);
            ins = lir->insImmq(quad(toks[0]));
            break;

        case LIR_sti:
        case LIR_stqi:
            need(toks, 3, line);
            ins = lir->insStorei(ref(labels, toks[0], line),
                                 ref(labels, toks[1], line),
                                 imm(toks[2]));
            break;

        case LIR_ld:
        case LIR_ldc:
        case LIR_ldq:
        case LIR_ldqc:
        case LIR_ldcb:
        case LIR_ldcs:
            ins = assemble_load(opcode, toks, lir,
                                labels, line);
            break;

        case LIR_param:
            need(toks, 2, line);
            ins = lir->insParam(imm(toks[0]),
                                imm(toks[1]));
            break;

        case LIR_alloc:
            need(toks, 1, line);
            ins = lir->insAlloc(imm(toks[0]));
            break;

        case LIR_skip:
            need(toks, 1, line);
            {
                int32_t count = imm(toks[0]);
                if (uint32_t(count) > NJ_MAX_SKIP_PAYLOAD_SZB)
                    bad("oversize skip", line);
                ins = do_skip(lir, count);
            }
            break;

        case LIR_call:
        case LIR_callh:
        case LIR_fcall:
            ins = assemble_call(op, opcode, toks, lir,
                                labels, callinfos, line,
                                frags);
            break;

        case LIR_fret:
            isFloat = true;
            /* FALL THROUGH */
        default:
            ins = assemble_general(opcount, opcode, toks,
                                   lir, labels, line);
            break;
        }

        assert(ins);
        if (!lab.empty())
            labels.insert(make_pair(lab, ins));
    }
    if (inFrag && singleFrag)
        endFragment(writer, fragmento, callinfos, isFloat,
                    labels, frags, fragname, inFrag);

    if (inFrag)
        bad("unexpected EOF", line);
    if (lirbuf->outOMem()) {
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

    nanojit::AvmCore::config.tree_opt = true;

    LogControl logc;
    logc.lcbits = 0;
#ifdef DEBUG
    if (verbose)
        logc.lcbits = LC_Assembly;
#endif

    Fragmento *fragmento = new (&gc) Fragmento(core, &logc, 32);
    LirBuffer *lirbuf = new (&gc) LirBuffer(fragmento);
#ifdef DEBUG
    if (verbose) {
        fragmento->labels = new (&gc) LabelMap(core);
        lirbuf->names = new (&gc) LirNameMap(&gc, fragmento->labels);
    }
#endif

    vector<CallInfo*> callinfos;

    ifstream in(args[0].c_str());
    if (!in) {
        cerr << prog << ": error: unable to open file " << args[0] << endl;
        exit(1);
    }
    Fragments frags;

    assemble(in, lirbuf, fragmento, &logc,
             callinfos, frags, verbose);

    Fragments::iterator i;
    if (execute) {
        i = frags.find("main");
        if (i == frags.end()) {
            cerr << prog << ": error: atleast one fragment must be named main" << endl;
            exit(1);
        }
        if (i->second.isFloat)
            cout << "Output is: " << i->second.rfloat() << endl;
        else
            cout << "Output is: " << i->second.rint() << endl;
    } else {
        Fragments::const_iterator i = frags.begin();
        for ( i = frags.begin(); i != frags.end(); i++)
            dump_srecords(cout, i->second.fragptr);
    }

    for (i = frags.begin(); i != frags.end(); ++i) {
        i->second.fragptr->releaseCode(fragmento);
        delete i->second.fragptr;
    }

    delete lirbuf;
#ifdef DEBUG
    if (verbose)
        delete fragmento->labels;
#endif
    delete fragmento;
}
