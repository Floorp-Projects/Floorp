/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79 ft=cpp:
 *
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   Andreas Gal <gal@mozilla.com>
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

#ifndef jstracer_h___
#define jstracer_h___

#include "jsstddef.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsinterp.h"

#include "nanojit/nanojit.h"

/*
 * We use a magic boxed pointer value to represent error conditions that 
 * trigger a side exit. The address is so low that it should never be
 * actually in use. If it is, a performance regression occurs, not an
 * actual runtime error.
 */
#define JSVAL_ERROR_COOKIE OBJECT_TO_JSVAL((void*)0x10)

/*
 * We also need a magic unboxed 32-bit integer that signals an error.
 * Again if this number is hit we experience a performance regression,
 * not a runtime error.
 */
#define INT32_ERROR_COOKIE 0xffffabcd

/*
 * Tracker is used to keep track of values being manipulated by the 
 * interpreter during trace recording.
 */
template <typename T>
class Tracker 
{
    struct Page {
        struct Page*    next;
        jsuword         base;
        T               map[0];
    };
    struct Page* pagelist;
    
    jsuword         getPageBase(const void* v) const;
    struct Page*    findPage(const void* v) const;
    struct Page*    addPage(const void* v);
public:    
    Tracker();
    ~Tracker();
    
    T               get(const void* v) const;
    void            set(const void* v, T ins);
    void            clear();
};

struct VMFragmentInfo {
    unsigned                entryNativeFrameSlots;
    unsigned                maxNativeFrameSlots;
    size_t                  nativeStackBase;
    uint8                   typeMap[0];
};

struct VMSideExitInfo {
    uint8                   typeMap[0];
};

#define TYPEMAP_GET_TYPE(x)         ((x) & JSVAL_TAGMASK)
#define TYPEMAP_GET_FLAG(x, flag)   ((x) & flag)
#define TYPEMAP_SET_FLAG(x, flag)   do { (x) |= flag; } while (0)

#define TYPEMAP_FLAG_DEMOTE 0x10 /* try to record as int */
#define TYPEMAP_FLAG_DONT_DEMOTE 0x20 /* do not try to record as int */

class TraceRecorder {
    JSContext*              cx;
    Tracker<nanojit::LIns*> tracker;
    char*                   entryTypeMap;
    struct JSStackFrame*    entryFrame;
    struct JSFrameRegs      entryRegs;
    nanojit::Fragment*      fragment;
    VMFragmentInfo*         fragmentInfo;
    nanojit::LirBuffer*     lirbuf;
    nanojit::LirWriter*     lir;
    nanojit::LirBufWriter*  lir_buf_writer;
    nanojit::LirWriter*     verbose_filter;
    nanojit::LirWriter*     cse_filter;
    nanojit::LirWriter*     expr_filter;
    nanojit::LirWriter*     exit_filter;
    nanojit::LirWriter*     func_filter;
    nanojit::LIns*          cx_ins;
    nanojit::SideExit       exit;
    bool                    recompileFlag;
    
    JSStackFrame* findFrame(void* p) const;
    bool onFrame(void* p) const;
    unsigned nativeFrameSlots(JSStackFrame* fp, JSFrameRegs& regs) const;
    size_t nativeFrameOffset(void* p) const;
    void import(jsval*, uint8& t, char *prefix, int index);
    void trackNativeFrameUse(unsigned slots);
    
    unsigned getCallDepth() const;
    void guard(bool expected, nanojit::LIns* cond);

    void set(void* p, nanojit::LIns* l);

    bool checkType(jsval& v, uint8& type);
    bool verifyTypeStability(JSStackFrame* fp, JSFrameRegs& regs, uint8* m);
    void closeLoop(nanojit::Fragmento* fragmento);
    
    jsval& argval(unsigned n) const;
    jsval& varval(unsigned n) const;
    jsval& stackval(int n) const;
    
    nanojit::LIns* arg(unsigned n);
    void arg(unsigned n, nanojit::LIns* i);
    nanojit::LIns* var(unsigned n);
    void var(unsigned n, nanojit::LIns* i);
    nanojit::LIns* stack(int n);
    void stack(int n, nanojit::LIns* i);
    
    nanojit::LIns* f2i(nanojit::LIns* f);

    bool ifop(bool sense);
    bool inc(jsval& v, jsint incr, bool pre);
    bool cmp(nanojit::LOpcode op, bool negate = false);

    bool unary(nanojit::LOpcode op);
    bool binary(nanojit::LOpcode op);
    
    bool ibinary(nanojit::LOpcode op); 
    bool iunary(nanojit::LOpcode op);
    bool bbinary(nanojit::LOpcode op); 
    void demote(jsval& v, jsdouble result);
    
    bool map_is_native(JSObjectMap* map, nanojit::LIns* map_ins);
    bool test_property_cache(JSObject* obj, nanojit::LIns* obj_ins, JSObject*& obj2,
                             JSPropCacheEntry*& entry);
    void stobj_set_slot(nanojit::LIns* obj_ins, unsigned slot, 
            nanojit::LIns*& dslots_ins, nanojit::LIns* v_ins);
    nanojit::LIns* stobj_get_slot(nanojit::LIns* obj_ins, unsigned slot, 
            nanojit::LIns*& dslots_ins);
    bool native_set(nanojit::LIns* obj_ins, JSScopeProperty* sprop, 
            nanojit::LIns*& dslots_ins, nanojit::LIns* v_ins);
    bool native_get(nanojit::LIns* obj_ins, nanojit::LIns* pobj_ins, JSScopeProperty* sprop, 
            nanojit::LIns*& dslots_ins, nanojit::LIns*& v_ins);

    bool box_jsval(jsval v, nanojit::LIns*& v_ins);
    bool unbox_jsval(jsval v, nanojit::LIns*& v_ins);
    bool guardThatObjectIsDenseArray(JSObject* obj, 
            nanojit::LIns* obj_ins, nanojit::LIns*& dslots_ins);
    bool guardDenseArrayIndexWithinBounds(JSObject* obj, jsint idx, 
            nanojit::LIns* obj_ins, nanojit::LIns*& dslots_ins, nanojit::LIns* idx_ins);
public:
    TraceRecorder(JSContext* cx, nanojit::Fragmento*, nanojit::Fragment*);
    ~TraceRecorder();

    JSStackFrame* getEntryFrame() const;
    JSStackFrame* getFp() const;
    JSFrameRegs& getRegs() const;
    nanojit::Fragment* getFragment() const;
    nanojit::SideExit* snapshot();

    nanojit::LIns* get(void* p);
    
    bool loopEdge();
    void stop();
    
    bool JSOP_INTERRUPT();
    bool JSOP_PUSH();
    bool JSOP_POPV();
    bool JSOP_ENTERWITH();
    bool JSOP_LEAVEWITH();
    bool JSOP_RETURN();
    bool JSOP_GOTO();
    bool JSOP_IFEQ();
    bool JSOP_IFNE();
    bool JSOP_ARGUMENTS();
    bool JSOP_FORARG();
    bool JSOP_FORVAR();
    bool JSOP_DUP();
    bool JSOP_DUP2();
    bool JSOP_SETCONST();
    bool JSOP_BITOR();
    bool JSOP_BITXOR();
    bool JSOP_BITAND();
    bool JSOP_EQ();
    bool JSOP_NE();
    bool JSOP_LT();
    bool JSOP_LE();
    bool JSOP_GT();
    bool JSOP_GE();
    bool JSOP_LSH();
    bool JSOP_RSH();
    bool JSOP_URSH();
    bool JSOP_ADD();
    bool JSOP_SUB();
    bool JSOP_MUL();
    bool JSOP_DIV();
    bool JSOP_MOD();
    bool JSOP_NOT();
    bool JSOP_BITNOT();
    bool JSOP_NEG();
    bool JSOP_NEW();
    bool JSOP_DELNAME();
    bool JSOP_DELPROP();
    bool JSOP_DELELEM();
    bool JSOP_TYPEOF();
    bool JSOP_VOID();
    bool JSOP_INCNAME();
    bool JSOP_INCPROP();
    bool JSOP_INCELEM();
    bool JSOP_DECNAME();
    bool JSOP_DECPROP();
    bool JSOP_DECELEM();
    bool JSOP_NAMEINC();
    bool JSOP_PROPINC();
    bool JSOP_ELEMINC();
    bool JSOP_NAMEDEC();
    bool JSOP_PROPDEC();
    bool JSOP_ELEMDEC();
    bool JSOP_GETPROP();
    bool JSOP_SETPROP();
    bool JSOP_GETELEM();
    bool JSOP_SETELEM();
    bool JSOP_CALLNAME();
    bool JSOP_CALL();
    bool JSOP_NAME();
    bool JSOP_DOUBLE();
    bool JSOP_STRING();
    bool JSOP_ZERO();
    bool JSOP_ONE();
    bool JSOP_NULL();
    bool JSOP_THIS();
    bool JSOP_FALSE();
    bool JSOP_TRUE();
    bool JSOP_OR();
    bool JSOP_AND();
    bool JSOP_TABLESWITCH();
    bool JSOP_LOOKUPSWITCH();
    bool JSOP_STRICTEQ();
    bool JSOP_STRICTNE();
    bool JSOP_CLOSURE();
    bool JSOP_EXPORTALL();
    bool JSOP_EXPORTNAME();
    bool JSOP_IMPORTALL();
    bool JSOP_IMPORTPROP();
    bool JSOP_IMPORTELEM();
    bool JSOP_OBJECT();
    bool JSOP_POP();
    bool JSOP_POS();
    bool JSOP_TRAP();
    bool JSOP_GETARG();
    bool JSOP_SETARG();
    bool JSOP_GETVAR();
    bool JSOP_SETVAR();
    bool JSOP_UINT16();
    bool JSOP_NEWINIT();
    bool JSOP_ENDINIT();
    bool JSOP_INITPROP();
    bool JSOP_INITELEM();
    bool JSOP_DEFSHARP();
    bool JSOP_USESHARP();
    bool JSOP_INCARG();
    bool JSOP_INCVAR();
    bool JSOP_DECARG();
    bool JSOP_DECVAR();
    bool JSOP_ARGINC();
    bool JSOP_VARINC();
    bool JSOP_ARGDEC();
    bool JSOP_VARDEC();
    bool JSOP_ITER();
    bool JSOP_FORNAME();
    bool JSOP_FORPROP();
    bool JSOP_FORELEM();
    bool JSOP_POPN();
    bool JSOP_BINDNAME();
    bool JSOP_SETNAME();
    bool JSOP_THROW();
    bool JSOP_IN();
    bool JSOP_INSTANCEOF();
    bool JSOP_DEBUGGER();
    bool JSOP_GOSUB();
    bool JSOP_RETSUB();
    bool JSOP_EXCEPTION();
    bool JSOP_LINENO();
    bool JSOP_CONDSWITCH();
    bool JSOP_CASE();
    bool JSOP_DEFAULT();
    bool JSOP_EVAL();
    bool JSOP_ENUMELEM();
    bool JSOP_GETTER();
    bool JSOP_SETTER();
    bool JSOP_DEFFUN();
    bool JSOP_DEFCONST();
    bool JSOP_DEFVAR();
    bool JSOP_ANONFUNOBJ();
    bool JSOP_NAMEDFUNOBJ();
    bool JSOP_SETLOCALPOP();
    bool JSOP_GROUP();
    bool JSOP_SETCALL();
    bool JSOP_TRY();
    bool JSOP_FINALLY();
    bool JSOP_NOP();
    bool JSOP_ARGSUB();
    bool JSOP_ARGCNT();
    bool JSOP_DEFLOCALFUN();
    bool JSOP_GOTOX();
    bool JSOP_IFEQX();
    bool JSOP_IFNEX();
    bool JSOP_ORX();
    bool JSOP_ANDX();
    bool JSOP_GOSUBX();
    bool JSOP_CASEX();
    bool JSOP_DEFAULTX();
    bool JSOP_TABLESWITCHX();
    bool JSOP_LOOKUPSWITCHX();
    bool JSOP_BACKPATCH();
    bool JSOP_BACKPATCH_POP();
    bool JSOP_THROWING();
    bool JSOP_SETRVAL();
    bool JSOP_RETRVAL();
    bool JSOP_GETGVAR();
    bool JSOP_SETGVAR();
    bool JSOP_INCGVAR();
    bool JSOP_DECGVAR();
    bool JSOP_GVARINC();
    bool JSOP_GVARDEC();
    bool JSOP_REGEXP();
    bool JSOP_DEFXMLNS();
    bool JSOP_ANYNAME();
    bool JSOP_QNAMEPART();
    bool JSOP_QNAMECONST();
    bool JSOP_QNAME();
    bool JSOP_TOATTRNAME();
    bool JSOP_TOATTRVAL();
    bool JSOP_ADDATTRNAME();
    bool JSOP_ADDATTRVAL();
    bool JSOP_BINDXMLNAME();
    bool JSOP_SETXMLNAME();
    bool JSOP_XMLNAME();
    bool JSOP_DESCENDANTS();
    bool JSOP_FILTER();
    bool JSOP_ENDFILTER();
    bool JSOP_TOXML();
    bool JSOP_TOXMLLIST();
    bool JSOP_XMLTAGEXPR();
    bool JSOP_XMLELTEXPR();
    bool JSOP_XMLOBJECT();
    bool JSOP_XMLCDATA();
    bool JSOP_XMLCOMMENT();
    bool JSOP_XMLPI();
    bool JSOP_CALLPROP();
    bool JSOP_GETFUNNS();
    bool JSOP_UNUSED186();
    bool JSOP_DELDESC();
    bool JSOP_UINT24();
    bool JSOP_INDEXBASE();
    bool JSOP_RESETBASE();
    bool JSOP_RESETBASE0();
    bool JSOP_STARTXML();
    bool JSOP_STARTXMLEXPR();
    bool JSOP_CALLELEM();
    bool JSOP_STOP();
    bool JSOP_GETXPROP();
    bool JSOP_CALLXMLNAME();
    bool JSOP_TYPEOFEXPR();
    bool JSOP_ENTERBLOCK();
    bool JSOP_LEAVEBLOCK();
    bool JSOP_GETLOCAL();
    bool JSOP_SETLOCAL();
    bool JSOP_INCLOCAL();
    bool JSOP_DECLOCAL();
    bool JSOP_LOCALINC();
    bool JSOP_LOCALDEC();
    bool JSOP_FORLOCAL();
    bool JSOP_FORCONST();
    bool JSOP_ENDITER();
    bool JSOP_GENERATOR();
    bool JSOP_YIELD();
    bool JSOP_ARRAYPUSH();
    bool JSOP_UNUSED213();
    bool JSOP_ENUMCONSTELEM();
    bool JSOP_LEAVEBLOCKEXPR();
    bool JSOP_GETTHISPROP();
    bool JSOP_GETARGPROP();
    bool JSOP_GETVARPROP();
    bool JSOP_GETLOCALPROP();
    bool JSOP_INDEXBASE1();
    bool JSOP_INDEXBASE2();
    bool JSOP_INDEXBASE3();
    bool JSOP_CALLGVAR();
    bool JSOP_CALLVAR();
    bool JSOP_CALLARG();
    bool JSOP_CALLLOCAL();
    bool JSOP_INT8();
    bool JSOP_INT32();
    bool JSOP_LENGTH();
    bool JSOP_NEWARRAY();
    bool JSOP_HOLE();
};

FASTCALL jsdouble builtin_dmod(jsdouble a, jsdouble b);
FASTCALL jsval    builtin_BoxDouble(JSContext* cx, jsdouble d);
FASTCALL jsval    builtin_BoxInt32(JSContext* cx, jsint i);
FASTCALL jsdouble builtin_UnboxDouble(jsval v);
FASTCALL jsint    builtin_UnboxInt32(jsval v);
FASTCALL int32    builtin_doubleToInt32(jsdouble d);
FASTCALL int32    builtin_doubleToUint32(jsdouble d);

/*
 * Trace monitor. Every runtime is associated with a trace monitor that keeps
 * track of loop frequencies for all JavaScript code loaded into that runtime.
 * For this we use a loop table. Adjacent slots in the loop table, one for each
 * loop header in a given script, are requested using lock-free synchronization
 * from the runtime-wide loop table slot space, when the script is compiled.
 *
 * The loop table also doubles as trace tree pointer table once a loop achieves
 * a certain number of iterations and we recorded a tree for that loop.
 */
struct JSTraceMonitor {
    nanojit::Fragmento*     fragmento;
    TraceRecorder*          recorder;
};

#define TRACING_ENABLED(cx)       JS_HAS_OPTION(cx, JSOPTION_JIT)

extern bool
js_LoopEdge(JSContext* cx);

extern void
js_AbortRecording(JSContext* cx, const char* reason);

extern void
js_InitJIT(JSContext* cx);

#endif /* jstracer_h___ */
