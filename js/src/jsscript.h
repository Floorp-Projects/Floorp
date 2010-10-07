/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef jsscript_h___
#define jsscript_h___
/*
 * JS script descriptor.
 */
#include "jsatom.h"
#include "jsprvtd.h"
#include "jsdbgapi.h"
#include "jsclist.h"

/*
 * Type of try note associated with each catch or finally block, and also with
 * for-in loops.
 */
typedef enum JSTryNoteKind {
    JSTRY_CATCH,
    JSTRY_FINALLY,
    JSTRY_ITER
} JSTryNoteKind;

namespace js {

/*
 * Indicates a location in the stack that an upvar value can be retrieved from
 * as a two tuple of (level, slot).
 *
 * Some existing client code uses the level value as a delta, or level "skip"
 * quantity. We could probably document that through use of more types at some
 * point in the future.
 *
 * Existing XDR code wants this to be backed by a 32b integer for serialization,
 * so we oblige.
 *
 * TODO: consider giving more bits to the slot value and takings ome from the level.
 */
class UpvarCookie 
{
    uint32 value;

    static const uint32 FREE_VALUE = 0xfffffffful;

    void checkInvariants() {
        JS_STATIC_ASSERT(sizeof(UpvarCookie) == sizeof(uint32));
        JS_STATIC_ASSERT(UPVAR_LEVEL_LIMIT < FREE_LEVEL);
    }

  public:
    /*
     * All levels above-and-including FREE_LEVEL are reserved so that
     * FREE_VALUE can be used as a special value.
     */
    static const uint16 FREE_LEVEL = 0x3fff;

    /*
     * If a function has a higher static level than this limit, we will not
     * optimize it using UPVAR opcodes.
     */
    static const uint16 UPVAR_LEVEL_LIMIT = 16;
    static const uint16 CALLEE_SLOT = 0xffff;
    static bool isLevelReserved(uint16 level) { return level >= FREE_LEVEL; }

    bool isFree() const { return value == FREE_VALUE; }
    uint32 asInteger() const { return value; }
    /* isFree check should be performed before using these accessors. */
    uint16 level() const { JS_ASSERT(!isFree()); return uint16(value >> 16); }
    uint16 slot() const { JS_ASSERT(!isFree()); return uint16(value); }

    void set(const UpvarCookie &other) { set(other.level(), other.slot()); }
    void set(uint16 newLevel, uint16 newSlot) { value = (uint32(newLevel) << 16) | newSlot; }
    void makeFree() { set(0xffff, 0xffff); JS_ASSERT(isFree()); }
    void fromInteger(uint32 u32) { value = u32; }
};

}

/*
 * Exception handling record.
 */
struct JSTryNote {
    uint8           kind;       /* one of JSTryNoteKind */
    uint8           padding;    /* explicit padding on uint16 boundary */
    uint16          stackDepth; /* stack depth upon exception handler entry */
    uint32          start;      /* start of the try statement or for-in loop
                                   relative to script->main */
    uint32          length;     /* length of the try statement or for-in loop */
};

typedef struct JSTryNoteArray {
    JSTryNote       *vector;    /* array of indexed try notes */
    uint32          length;     /* count of indexed try notes */
} JSTryNoteArray;

typedef struct JSObjectArray {
    JSObject        **vector;   /* array of indexed objects */
    uint32          length;     /* count of indexed objects */
} JSObjectArray;

typedef struct JSUpvarArray {
    js::UpvarCookie *vector;    /* array of indexed upvar cookies */
    uint32          length;     /* count of indexed upvar cookies */
} JSUpvarArray;

typedef struct JSConstArray {
    js::Value       *vector;    /* array of indexed constant values */
    uint32          length;
} JSConstArray;

namespace js {

struct GlobalSlotArray {
    struct Entry {
        uint32      atomIndex;  /* index into atom table */
        uint32      slot;       /* global obj slot number */
    };
    Entry           *vector;
    uint32          length;
};

} /* namespace js */

#define JS_OBJECT_ARRAY_SIZE(length)                                          \
    (offsetof(JSObjectArray, vector) + sizeof(JSObject *) * (length))

#if defined DEBUG && defined JS_THREADSAFE
# define CHECK_SCRIPT_OWNER 1
#endif

#ifdef JS_METHODJIT
namespace JSC {
    class ExecutablePool;
}

#define JS_UNJITTABLE_SCRIPT (reinterpret_cast<void*>(1))

enum JITScriptStatus {
    JITScript_None,
    JITScript_Invalid,
    JITScript_Valid
};

namespace js {
namespace mjit {

struct JITScript;

}
}
#endif

struct JSScript {
    /*
     * Two successively less primitive ways to make a new JSScript.  The first
     * does *not* call a non-null cx->runtime->newScriptHook -- only the second,
     * NewScriptFromCG, calls this optional debugger hook.
     *
     * The NewScript function can't know whether the script it creates belongs
     * to a function, or is top-level or eval code, but the debugger wants access
     * to the newly made script's function, if any -- so callers of NewScript
     * are responsible for notifying the debugger after successfully creating any
     * kind (function or other) of new JSScript.
     *
     * NB: NewScript always creates a new script; it never returns the empty
     * script singleton (JSScript::emptyScript()). Callers who know they can use
     * that read-only singleton are responsible for choosing it instead of calling
     * NewScript with length and nsrcnotes equal to 1 and other parameters save
     * cx all zero.
     */
    static JSScript *NewScript(JSContext *cx, uint32 length, uint32 nsrcnotes, uint32 natoms,
                               uint32 nobjects, uint32 nupvars, uint32 nregexps,
                               uint32 ntrynotes, uint32 nconsts, uint32 nglobals,
                               uint32 nClosedArgs, uint32 nClosedVars);

    static JSScript *NewScriptFromCG(JSContext *cx, JSCodeGenerator *cg);

    /* FIXME: bug 586181 */
    JSCList         links;      /* Links for compartment script list */
    jsbytecode      *code;      /* bytecodes and their immediate operands */
    uint32          length;     /* length of code vector */
    uint16          version;    /* JS version under which script was compiled */
    uint16          nfixed;     /* number of slots besides stack operands in
                                   slot array */
    uint8           objectsOffset;  /* offset to the array of nested function,
                                       block, scope, xml and one-time regexps
                                       objects or 0 if none */
    uint8           upvarsOffset;   /* offset of the array of display ("up")
                                       closure vars or 0 if none */
    uint8           regexpsOffset;  /* offset to the array of to-be-cloned
                                       regexps or 0 if none. */
    uint8           trynotesOffset; /* offset to the array of try notes or
                                       0 if none */
    uint8           globalsOffset;  /* offset to the array of global slots or
                                       0 if none */
    uint8           constOffset;    /* offset to the array of constants or
                                       0 if none */
    bool            noScriptRval:1; /* no need for result value of last
                                       expression statement */
    bool            savedCallerFun:1; /* object 0 is caller function */
    bool            hasSharps:1;      /* script uses sharp variables */
    bool            strictModeCode:1; /* code is in strict mode */
    bool            compileAndGo:1;   /* script was compiled with TCF_COMPILE_N_GO */
    bool            usesEval:1;       /* script uses eval() */
    bool            usesArguments:1;  /* script uses arguments */
    bool            warnedAboutTwoArgumentEval:1; /* have warned about use of
                                                     obsolete eval(s, o) in
                                                     this script */
#ifdef JS_METHODJIT
    bool            debugMode:1;      /* script was compiled in debug mode */
#endif

    jsbytecode      *main;      /* main entry point, after predef'ing prolog */
    JSAtomMap       atomMap;    /* maps immediate index to literal struct */
    const char      *filename;  /* source filename or null */
    uint32          lineno;     /* base line number of script */
    uint16          nslots;     /* vars plus maximum stack depth */
    uint16          staticLevel;/* static level for display maintenance */
    uint16          nClosedArgs; /* number of args which are closed over. */
    uint16          nClosedVars; /* number of vars which are closed over. */
    JSPrincipals    *principals;/* principals for this script */
    union {
        /*
         * A script object of class js_ScriptClass, to ensure the script is GC'd.
         * - All scripts returned by JSAPI functions (JS_CompileScript,
         *   JS_CompileFile, etc.) have these objects.
         * - Function scripts never have script objects; such scripts are owned
         *   by their function objects.
         * - Temporary scripts created by obj_eval, JS_EvaluateScript, and 
         *   similar functions never have these objects; such scripts are
         *   explicitly destroyed by the code that created them.
         * Debugging API functions (JSDebugHooks::newScriptHook;
         * JS_GetFunctionScript) may reveal sans-script-object Function and
         * temporary scripts to clients, but clients must never call
         * JS_NewScriptObject on such scripts: doing so would double-free them,
         * once from the explicit call to js_DestroyScript, and once when the
         * script object is garbage collected.
         */
        JSObject    *object;
        JSScript    *nextToGC;  /* next to GC in rt->scriptsToGC list */
    } u;
#ifdef CHECK_SCRIPT_OWNER
    JSThread        *owner;     /* for thread-safe life-cycle assertions */
#endif

    uint32          *closedSlots; /* vector of closed slots; args first, then vars. */

  public:
#ifdef JS_METHODJIT
    // Fast-cached pointers to make calls faster. These are also used to
    // quickly test whether there is JIT code; a NULL value means no
    // compilation has been attempted. A JS_UNJITTABLE_SCRIPT value means
    // compilation failed. Any value is the arity-check entry point.
    void *jitArityCheckNormal;
    void *jitArityCheckCtor;

    js::mjit::JITScript *jitNormal;   /* Extra JIT info for normal scripts */
    js::mjit::JITScript *jitCtor;     /* Extra JIT info for constructors */

    void **nmapNormal;
    void **nmapCtor;

    bool hasJITCode() {
        return jitNormal || jitCtor;
    }

    void setNativeMap(bool constructing, void **map) {
        if (constructing)
            nmapCtor = map;
        else
            nmapNormal = map;
    }

    void **maybeNativeMap(bool constructing) {
        return constructing ? nmapCtor : nmapNormal;
    }

    void **nativeMap(bool constructing) {
        void **nmap = maybeNativeMap(constructing);
        JS_ASSERT(nmap);
        return nmap;
    }

    void *maybeNativeCodeForPC(bool constructing, jsbytecode *pc) {
        void **nmap = maybeNativeMap(constructing);
        if (!nmap)
            return NULL;
        JS_ASSERT(pc >= code && pc < code + length);
        return nmap[pc - code];
    }

    void *nativeCodeForPC(bool constructing, jsbytecode *pc) {
        void **nmap = nativeMap(constructing);
        JS_ASSERT(pc >= code && pc < code + length);
        JS_ASSERT(nmap[pc - code]);
        return nmap[pc - code];
    }

    js::mjit::JITScript *getJIT(bool constructing) {
        return constructing ? jitCtor : jitNormal;
    }

    JITScriptStatus getJITStatus(bool constructing) {
        void *addr = constructing ? jitArityCheckCtor : jitArityCheckNormal;
        if (addr == NULL)
            return JITScript_None;
        if (addr == JS_UNJITTABLE_SCRIPT)
            return JITScript_Invalid;
        return JITScript_Valid;
    }
#endif

    /* Script notes are allocated right after the code. */
    jssrcnote *notes() { return (jssrcnote *)(code + length); }

    JSObjectArray *objects() {
        JS_ASSERT(objectsOffset != 0);
        return (JSObjectArray *)((uint8 *) this + objectsOffset);
    }

    JSUpvarArray *upvars() {
        JS_ASSERT(upvarsOffset != 0);
        return (JSUpvarArray *) ((uint8 *) this + upvarsOffset);
    }

    JSObjectArray *regexps() {
        JS_ASSERT(regexpsOffset != 0);
        return (JSObjectArray *) ((uint8 *) this + regexpsOffset);
    }

    JSTryNoteArray *trynotes() {
        JS_ASSERT(trynotesOffset != 0);
        return (JSTryNoteArray *) ((uint8 *) this + trynotesOffset);
    }

    js::GlobalSlotArray *globals() {
        JS_ASSERT(globalsOffset != 0);
        return (js::GlobalSlotArray *) ((uint8 *)this + globalsOffset);
    }

    JSConstArray *consts() {
        JS_ASSERT(constOffset != 0);
        return (JSConstArray *) ((uint8 *) this + constOffset);
    }

    JSAtom *getAtom(size_t index) {
        JS_ASSERT(index < atomMap.length);
        return atomMap.vector[index];
    }

    JSObject *getObject(size_t index) {
        JSObjectArray *arr = objects();
        JS_ASSERT(index < arr->length);
        return arr->vector[index];
    }

    uint32 getGlobalSlot(size_t index) {
        js::GlobalSlotArray *arr = globals();
        JS_ASSERT(index < arr->length);
        return arr->vector[index].slot;
    }

    JSAtom *getGlobalAtom(size_t index) {
        js::GlobalSlotArray *arr = globals();
        JS_ASSERT(index < arr->length);
        return getAtom(arr->vector[index].atomIndex);
    }

    JSVersion getVersion() const {
        return JSVersion(version);
    }

    void setVersion(JSVersion newVersion) {
        JS_ASSERT((newVersion & JS_BITMASK(16)) == uint32(newVersion));
        version = newVersion;
    }

    inline JSFunction *getFunction(size_t index);

    inline JSObject *getRegExp(size_t index);

    const js::Value &getConst(size_t index) {
        JSConstArray *arr = consts();
        JS_ASSERT(index < arr->length);
        return arr->vector[index];
    }

    /*
     * The isEmpty method tells whether this script has code that computes any
     * result (not return value, result AKA normal completion value) other than
     * JSVAL_VOID, or any other effects. It has a fast path for the case where
     * |this| is the emptyScript singleton, but it also checks this->length and
     * this->code, to handle debugger-generated mutable empty scripts.
     */
    inline bool isEmpty() const;

    /*
     * Accessor for the emptyScriptConst singleton, to consolidate const_cast.
     * See the private member declaration.
     */
    static JSScript *emptyScript() {
        return const_cast<JSScript *>(&emptyScriptConst);
    }

    uint32 getClosedArg(uint32 index) {
        JS_ASSERT(index < nClosedArgs);
        return closedSlots[index];
    }

    uint32 getClosedVar(uint32 index) {
        JS_ASSERT(index < nClosedVars);
        return closedSlots[nClosedArgs + index];
    }

    void copyClosedSlotsTo(JSScript *other);

  private:
    /*
     * Use const to put this in read-only memory if possible. We are stuck with
     * non-const JSScript * and jsbytecode * by legacy code (back in the 1990s,
     * const wasn't supported correctly on all target platforms). The debugger
     * does mutate bytecode, and script->u.object may be set after construction
     * in some cases, so making JSScript pointers const will be "hard".
     */
    static const JSScript emptyScriptConst;
};

#define SHARP_NSLOTS            2       /* [#array, #depth] slots if the script
                                           uses sharp variables */

static JS_INLINE uintN
StackDepth(JSScript *script)
{
    return script->nslots - script->nfixed;
}

/*
 * If pc_ does not point within script_'s bytecode, then it must point into an
 * imacro body, so we use cx->runtime common atoms instead of script_'s atoms.
 * This macro uses cx from its callers' environments in the pc-in-imacro case.
 */
#define JS_GET_SCRIPT_ATOM(script_, pc_, index, atom)                         \
    JS_BEGIN_MACRO                                                            \
        if ((pc_) < (script_)->code ||                                        \
            (script_)->code + (script_)->length <= (pc_)) {                   \
            JS_ASSERT((size_t)(index) < js_common_atom_count);                \
            (atom) = COMMON_ATOMS_START(&cx->runtime->atomState)[index];      \
        } else {                                                              \
            (atom) = script_->getAtom(index);                                 \
        }                                                                     \
    JS_END_MACRO

extern JS_FRIEND_DATA(js::Class) js_ScriptClass;

extern JSObject *
js_InitScriptClass(JSContext *cx, JSObject *obj);

/*
 * On first new context in rt, initialize script runtime state, specifically
 * the script filename table and its lock.
 */
extern JSBool
js_InitRuntimeScriptState(JSRuntime *rt);

/*
 * On JS_DestroyRuntime(rt), forcibly free script filename prefixes and any
 * script filename table entries that have not been GC'd.
 *
 * This allows script filename prefixes to outlive any context in rt.
 */
extern void
js_FreeRuntimeScriptState(JSRuntime *rt);

extern const char *
js_SaveScriptFilename(JSContext *cx, const char *filename);

extern const char *
js_SaveScriptFilenameRT(JSRuntime *rt, const char *filename, uint32 flags);

extern uint32
js_GetScriptFilenameFlags(const char *filename);

extern void
js_MarkScriptFilename(const char *filename);

extern void
js_MarkScriptFilenames(JSRuntime *rt);

extern void
js_SweepScriptFilenames(JSRuntime *rt);

/*
 * New-script-hook calling is factored from js_NewScriptFromCG so that it
 * and callers of js_XDRScript can share this code.  In the case of callers
 * of js_XDRScript, the hook should be invoked only after successful decode
 * of any owning function (the fun parameter) or script object (null fun).
 */
extern JS_FRIEND_API(void)
js_CallNewScriptHook(JSContext *cx, JSScript *script, JSFunction *fun);

extern JS_FRIEND_API(void)
js_CallDestroyScriptHook(JSContext *cx, JSScript *script);

extern void
js_DestroyScript(JSContext *cx, JSScript *script);

extern void
js_TraceScript(JSTracer *trc, JSScript *script);

extern JSBool
js_NewScriptObject(JSContext *cx, JSScript *script);

/*
 * To perturb as little code as possible, we introduce a js_GetSrcNote lookup
 * cache without adding an explicit cx parameter.  Thus js_GetSrcNote becomes
 * a macro that uses cx from its calls' lexical environments.
 */
#define js_GetSrcNote(script,pc) js_GetSrcNoteCached(cx, script, pc)

extern jssrcnote *
js_GetSrcNoteCached(JSContext *cx, JSScript *script, jsbytecode *pc);

/*
 * NOTE: use js_FramePCToLineNumber(cx, fp) when you have an active fp, in
 * preference to js_PCToLineNumber (cx, fp->script  fp->regs->pc), because
 * fp->imacpc may be non-null, indicating an active imacro.
 */
extern uintN
js_FramePCToLineNumber(JSContext *cx, JSStackFrame *fp);

extern uintN
js_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc);

extern jsbytecode *
js_LineNumberToPC(JSScript *script, uintN lineno);

extern JS_FRIEND_API(uintN)
js_GetScriptLineExtent(JSScript *script);

static JS_INLINE JSOp
js_GetOpcode(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    JSOp op = (JSOp) *pc;
    if (op == JSOP_TRAP)
        op = JS_GetTrapOpcode(cx, script, pc);
    return op;
}

/*
 * If magic is non-null, js_XDRScript succeeds on magic number mismatch but
 * returns false in *magic; it reflects a match via a true *magic out param.
 * If magic is null, js_XDRScript returns false on bad magic number errors,
 * which it reports.
 *
 * NB: after a successful JSXDR_DECODE, and provided that *scriptp is not the
 * JSScript::emptyScript() immutable singleton, js_XDRScript callers must do
 * any required subsequent set-up of owning function or script object and then
 * call js_CallNewScriptHook.
 *
 * If the caller requires a mutable empty script (for debugging or u.object
 * ownership setting), pass true for needMutableScript. Otherwise pass false.
 * Call js_CallNewScriptHook only with a mutable script, i.e. never with the
 * JSScript::emptyScript() singleton.
 */
extern JSBool
js_XDRScript(JSXDRState *xdr, JSScript **scriptp, bool needMutableScript,
             JSBool *hasMagic);

#endif /* jsscript_h___ */
