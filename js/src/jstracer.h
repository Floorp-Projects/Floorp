/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
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
 * trigger a side exit. The address is so low that it should never be actually
 * in use. If it is, a performance regression occurs, not an actual runtime
 * error.
 */
#define JSVAL_ERROR_COOKIE OBJECT_TO_JSVAL((void*)0x10)

/*
 * We also need a magic unboxed 32-bit integer that signals an error.  Again if
 * this number is hit we experience a performance regression, not a runtime
 * error.
 */
#define INT32_ERROR_COOKIE 0xffffabcd

/*
 * Tracker is used to keep track of values being manipulated by the interpreter
 * during trace recording.
 */
template <typename T>
class Tracker {
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
#define TYPEMAP_SET_TYPE(x, t)      (x = (x & 0xf0) | t)
#define TYPEMAP_GET_FLAG(x, flag)   ((x) & flag)
#define TYPEMAP_SET_FLAG(x, flag)   do { (x) |= flag; } while (0)

#define TYPEMAP_TYPE_ANY            7

#define TYPEMAP_FLAG_DEMOTE 0x10 /* try to record as int */
#define TYPEMAP_FLAG_DONT_DEMOTE 0x20 /* do not try to record as int */

class TraceRecorder {
    JSContext*              cx;
    JSStackFrame*           global;
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
    bool isGlobal(void* p) const;
    unsigned nativeFrameSlots(JSStackFrame* fp, JSFrameRegs& regs) const;
    size_t nativeFrameOffset(void* p) const;
    void import(jsval* p, uint8& t, char *prefix, int index);
    void trackNativeFrameUse(unsigned slots);

    unsigned getCallDepth() const;
    void guard(bool expected, nanojit::LIns* cond);

    void set(void* p, nanojit::LIns* l);

    bool checkType(jsval& v, uint8& type);
    bool verifyTypeStability(JSStackFrame* fp, JSFrameRegs& regs, uint8* m);
    void closeLoop(nanojit::Fragmento* fragmento);

    jsval& gvarval(unsigned n) const;
    jsval& argval(unsigned n) const;
    jsval& varval(unsigned n) const;
    jsval& stackval(int n) const;

    nanojit::LIns* gvar(unsigned n);
    void gvar(unsigned n, nanojit::LIns* i);
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
    bool guardThatObjectIsDenseArray(JSObject* obj, nanojit::LIns* obj_ins,
                                     nanojit::LIns*& dslots_ins);
    bool guardDenseArrayIndexWithinBounds(JSObject* obj, jsint idx, nanojit::LIns* obj_ins,
                                          nanojit::LIns*& dslots_ins, nanojit::LIns* idx_ins);
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

#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format)               \
    bool op();
# include "jsopcode.tbl"
#undef OPDEF
};

FASTCALL jsdouble builtin_dmod(jsdouble a, jsdouble b);
FASTCALL jsval    builtin_BoxDouble(JSContext* cx, jsdouble d);
FASTCALL jsval    builtin_BoxInt32(JSContext* cx, jsint i);
FASTCALL jsdouble builtin_UnboxDouble(jsval v);
FASTCALL jsint    builtin_UnboxInt32(jsval v);
FASTCALL int32    builtin_doubleToInt32(jsdouble d);
FASTCALL int32    builtin_doubleToUint32(jsdouble d);

/*
 * Trace monitor. Every JSThread (if JS_THREADSAFE) or JSRuntime (if not
 * JS_THREADSAFE) has an associated trace monitor that keeps track of loop
 * frequencies for all JavaScript code loaded into that runtime.
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
