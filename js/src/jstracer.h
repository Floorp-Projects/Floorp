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
 *   Mike Shaver <shaver@mozilla.org>
 *   David Anderson <danderson@mozilla.com>
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
class Tracker {
    struct Page {
        struct Page*    next;
        jsuword         base;
        nanojit::LIns*  map[1];
    };
    struct Page* pagelist;

    jsuword         getPageBase(const void* v) const;
    struct Page*    findPage(const void* v) const;
    struct Page*    addPage(const void* v);
public:
    Tracker();
    ~Tracker();

    bool            has(const void* v) const;
    nanojit::LIns*  get(const void* v) const;
    void            set(const void* v, nanojit::LIns* ins);
    void            clear();
};

class TreeInfo {
public:
    TreeInfo() {
        typeMap = NULL;
        gslots = NULL;
    }

    virtual ~TreeInfo() {
        if (typeMap) free(typeMap);
        if (gslots) free(gslots);
    }
    
    struct JSStackFrame*    entryFrame;
    struct JSFrameRegs      entryRegs;
    unsigned                entryNativeFrameSlots;
    unsigned                maxNativeFrameSlots;
    size_t                  nativeStackBase;
    unsigned                maxCallDepth;
    uint32                  globalShape;
    unsigned                ngslots;
    uint8                  *typeMap;
    uint16                 *gslots;
};

extern struct nanojit::CallInfo builtins[];

#define TYPEMAP_GET_TYPE(x)         ((x) & JSVAL_TAGMASK)
#define TYPEMAP_SET_TYPE(x, t)      (x = (x & 0xf0) | t)
#define TYPEMAP_GET_FLAG(x, flag)   ((x) & flag)
#define TYPEMAP_SET_FLAG(x, flag)   do { (x) |= flag; } while (0)

#define TYPEMAP_TYPE_ANY            7

#define TYPEMAP_FLAG_DEMOTE 0x10 /* try to record as int */
#define TYPEMAP_FLAG_DONT_DEMOTE 0x20 /* do not try to record as int */

class TraceRecorder {
    JSContext*              cx;
    JSObject*               globalObj;
    Tracker                 tracker;
    char*                   entryTypeMap;
    struct JSStackFrame*    entryFrame;
    struct JSFrameRegs*     entryRegs;
    unsigned                callDepth;
    JSAtom**                atoms;
    nanojit::GuardRecord*   anchor;
    nanojit::Fragment*      fragment;
    TreeInfo*               treeInfo;
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

    size_t nativeFrameOffset(jsval* p) const;
    void import(jsval* p, uint8& t, const char *prefix, int index, jsuword* localNames);
    void trackNativeFrameUse(unsigned slots);

    unsigned getCallDepth() const;
    nanojit::LIns* guard(bool expected, nanojit::LIns* cond);
    nanojit::LIns* addName(nanojit::LIns* ins, const char* name);

    nanojit::LIns* get(jsval* p);
    void set(jsval* p, nanojit::LIns* l, bool initializing = false);

    bool checkType(jsval& v, uint8& type);
    bool verifyTypeStability(uint8* m);

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

    bool ifop();
    bool inc(jsval& v, jsint incr, bool pre = true);
    bool inc(jsval& v, nanojit::LIns*& v_ins, jsint incr, bool pre = true);
    bool incProp(jsint incr, bool pre = true);
    bool incElem(jsint incr, bool pre = true);
    bool cmp(nanojit::LOpcode op, bool negate = false);

    bool unary(nanojit::LOpcode op);
    bool binary(nanojit::LOpcode op);

    bool ibinary(nanojit::LOpcode op);
    bool iunary(nanojit::LOpcode op);
    bool bbinary(nanojit::LOpcode op);
    void demote(jsval& v, jsdouble result);

    bool map_is_native(JSObjectMap* map, nanojit::LIns* map_ins);
    bool test_property_cache(JSObject* obj, nanojit::LIns* obj_ins, JSObject*& obj2, jsuword& pcval);
    bool test_property_cache_direct_slot(JSObject* obj, nanojit::LIns* obj_ins, uint32& slot);
    void stobj_set_slot(nanojit::LIns* obj_ins, unsigned slot,
                        nanojit::LIns*& dslots_ins, nanojit::LIns* v_ins);
    nanojit::LIns* stobj_get_slot(nanojit::LIns* obj_ins, unsigned slot,
                                  nanojit::LIns*& dslots_ins);
    bool native_set(nanojit::LIns* obj_ins, JSScopeProperty* sprop,
                    nanojit::LIns*& dslots_ins, nanojit::LIns* v_ins);
    bool native_get(nanojit::LIns* obj_ins, nanojit::LIns* pobj_ins, JSScopeProperty* sprop,
                    nanojit::LIns*& dslots_ins, nanojit::LIns*& v_ins);

    bool prop(JSObject* obj, nanojit::LIns* obj_ins, uint32& slot, nanojit::LIns*& v_ins);
    bool elem(jsval& l, jsval& r, jsval*& vp, nanojit::LIns*& v_ins, nanojit::LIns*& addr_ins);

    bool getProp(JSObject* obj, nanojit::LIns* obj_ins);
    bool getProp(jsval& v);
    bool getThis(nanojit::LIns*& this_ins);
    
    bool box_jsval(jsval v, nanojit::LIns*& v_ins);
    bool unbox_jsval(jsval v, nanojit::LIns*& v_ins);
    bool guardThatObjectHasClass(JSObject* obj, nanojit::LIns* obj_ins,
                                 JSClass* cls, nanojit::LIns*& dslots_ins);
    bool guardThatObjectIsDenseArray(JSObject* obj, nanojit::LIns* obj_ins,
                                     nanojit::LIns*& dslots_ins);
    bool guardDenseArrayIndexWithinBounds(JSObject* obj, jsint idx, nanojit::LIns* obj_ins,
                                          nanojit::LIns*& dslots_ins, nanojit::LIns* idx_ins);
public:
    TraceRecorder(JSContext* cx, nanojit::GuardRecord*, nanojit::Fragment*, uint8* typemap);
    ~TraceRecorder();

    nanojit::SideExit* snapshot();
    nanojit::Fragment* getFragment() const { return fragment; }
    void closeLoop(nanojit::Fragmento* fragmento);
    
    bool record_EnterFrame();
    
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format)               \
    bool record_##op();
# include "jsopcode.tbl"
#undef OPDEF
};

#define TRACING_ENABLED(cx)       JS_HAS_OPTION(cx, JSOPTION_JIT)

#define RECORD(x)                                                             \
    JS_BEGIN_MACRO                                                            \
        TraceRecorder* r = JS_TRACE_MONITOR(cx).recorder;                     \
        if (!r->record_##x()) {                                               \
            js_AbortRecording(cx, NULL, #x);                                  \
            ENABLE_TRACER(0);                                                 \
        }                                                                     \
    JS_END_MACRO

extern bool
js_LoopEdge(JSContext* cx, jsbytecode* oldpc);

extern void
js_AbortRecording(JSContext* cx, jsbytecode* abortpc, const char* reason);

extern void
js_InitJIT(JSContext* cx);

extern void
js_DestroyJIT(JSContext* cx);

#endif /* jstracer_h___ */
