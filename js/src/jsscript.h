/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "jsinfer.h"

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

struct JSArenaPool;

namespace js {

struct GlobalSlotArray {
    struct Entry {
        uint32      atomIndex;  /* index into atom table */
        uint32      slot;       /* global obj slot number */
    };
    Entry           *vector;
    uint32          length;
};

struct Shape;

enum BindingKind { NONE, ARGUMENT, VARIABLE, CONSTANT, UPVAR };

/*
 * Formal parameters, local variables, and upvars are stored in a shape tree
 * path encapsulated within this class.  This class represents bindings for
 * both function and top-level scripts (the latter is needed to track names in
 * strict mode eval code, to give such code its own lexical environment).
 */
class Bindings {
    js::Shape *lastBinding;
    uint16 nargs;
    uint16 nvars;
    uint16 nupvars;
    bool hasExtensibleParents;

  public:
    inline Bindings(JSContext *cx);

    /*
     * Transfers ownership of bindings data from bindings into this fresh
     * Bindings instance. Once such a transfer occurs, the old bindings must
     * not be used again.
     */
    inline void transfer(JSContext *cx, Bindings *bindings);

    /*
     * Clones bindings data from bindings, which must be immutable, into this
     * fresh Bindings instance. A Bindings instance may be cloned multiple
     * times.
     */
    inline void clone(JSContext *cx, Bindings *bindings);

    uint16 countArgs() const { return nargs; }
    uint16 countVars() const { return nvars; }
    uint16 countUpvars() const { return nupvars; }

    uintN countArgsAndVars() const { return nargs + nvars; }

    uintN countLocalNames() const { return nargs + nvars + nupvars; }

    bool hasUpvars() const { return nupvars > 0; }
    bool hasLocalNames() const { return countLocalNames() > 0; }

    /* Ensure these bindings have a shape lineage. */
    inline bool ensureShape(JSContext *cx);

    /* Returns the shape lineage generated for these bindings. */
    inline js::Shape *lastShape() const;

    enum {
        /*
         * A script may have no more than this many arguments, variables, or
         * upvars.
         */
        BINDING_COUNT_LIMIT = 0xFFFF
    };

    /*
     * Add a local binding for the given name, of the given type, for the code
     * being compiled.  If fun is non-null, this binding set is being created
     * for that function, so adjust corresponding metadata in that function
     * while adding.  Otherwise this set must correspond to a top-level script.
     *
     * A binding may be added twice with different kinds; the last one for a
     * given name prevails.  (We preserve both bindings for the decompiler,
     * which must deal with such cases.)  Pass null for name when indicating a
     * destructuring argument.  Return true on success.
     *
     * The parser builds shape paths for functions, usable by Call objects at
     * runtime, by calling an "add" method. All ARGUMENT bindings must be added
     * before before any VARIABLE or CONSTANT bindings, which themselves must
     * be added before all UPVAR bindings.
     */
    bool add(JSContext *cx, JSAtom *name, BindingKind kind);

    /* Convenience specializations. */
    bool addVariable(JSContext *cx, JSAtom *name) {
        return add(cx, name, VARIABLE);
    }
    bool addConstant(JSContext *cx, JSAtom *name) {
        return add(cx, name, CONSTANT);
    }
    bool addUpvar(JSContext *cx, JSAtom *name) {
        return add(cx, name, UPVAR);
    }
    bool addArgument(JSContext *cx, JSAtom *name, uint16 *slotp) {
        JS_ASSERT(name != NULL); /* not destructuring */
        *slotp = nargs;
        return add(cx, name, ARGUMENT);
    }
    bool addDestructuring(JSContext *cx, uint16 *slotp) {
        *slotp = nargs;
        return add(cx, NULL, ARGUMENT);
    }

    /*
     * Look up an argument or variable name, returning its kind when found or
     * NONE when no such name exists. When indexp is not null and the name
     * exists, *indexp will receive the index of the corresponding argument or
     * variable.
     */
    BindingKind lookup(JSContext *cx, JSAtom *name, uintN *indexp) const;

    /* Convenience method to check for any binding for a name. */
    bool hasBinding(JSContext *cx, JSAtom *name) const {
        return lookup(cx, name, NULL) != NONE;
    }

    /*
     * This method returns the local variable, argument, etc. names used by a
     * script.  This function must be called only when hasLocalNames().
     *
     * The elements of the vector with index less than nargs correspond to the
     * the names of arguments. An index >= nargs addresses a var binding.
     * The name at an element will be null when the element is for an argument
     * corresponding to a destructuring pattern.
     */
    bool getLocalNameArray(JSContext *cx, Vector<JSAtom *> *namesp);

    /*
     * Returns the slot where the sharp array is stored, or a value < 0 if no
     * sharps are present or in case of failure.
     */
    int sharpSlotBase(JSContext *cx);

    /*
     * Protect stored bindings from mutation.  Subsequent attempts to add
     * bindings will copy the existing bindings before adding to them, allowing
     * the original bindings to be safely shared.
     */
    void makeImmutable();

    /*
     * Sometimes call objects and run-time block objects need unique shapes, but
     * sometimes they don't.
     *
     * Property cache entries only record the shapes of the first and last
     * objects along the search path, so if the search traverses more than those
     * two objects, then those first and last shapes must determine the shapes
     * of everything else along the path. The js_PurgeScopeChain stuff takes
     * care of making this work, but that suffices only because we require that
     * start points with the same shape have the same successor object in the
     * search path --- a cache hit means the starting shapes were equal, which
     * means the seach path tail (everything but the first object in the path)
     * was shared, which in turn means the effects of a purge will be seen by
     * all affected starting search points.
     *
     * For call and run-time block objects, the "successor object" is the scope
     * chain parent. Unlike prototype objects (of which there are usually few),
     * scope chain parents are created frequently (possibly on every call), so
     * following the shape-implies-parent rule blindly would lead one to give
     * every call and block its own shape.
     *
     * In many cases, however, it's not actually necessary to give call and
     * block objects their own shapes, and we can do better. If the code will
     * always be used with the same global object, and none of the enclosing
     * call objects could have bindings added to them at runtime (by direct eval
     * calls or function statements), then we can use a fixed set of shapes for
     * those objects. You could think of the shapes in the functions' bindings
     * and compile-time blocks as uniquely identifying the global object(s) at
     * the end of the scope chain.
     *
     * (In fact, some JSScripts we do use against multiple global objects (see
     * bug 618497), and using the fixed shapes isn't sound there.)
     *
     * In deciding whether a call or block has any extensible parents, we
     * actually only need to consider enclosing calls; blocks are never
     * extensible, and the other sorts of objects that appear in the scope
     * chains ('with' blocks, say) are not CacheableNonGlobalScopes.
     *
     * If the hasExtensibleParents flag is set, then Call objects created for
     * the function this Bindings describes need unique shapes. If the flag is
     * clear, then we can use lastBinding's shape.
     *
     * For blocks, we set the the OWN_SHAPE flag on the compiler-generated
     * blocksto indicate that their clones need unique shapes.
     */
    void setExtensibleParents() { hasExtensibleParents = true; }
    bool extensibleParents() const { return hasExtensibleParents; }

    /*
     * These methods provide direct access to the shape path normally
     * encapsulated by js::Bindings. These methods may be used to make a
     * Shape::Range for iterating over the relevant shapes from youngest to
     * oldest (i.e., last or right-most to first or left-most in source order).
     *
     * Sometimes iteration order must be from oldest to youngest, however. For
     * such cases, use js::Bindings::getLocalNameArray.
     */
    const js::Shape *lastArgument() const;
    const js::Shape *lastVariable() const;
    const js::Shape *lastUpvar() const;

    void trace(JSTracer *trc);
};

} /* namespace js */

#define JS_OBJECT_ARRAY_SIZE(length)                                          \
    (offsetof(JSObjectArray, vector) + sizeof(JSObject *) * (length))

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

namespace js { namespace mjit { struct JITScript; } }
#endif

namespace js { namespace analyze { class ScriptAnalysis; } }

class JSPCCounters {
    size_t numBytecodes;
    double *counts;

 public:

    enum {
        INTERP = 0,
        TRACEJIT,
        METHODJIT,
        METHODJIT_STUBS,
        METHODJIT_CODE,
        METHODJIT_PICS,
        NUM_COUNTERS
    };

    JSPCCounters() : numBytecodes(0), counts(NULL) {
    }

    ~JSPCCounters() {
        JS_ASSERT(!counts);
    }

    bool init(JSContext *cx, size_t numBytecodes);
    void destroy(JSContext *cx);

    // Boolean conversion, for 'if (counters) ...'
    operator void*() const {
        return counts;
    }

    double *get(int runmode) {
        JS_ASSERT(runmode >= 0 && runmode < NUM_COUNTERS);
        return counts ? &counts[numBytecodes * runmode] : NULL;
    }

    double& get(int runmode, size_t offset) {
        JS_ASSERT(offset < numBytecodes);
        JS_ASSERT(counts);
        return get(runmode)[offset];
    }
};

static const uint32 JS_SCRIPT_COOKIE = 0xc00cee;

static JSObject * const JS_NEW_SCRIPT = (JSObject *)0x12345678;
static JSObject * const JS_CACHED_SCRIPT = (JSObject *)0x12341234;

struct JSScript : public js::gc::Cell {
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
     */
    static JSScript *NewScript(JSContext *cx, uint32 length, uint32 nsrcnotes, uint32 natoms,
                               uint32 nobjects, uint32 nupvars, uint32 nregexps,
                               uint32 ntrynotes, uint32 nconsts, uint32 nglobals,
                               uint16 nClosedArgs, uint16 nClosedVars, uint32 nTypeSets,
                               JSVersion version);

    static JSScript *NewScriptFromCG(JSContext *cx, JSCodeGenerator *cg);

#ifdef JS_CRASH_DIAGNOSTICS
    /*
     * Make sure that the cookie size does not affect the GC alignment
     * requirements.
     */
    uint32          cookie1[Cell::CellSize / sizeof(uint32)];
#endif
    jsbytecode      *code;      /* bytecodes and their immediate operands */
    uint8           *data;      /* pointer to variable-length data array */

    uint32          length;     /* length of code vector */
  private:
    uint16          version;    /* JS version under which script was compiled */

  public:
    uint16          nfixed;     /* number of slots besides stack operands in
                                   slot array */
    /*
     * Offsets to various array structures from the end of this script, or
     * JSScript::INVALID_OFFSET if the array has length 0.
     */
    uint8           objectsOffset;  /* offset to the array of nested function,
                                       block, scope, xml and one-time regexps
                                       objects */
    uint8           upvarsOffset;   /* offset of the array of display ("up")
                                       closure vars */
    uint8           regexpsOffset;  /* offset to the array of to-be-cloned
                                       regexps  */
    uint8           trynotesOffset; /* offset to the array of try notes */
    uint8           globalsOffset;  /* offset to the array of global slots */
    uint8           constOffset;    /* offset to the array of constants */

    uint16          nTypeSets;      /* number of type sets used in this script for
                                       dynamic type monitoring */

    /*
     * When non-zero, compile script in single-step mode. The top bit is set and
     * cleared by setStepMode, as used by JSD. The lower bits are a count,
     * adjusted by changeStepModeCount, used by the Debugger object. Only
     * when the bit is clear and the count is zero may we compile the script
     * without single-step support.
     */
    uint32          stepMode;

    uint32          lineno;     /* base line number of script */

    uint32          mainOffset; /* offset of main entry point from code, after
                                   predef'ing prolog */
    bool            noScriptRval:1; /* no need for result value of last
                                       expression statement */
    bool            savedCallerFun:1; /* can call getCallerFunction() */
    bool            hasSharps:1;      /* script uses sharp variables */
    bool            strictModeCode:1; /* code is in strict mode */
    bool            compileAndGo:1;   /* script was compiled with TCF_COMPILE_N_GO */
    bool            usesEval:1;       /* script uses eval() */
    bool            usesArguments:1;  /* script uses arguments */
    bool            warnedAboutTwoArgumentEval:1; /* have warned about use of
                                                     obsolete eval(s, o) in
                                                     this script */
    bool            warnedAboutUndefinedProp:1; /* have warned about uses of
                                                   undefined properties in this
                                                   script */
    bool            hasSingletons:1;  /* script has singleton objects */
    bool            hasFunction:1;    /* function is active in 'where' union */
    bool            isActiveEval:1;   /* script came from eval(), and is still active */
    bool            isCachedEval:1;   /* script came from eval(), and is in eval cache */
    bool            usedLazyArgs:1;   /* script has used lazy arguments at some point */
    bool            createdArgs:1;    /* script has had arguments objects created */
    bool            uninlineable:1;   /* script is considered uninlineable by analysis */
#ifdef JS_METHODJIT
    bool            debugMode:1;      /* script was compiled in debug mode */
    bool            failedBoundsCheck:1; /* script has had hoisted bounds checks fail */
#endif
    bool            callDestroyHook:1;/* need to call destroy hook */

    uint32          natoms;     /* length of atoms array */
    uint16          nslots;     /* vars plus maximum stack depth */
    uint16          staticLevel;/* static level for display maintenance */

    uint16          nClosedArgs; /* number of args which are closed over. */
    uint16          nClosedVars; /* number of vars which are closed over. */

    /*
     * To ensure sizeof(JSScript) % gc::Cell::CellSize  == 0 on we must pad
     * the script with 4 bytes. We use them to store tiny scripts like empty
     * scripts.
     */
#define JS_SCRIPT_INLINE_DATA_LIMIT 4
    uint8           inlineData[JS_SCRIPT_INLINE_DATA_LIMIT];

    const char      *filename;  /* source filename or null */
    JSAtom          **atoms;    /* maps immediate index to literal struct */
  private:
    size_t          useCount;  /* Number of times the script has been called
                                 * or has had backedges taken. Reset if the
                                 * script's JIT code is forcibly discarded. */
  public:
    js::Bindings    bindings;   /* names of top-level variables in this script
                                   (and arguments if this is a function script) */
    JSPrincipals    *principals;/* principals for this script */
    jschar          *sourceMap; /* source map file or null */

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
         */
        JSObject    *object;

        /* Hash table chaining for JSCompartment::evalCache. */
        JSScript    *evalHashLink;
    } u;

    uint32          *closedSlots; /* vector of closed slots; args first, then vars. */

    /* array of execution counters for every JSOp in the script, by runmode */
    JSPCCounters    pcCounters;

    union {
        /* Function this script is the body for, if there is one. */
        JSFunction *fun;

        /* Global object for this script, if compileAndGo. */
        js::GlobalObject *global;
    } where;

    inline JSFunction *function() const {
        JS_ASSERT(hasFunction);
        return where.fun;
    }

#ifdef JS_CRASH_DIAGNOSTICS
    JSObject        *ownerObject;

    /* All diagnostic fields must be multiples of Cell::CellSize. */
    uint32          cookie2[sizeof(JSObject *) == 4 ? 1 : 2];
#endif

    void setOwnerObject(JSObject *owner);

    /*
     * Associates this script with a specific function, constructing a new type
     * object for the function.
     */
    bool typeSetFunction(JSContext *cx, JSFunction *fun, bool singleton = false);

    inline bool hasGlobal() const;
    inline js::GlobalObject *global() const;

    inline bool hasClearedGlobal() const;

#ifdef DEBUG
    /*
     * Unique identifier within the compartment for this script, used for
     * printing analysis information.
     */
    uint32 id_;
    uint32 idpad;
    unsigned id() { return id_; }
#else
    unsigned id() { return 0; }
#endif

    /* Persistent type information retained across GCs. */
    js::types::TypeScript *types;

    /* Ensure the script has types, bytecode and/or type inference results. */
    inline bool ensureHasTypes(JSContext *cx);
    inline bool ensureRanBytecode(JSContext *cx);
    inline bool ensureRanInference(JSContext *cx);

    /* Filled in by one of the above. */
    inline bool hasAnalysis();
    inline js::analyze::ScriptAnalysis *analysis();

  private:
    bool makeTypes(JSContext *cx);
    bool makeAnalysis(JSContext *cx);
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
#endif

#ifdef JS_METHODJIT
    bool hasJITCode() {
        return jitNormal || jitCtor;
    }

    // These methods are implemented in MethodJIT.h.
    inline void **nativeMap(bool constructing);
    inline void *maybeNativeCodeForPC(bool constructing, jsbytecode *pc);
    inline void *nativeCodeForPC(bool constructing, jsbytecode *pc);

    js::mjit::JITScript *getJIT(bool constructing) {
        return constructing ? jitCtor : jitNormal;
    }

    size_t getUseCount() const  { return useCount; }
    size_t incUseCount() { return ++useCount; }
    size_t *addressOfUseCount() { return &useCount; }
    void resetUseCount() { useCount = 0; }

    JITScriptStatus getJITStatus(bool constructing) {
        void *addr = constructing ? jitArityCheckCtor : jitArityCheckNormal;
        if (addr == NULL)
            return JITScript_None;
        if (addr == JS_UNJITTABLE_SCRIPT)
            return JITScript_Invalid;
        return JITScript_Valid;
    }

    // This method is implemented in MethodJIT.h.
    JS_FRIEND_API(size_t) jitDataSize();/* Size of the JITScript and all sections */
#endif

    jsbytecode *main() {
        return code + mainOffset;
    }

    JS_FRIEND_API(size_t) dataSize();   /* Size of all data sections */
    uint32 numNotes();                  /* Number of srcnote slots in the srcnotes section */

    /* Script notes are allocated right after the code. */
    jssrcnote *notes() { return (jssrcnote *)(code + length); }

    static const uint8 INVALID_OFFSET = 0xFF;
    static bool isValidOffset(uint8 offset) { return offset != INVALID_OFFSET; }

    JSObjectArray *objects() {
        JS_ASSERT(isValidOffset(objectsOffset));
        return reinterpret_cast<JSObjectArray *>(data + objectsOffset);
    }

    JSUpvarArray *upvars() {
        JS_ASSERT(isValidOffset(upvarsOffset));
        return reinterpret_cast<JSUpvarArray *>(data + upvarsOffset);
    }

    JSObjectArray *regexps() {
        JS_ASSERT(isValidOffset(regexpsOffset));
        return reinterpret_cast<JSObjectArray *>(data + regexpsOffset);
    }

    JSTryNoteArray *trynotes() {
        JS_ASSERT(isValidOffset(trynotesOffset));
        return reinterpret_cast<JSTryNoteArray *>(data + trynotesOffset);
    }

    js::GlobalSlotArray *globals() {
        JS_ASSERT(isValidOffset(globalsOffset));
        return reinterpret_cast<js::GlobalSlotArray *>(data + globalsOffset);
    }

    JSConstArray *consts() {
        JS_ASSERT(isValidOffset(constOffset));
        return reinterpret_cast<JSConstArray *>(data + constOffset);
    }

    JSAtom *getAtom(size_t index) {
        JS_ASSERT(index < natoms);
        return atoms[index];
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

    inline JSFunction *getFunction(size_t index);
    inline JSFunction *getCallerFunction();

    inline JSObject *getRegExp(size_t index);

    const js::Value &getConst(size_t index) {
        JSConstArray *arr = consts();
        JS_ASSERT(index < arr->length);
        return arr->vector[index];
    }

    /*
     * The isEmpty method tells whether this script has code that computes any
     * result (not return value, result AKA normal completion value) other than
     * JSVAL_VOID, or any other effects.
     */
    inline bool isEmpty() const;

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
    static const uint32 stepFlagMask = 0x80000000U;
    static const uint32 stepCountMask = 0x7fffffffU;

    /*
     * Attempt to recompile with or without single-stepping support, as directed
     * by stepModeEnabled().
     */
    bool recompileForStepMode(JSContext *cx);

    /* Attempt to change this->stepMode to |newValue|. */
    bool tryNewStepMode(JSContext *cx, uint32 newValue);

  public:
    /*
     * Set or clear the single-step flag. If the flag is set or the count
     * (adjusted by changeStepModeCount) is non-zero, then the script is in
     * single-step mode. (JSD uses an on/off-style interface; Debugger uses a
     * count-style interface.)
     */
    bool setStepModeFlag(JSContext *cx, bool step);
    
    /*
     * Increment or decrement the single-step count. If the count is non-zero or
     * the flag (set by setStepModeFlag) is set, then the script is in
     * single-step mode. (JSD uses an on/off-style interface; Debugger uses a
     * count-style interface.)
     */
    bool changeStepModeCount(JSContext *cx, int delta);

    bool stepModeEnabled() { return !!stepMode; }

#ifdef DEBUG
    uint32 stepModeCount() { return stepMode & stepCountMask; }
#endif

    void finalize(JSContext *cx);
};

JS_STATIC_ASSERT(sizeof(JSScript) % js::gc::Cell::CellSize == 0);

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
            (atom) = cx->runtime->atomState.commonAtomsStart()[index];        \
        } else {                                                              \
            (atom) = script_->getAtom(index);                                 \
        }                                                                     \
    JS_END_MACRO

extern JS_FRIEND_DATA(js::Class) js_ScriptClass;

extern JSObject *
js_InitScriptClass(JSContext *cx, JSObject *obj);

extern void
js_MarkScriptFilename(const char *filename);

extern void
js_SweepScriptFilenames(JSCompartment *comp);

/*
 * New-script-hook calling is factored from js_NewScriptFromCG so that it
 * and callers of js_XDRScript can share this code.  In the case of callers
 * of js_XDRScript, the hook should be invoked only after successful decode
 * of any owning function (the fun parameter) or script object (null fun).
 */
extern JS_FRIEND_API(void)
js_CallNewScriptHook(JSContext *cx, JSScript *script, JSFunction *fun);

extern void
js_CallDestroyScriptHook(JSContext *cx, JSScript *script);

namespace js {

#ifdef JS_CRASH_DIAGNOSTICS

void
CheckScriptOwner(JSScript *script, JSObject *owner);

void
CheckScript(JSScript *script, JSScript *prev);

#else

inline void
CheckScriptOwner(JSScript *script, JSObject *owner)
{
}

inline void
CheckScript(JSScript *script, JSScript *prev)
{
}

#endif /* !JS_CRASH_DIAGNOSTICS */

} /* namespace js */

extern JSObject *
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
js_FramePCToLineNumber(JSContext *cx, js::StackFrame *fp, jsbytecode *pc);

extern uintN
js_PCToLineNumber(JSContext *cx, JSScript *script, jsbytecode *pc);

extern jsbytecode *
js_LineNumberToPC(JSScript *script, uintN lineno);

extern JS_FRIEND_API(uintN)
js_GetScriptLineExtent(JSScript *script);

namespace js {

extern uintN
CurrentLine(JSContext *cx);

/*
 * This function returns the file and line number of the script currently
 * executing on cx. If there is no current script executing on cx (e.g., a
 * native called directly through JSAPI (e.g., by setTimeout)), NULL and 0 are
 * returned as the file and line. Additionally, this function avoids the full
 * linear scan to compute line number when the caller guarnatees that the
 * script compilation occurs at a JSOP_EVAL.
 */

enum LineOption {
    CALLED_FROM_JSOP_EVAL,
    NOT_CALLED_FROM_JSOP_EVAL
};

inline const char *
CurrentScriptFileAndLine(JSContext *cx, uintN *linenop, LineOption = NOT_CALLED_FROM_JSOP_EVAL);

}

static JS_INLINE JSOp
js_GetOpcode(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    JSOp op = (JSOp) *pc;
    if (op == JSOP_TRAP)
        op = JS_GetTrapOpcode(cx, script, pc);
    return op;
}

extern JSScript *
js_CloneScript(JSContext *cx, JSScript *script);

/*
 * NB: after a successful JSXDR_DECODE, js_XDRScript callers must do any
 * required subsequent set-up of owning function or script object and then call
 * js_CallNewScriptHook.
 */
extern JSBool
js_XDRScript(JSXDRState *xdr, JSScript **scriptp);

inline bool
JSObject::isScript() const
{
    return getClass() == &js_ScriptClass;
}

inline JSScript *
JSObject::getScript() const
{
    JS_ASSERT(isScript());
    return static_cast<JSScript *>(getPrivate());
}

#endif /* jsscript_h___ */
