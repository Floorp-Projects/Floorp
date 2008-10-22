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

#ifdef JS_TRACER

#include "jsstddef.h"
#include "jstypes.h"
#include "jslock.h"
#include "jsnum.h"
#include "jsinterp.h"
#include "jsbuiltins.h"

template <typename T>
class Queue : public GCObject {
    T* _data;
    unsigned _len;
    unsigned _max;
    
    void ensure(unsigned size) {
        while (_max < size) 
            _max <<= 1;
        _data = (T*)realloc(_data, _max * sizeof(T));
    }
public:
    Queue(unsigned max = 16) {
        this->_max = max;
        this->_len = 0;
        this->_data = (T*)malloc(max * sizeof(T));
    }
    
    ~Queue() {
        free(_data);
    }
    
    bool contains(T a) {
        for (unsigned n = 0; n < _len; ++n)
            if (_data[n] == a)
                return true;
        return false;
    }
    
    void add(T a) {
        ensure(_len + 1);
        JS_ASSERT(_len <= _max);
        _data[_len++] = a;
    }
    
    void add(T* chunk, unsigned size) {
        ensure(_len + size);
        JS_ASSERT(_len <= _max);
        memcpy(&_data[_len], chunk, size * sizeof(T));
        _len += size;
    }
    
    void addUnique(T a) {
        if (!contains(a))
            add(a);
    }
    
    void setLength(unsigned len) {
        ensure(len + 1);
        _len = len;
    }
    
    void clear() {
        _len = 0;
    }
    
    unsigned length() const {
        return _len;
    }

    T* data() const {
        return _data;
    }
};

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

/*
 * The oracle keeps track of slots that should not be demoted to int because we know them
 * to overflow or they result in type-unstable traces. We are using a simple hash table. 
 * Collisions lead to loss of optimization (demotable slots are not demoted) but have no
 * correctness implications.
 */
#define ORACLE_SIZE 4096

class Oracle {
    avmplus::BitSet _dontDemote;
public:
    void markGlobalSlotUndemotable(JSScript* script, unsigned slot);
    bool isGlobalSlotUndemotable(JSScript* script, unsigned slot) const;
    void markStackSlotUndemotable(JSScript* script, jsbytecode* ip, unsigned slot);
    bool isStackSlotUndemotable(JSScript* script, jsbytecode* ip, unsigned slot) const;
    void clear();
};

typedef Queue<uint16> SlotList;

class TypeMap : public Queue<uint8> {
public:
    void captureGlobalTypes(JSContext* cx, SlotList& slots);
    void captureStackTypes(JSContext* cx, unsigned callDepth);
    bool matches(TypeMap& other) const;
};

class TreeInfo MMGC_SUBCLASS_DECL {
    nanojit::Fragment*      fragment;
public:
    JSScript*               script;
    unsigned                maxNativeStackSlots;
    ptrdiff_t               nativeStackBase;
    unsigned                maxCallDepth;
    TypeMap                 stackTypeMap;
    unsigned                mismatchCount;
    Queue<nanojit::Fragment*> dependentTrees;
    unsigned                branchCount;
    Queue<nanojit::SideExit*> sideExits;

    TreeInfo(nanojit::Fragment* _fragment) { 
        fragment = _fragment;
    }
};

struct FrameInfo {
    JSObject*       callee;     // callee function object
    jsbytecode*     callpc;     // pc of JSOP_CALL in caller script
    uint8*          typemap;    // typemap for the stack frame
    union {
        struct {
            uint16  spdist;     // distance from fp->slots to fp->regs->sp at JSOP_CALL
            uint16  argc;       // actual argument count, may be < fun->nargs
        } s;
        uint32      word;       // for spdist/argc LIR store in record_JSOP_CALL
    };
};
 
class TraceRecorder : public GCObject {
    JSContext*              cx;
    JSTraceMonitor*         traceMonitor;
    JSObject*               globalObj;
    Tracker                 tracker;
    Tracker                 nativeFrameTracker;
    char*                   entryTypeMap;
    unsigned                callDepth;
    JSAtom**                atoms;
    nanojit::SideExit*      anchor;
    nanojit::Fragment*      fragment;
    TreeInfo*               treeInfo;
    nanojit::LirBuffer*     lirbuf;
    nanojit::LirWriter*     lir;
    nanojit::LirBufWriter*  lir_buf_writer;
    nanojit::LirWriter*     verbose_filter;
    nanojit::LirWriter*     cse_filter;
    nanojit::LirWriter*     expr_filter;
    nanojit::LirWriter*     func_filter;
#ifdef NJ_SOFTFLOAT
    nanojit::LirWriter*     float_filter;
#endif
    nanojit::LIns*          cx_ins;
    nanojit::LIns*          gp_ins;
    nanojit::LIns*          eos_ins;
    nanojit::LIns*          eor_ins;
    nanojit::LIns*          rval_ins;
    nanojit::LIns*          inner_sp_ins;
    bool                    deepAborted;
    bool                    applyingArguments;
    bool                    trashTree;
    nanojit::Fragment*      whichTreeToTrash;
    Queue<jsbytecode*>      cfgMerges;
    jsval*                  global_dslots;
    JSTraceableNative*      pendingTraceableNative;
    bool                    terminate;

    bool isGlobal(jsval* p) const;
    ptrdiff_t nativeGlobalOffset(jsval* p) const;
    ptrdiff_t nativeStackOffset(jsval* p) const;
    void import(nanojit::LIns* base, ptrdiff_t offset, jsval* p, uint8& t, 
                const char *prefix, uintN index, JSStackFrame *fp);
    void import(TreeInfo* treeInfo, nanojit::LIns* sp, unsigned ngslots, unsigned callDepth, 
                uint8* globalTypeMap, uint8* stackTypeMap);
    void trackNativeStackUse(unsigned slots);

    bool lazilyImportGlobalSlot(unsigned slot);

    nanojit::LIns* guard(bool expected, nanojit::LIns* cond, nanojit::ExitType exitType);
    nanojit::LIns* addName(nanojit::LIns* ins, const char* name);

    nanojit::LIns* get(jsval* p) const;
    nanojit::LIns* writeBack(nanojit::LIns* i, nanojit::LIns* base, ptrdiff_t offset);
    void set(jsval* p, nanojit::LIns* l, bool initializing = false);

    bool checkType(jsval& v, uint8 type, bool& recompile);
    bool verifyTypeStability();

    jsval& argval(unsigned n) const;
    jsval& varval(unsigned n) const;
    jsval& stackval(int n) const;

    nanojit::LIns* scopeChain() const;
    bool activeCallOrGlobalSlot(JSObject* obj, jsval*& vp);

    nanojit::LIns* arg(unsigned n);
    void arg(unsigned n, nanojit::LIns* i);
    nanojit::LIns* var(unsigned n);
    void var(unsigned n, nanojit::LIns* i);
    nanojit::LIns* stack(int n);
    void stack(int n, nanojit::LIns* i);

    nanojit::LIns* f2i(nanojit::LIns* f);
    nanojit::LIns* makeNumberInt32(nanojit::LIns* f);
    
    bool ifop();
    bool switchop();
    bool inc(jsval& v, jsint incr, bool pre = true);
    bool inc(jsval& v, nanojit::LIns*& v_ins, jsint incr, bool pre = true);
    bool incProp(jsint incr, bool pre = true);
    bool incElem(jsint incr, bool pre = true);
    bool incName(jsint incr, bool pre = true);

    enum { CMP_NEGATE = 1, CMP_TRY_BRANCH_AFTER_COND = 2, CMP_CASE = 4, CMP_STRICT = 8 };
    bool cmp(nanojit::LOpcode op, int flags = 0);

    bool unary(nanojit::LOpcode op);
    bool binary(nanojit::LOpcode op);

    bool ibinary(nanojit::LOpcode op);
    bool iunary(nanojit::LOpcode op);
    bool bbinary(nanojit::LOpcode op);
    void demote(jsval& v, jsdouble result);

    bool map_is_native(JSObjectMap* map, nanojit::LIns* map_ins, nanojit::LIns*& ops_ins,
                       size_t op_offset = 0);
    bool test_property_cache(JSObject* obj, nanojit::LIns* obj_ins, JSObject*& obj2,
                             jsuword& pcval);
    bool test_property_cache_direct_slot(JSObject* obj, nanojit::LIns* obj_ins, uint32& slot);
    void stobj_set_slot(nanojit::LIns* obj_ins, unsigned slot,
                        nanojit::LIns*& dslots_ins, nanojit::LIns* v_ins);
    nanojit::LIns* stobj_get_fslot(nanojit::LIns* obj_ins, unsigned slot);
    nanojit::LIns* stobj_get_slot(nanojit::LIns* obj_ins, unsigned slot,
                                  nanojit::LIns*& dslots_ins);
    bool native_set(nanojit::LIns* obj_ins, JSScopeProperty* sprop,
                    nanojit::LIns*& dslots_ins, nanojit::LIns* v_ins);
    bool native_get(nanojit::LIns* obj_ins, nanojit::LIns* pobj_ins, JSScopeProperty* sprop,
                    nanojit::LIns*& dslots_ins, nanojit::LIns*& v_ins);
    
    bool name(jsval*& vp);
    bool prop(JSObject* obj, nanojit::LIns* obj_ins, uint32& slot, nanojit::LIns*& v_ins);
    bool elem(jsval& oval, jsval& idx, jsval*& vp, nanojit::LIns*& v_ins, nanojit::LIns*& addr_ins);

    bool getProp(JSObject* obj, nanojit::LIns* obj_ins);
    bool getProp(jsval& v);
    bool getThis(nanojit::LIns*& this_ins);

    bool box_jsval(jsval v, nanojit::LIns*& v_ins);
    bool unbox_jsval(jsval v, nanojit::LIns*& v_ins);
    bool guardClass(JSObject* obj, nanojit::LIns* obj_ins, JSClass* clasp);
    bool guardDenseArray(JSObject* obj, nanojit::LIns* obj_ins);
    bool guardDenseArrayIndex(JSObject* obj, jsint idx, nanojit::LIns* obj_ins,
                              nanojit::LIns* dslots_ins, nanojit::LIns* idx_ins);
    bool guardElemOp(JSObject* obj, nanojit::LIns* obj_ins, jsid id, size_t op_offset, jsval* vp);
    void clearFrameSlotsFromCache();
    bool guardShapelessCallee(jsval& callee);
    bool interpretedFunctionCall(jsval& fval, JSFunction* fun, uintN argc, bool constructing);
    bool functionCall(bool constructing);
    bool forInLoop(jsval* vp);

    void trackCfgMerges(jsbytecode* pc);
    void flipIf(jsbytecode* pc, bool& cond);
    void fuseIf(jsbytecode* pc, bool cond, nanojit::LIns* x);

public:
    friend bool js_MonitorRecording(TraceRecorder* tr);

    TraceRecorder(JSContext* cx, nanojit::SideExit*, nanojit::Fragment*, TreeInfo*,
            unsigned ngslots, uint8* globalTypeMap, uint8* stackTypeMap, 
            nanojit::SideExit* expectedInnerExit);
    ~TraceRecorder();

    uint8 determineSlotType(jsval* vp) const;
    nanojit::LIns* snapshot(nanojit::ExitType exitType);
    nanojit::Fragment* getFragment() const { return fragment; }
    bool isLoopHeader(JSContext* cx) const;
    void compile(nanojit::Fragmento* fragmento);
    void closeLoop(nanojit::Fragmento* fragmento);
    void endLoop(nanojit::Fragmento* fragmento);
    void blacklist() { fragment->blacklist(); }
    bool adjustCallerTypes(nanojit::Fragment* f);
    bool selectCallablePeerFragment(nanojit::Fragment** first);
    void prepareTreeCall(nanojit::Fragment* inner);
    void emitTreeCall(nanojit::Fragment* inner, nanojit::SideExit* exit);
    unsigned getCallDepth() const;
    
    bool record_EnterFrame();
    bool record_LeaveFrame();
    bool record_SetPropHit(JSPropCacheEntry* entry, JSScopeProperty* sprop);
    bool record_SetPropMiss(JSPropCacheEntry* entry);
    bool record_DefLocalFunSetSlot(uint32 slot, JSObject* obj);
    bool record_FastNativeCallComplete();
    
    void deepAbort() { deepAborted = true; }
    bool wasDeepAborted() { return deepAborted; }
    bool walkedOutOfLoop() { return terminate; }
    
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format)               \
    bool record_##op();
# include "jsopcode.tbl"
#undef OPDEF
};

#define TRACING_ENABLED(cx)       JS_HAS_OPTION(cx, JSOPTION_JIT)
#define TRACE_RECORDER(cx)        (JS_TRACE_MONITOR(cx).recorder)
#define SET_TRACE_RECORDER(cx,tr) (JS_TRACE_MONITOR(cx).recorder = (tr))

// See jsinterp.cpp for the ENABLE_TRACER definition.
#define RECORD_ARGS(x,args)                                                   \
    JS_BEGIN_MACRO                                                            \
        TraceRecorder* tr_ = TRACE_RECORDER(cx);                              \
        if (!js_MonitorRecording(tr_))                                        \
            ENABLE_TRACER(0);                                                 \
        else                                                                  \
            TRACE_ARGS_(tr_,x,args);                                          \
    JS_END_MACRO

#define TRACE_ARGS_(tr,x,args)                                                \
    JS_BEGIN_MACRO                                                            \
        if (!tr->record_##x args) {                                           \
            js_AbortRecording(cx, #x);                                        \
            ENABLE_TRACER(0);                                                 \
        }                                                                     \
    JS_END_MACRO

#define TRACE_ARGS(x,args)                                                    \
    JS_BEGIN_MACRO                                                            \
        TraceRecorder* tr_ = TRACE_RECORDER(cx);                              \
        if (tr_)                                                              \
            TRACE_ARGS_(tr_, x, args);                                        \
    JS_END_MACRO

#define RECORD(x)               RECORD_ARGS(x, ())
#define TRACE_0(x)              TRACE_ARGS(x, ())
#define TRACE_1(x,a)            TRACE_ARGS(x, (a))
#define TRACE_2(x,a,b)          TRACE_ARGS(x, (a, b))

extern bool
js_MonitorLoopEdge(JSContext* cx, uintN& inlineCallCount);

extern bool
js_MonitorRecording(TraceRecorder *tr);

extern void
js_AbortRecording(JSContext* cx, const char* reason);

extern void
js_InitJIT(JSTraceMonitor *tm);

extern void
js_FinishJIT(JSTraceMonitor *tm);

extern void
js_FlushJITCache(JSContext* cx);

extern void
js_FlushJITOracle(JSContext* cx);

#else  /* !JS_TRACER */

#define RECORD(x)               ((void)0)
#define TRACE_0(x)              ((void)0)
#define TRACE_1(x,a)            ((void)0)
#define TRACE_2(x,a,b)          ((void)0)

#endif /* !JS_TRACER */

#endif /* jstracer_h___ */
