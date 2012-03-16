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
#include "jsopcode.h"
#include "jsscope.h"

#include "gc/Barrier.h"

/*
 * Type of try note associated with each catch or finally block, and also with
 * for-in loops.
 */
typedef enum JSTryNoteKind {
    JSTRY_CATCH,
    JSTRY_FINALLY,
    JSTRY_ITER
} JSTryNoteKind;

/*
 * Exception handling record.
 */
struct JSTryNote {
    uint8_t         kind;       /* one of JSTryNoteKind */
    uint8_t         padding;    /* explicit padding on uint16_t boundary */
    uint16_t        stackDepth; /* stack depth upon exception handler entry */
    uint32_t        start;      /* start of the try statement or for-in loop
                                   relative to script->main */
    uint32_t        length;     /* length of the try statement or for-in loop */
};

typedef struct JSTryNoteArray {
    JSTryNote       *vector;    /* array of indexed try notes */
    uint32_t        length;     /* count of indexed try notes */
} JSTryNoteArray;

typedef struct JSObjectArray {
    js::HeapPtrObject *vector;  /* array of indexed objects */
    uint32_t        length;     /* count of indexed objects */
} JSObjectArray;

typedef struct JSConstArray {
    js::HeapValue   *vector;    /* array of indexed constant values */
    uint32_t        length;
} JSConstArray;

namespace js {

struct GlobalSlotArray {
    struct Entry {
        uint32_t    atomIndex;  /* index into atom table */
        uint32_t    slot;       /* global obj slot number */
    };
    Entry           *vector;
    uint32_t        length;
};

struct ClosedSlotArray {
    uint32_t        *vector;    /* array of closed slots */
    uint32_t        length;     /* count of closed slots */
};

struct Shape;

enum BindingKind { NONE, ARGUMENT, VARIABLE, CONSTANT };

/*
 * Formal parameters and local variables are stored in a shape tree
 * path encapsulated within this class.  This class represents bindings for
 * both function and top-level scripts (the latter is needed to track names in
 * strict mode eval code, to give such code its own lexical environment).
 */
class Bindings
{
    HeapPtr<Shape> lastBinding;
    uint16_t nargs;
    uint16_t nvars;
    bool     hasDup_:1;     // true if there are duplicate argument names

    inline Shape *initialShape(JSContext *cx) const;
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

    uint16_t countArgs() const { return nargs; }
    uint16_t countVars() const { return nvars; }

    unsigned countLocalNames() const { return nargs + nvars; }

    bool hasLocalNames() const { return countLocalNames() > 0; }

    /* Ensure these bindings have a shape lineage. */
    inline bool ensureShape(JSContext *cx);

    /* Return the shape lineage generated for these bindings. */
    inline Shape *lastShape() const;

    /*
     * Return the shape to use to create a call object for these bindings.
     * The result is guaranteed not to have duplicate property names.
     */
    Shape *callObjectShape(JSContext *cx) const;

    /* See Scope::extensibleParents */
    inline bool extensibleParents();
    bool setExtensibleParents(JSContext *cx);

    bool setParent(JSContext *cx, JSObject *obj);

    enum {
        /* A script may have no more than this many arguments or variables. */
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
     * before before any VARIABLE or CONSTANT bindings.
     */
    bool add(JSContext *cx, JSAtom *name, BindingKind kind);

    /* Convenience specializations. */
    bool addVariable(JSContext *cx, JSAtom *name) {
        return add(cx, name, VARIABLE);
    }
    bool addConstant(JSContext *cx, JSAtom *name) {
        return add(cx, name, CONSTANT);
    }
    bool addArgument(JSContext *cx, JSAtom *name, uint16_t *slotp) {
        JS_ASSERT(name != NULL); /* not destructuring */
        *slotp = nargs;
        return add(cx, name, ARGUMENT);
    }
    bool addDestructuring(JSContext *cx, uint16_t *slotp) {
        *slotp = nargs;
        return add(cx, NULL, ARGUMENT);
    }

    void noteDup() { hasDup_ = true; }
    bool hasDup() const { return hasDup_; }

    /*
     * Look up an argument or variable name, returning its kind when found or
     * NONE when no such name exists. When indexp is not null and the name
     * exists, *indexp will receive the index of the corresponding argument or
     * variable.
     */
    BindingKind lookup(JSContext *cx, JSAtom *name, unsigned *indexp) const;

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
     * Protect stored bindings from mutation.  Subsequent attempts to add
     * bindings will copy the existing bindings before adding to them, allowing
     * the original bindings to be safely shared.
     */
    void makeImmutable();

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

    void trace(JSTracer *trc);

    /* Rooter for stack allocated Bindings. */
    struct StackRoot {
        RootShape root;
        StackRoot(JSContext *cx, Bindings *bindings)
            : root(cx, (Shape **) &bindings->lastBinding)
        {}
    };
};

} /* namespace js */

#define JS_OBJECT_ARRAY_SIZE(length)                                          \
    (offsetof(JSObjectArray, vector) + sizeof(JSObject *) * (length))

#ifdef JS_METHODJIT
namespace JSC {
    class ExecutablePool;
}

namespace js {
namespace mjit {
    struct JITScript;
    class CallCompiler;
}
}

#endif

namespace js {

namespace analyze { class ScriptAnalysis; }

class ScriptCounts
{
    friend struct ::JSScript;
    friend struct ScriptAndCounts;
    /*
     * This points to a single block that holds an array of PCCounts followed
     * by an array of doubles.  Each element in the PCCounts array has a
     * pointer into the array of doubles.
     */
    PCCounts *pcCountsVector;

 public:

    ScriptCounts() : pcCountsVector(NULL) {
    }

    inline void destroy(FreeOp *fop);

    void steal(ScriptCounts &other) {
        *this = other;
        js::PodZero(&other);
    }

    // Boolean conversion, for 'if (scriptCounts) ...'
    operator void*() const {
        return pcCountsVector;
    }
};

class DebugScript
{
    friend struct ::JSScript;

    /*
     * When non-zero, compile script in single-step mode. The top bit is set and
     * cleared by setStepMode, as used by JSD. The lower bits are a count,
     * adjusted by changeStepModeCount, used by the Debugger object. Only
     * when the bit is clear and the count is zero may we compile the script
     * without single-step support.
     */
    uint32_t        stepMode;

    /* Number of breakpoint sites at opcodes in the script. */
    uint32_t        numSites;

    /*
     * Array with all breakpoints installed at opcodes in the script, indexed
     * by the offset of the opcode into the script.
     */
    BreakpointSite  *breakpoints[1];
};

} /* namespace js */

static const uint32_t JS_SCRIPT_COOKIE = 0xc00cee;

struct JSScript : public js::gc::Cell
{
  private:
    static const uint32_t stepFlagMask = 0x80000000U;
    static const uint32_t stepCountMask = 0x7fffffffU;

  public:
#ifdef JS_METHODJIT
    // This type wraps JITScript.  It has three possible states.
    // - "Empty": no compilation has been attempted and there is no JITScript.
    // - "Unjittable": compilation failed and there is no JITScript.
    // - "Valid": compilation succeeded and there is a JITScript.
    class JITScriptHandle
    {
        // CallCompiler must be a friend because it generates code that uses
        // UNJITTABLE.
        friend class js::mjit::CallCompiler;

        // The exact representation:
        // - NULL means "empty".
        // - UNJITTABLE means "unjittable".
        // - Any other value means "valid".
        // UNJITTABLE = 1 so that we can check that a JITScript is valid
        // with a single |> 1| test.  It's defined outside the class because
        // non-integral static const fields can't be defined in the class.
        static const js::mjit::JITScript *UNJITTABLE;   // = (JITScript *)1;
        js::mjit::JITScript *value;

      public:
        JITScriptHandle()       { value = NULL; }

        bool isEmpty()          { return value == NULL; }
        bool isUnjittable()     { return value == UNJITTABLE; }
        bool isValid()          { return value  > UNJITTABLE; }

        js::mjit::JITScript *getValid() {
            JS_ASSERT(isValid());
            return value;
        }

        void setEmpty()         { value = NULL; }
        void setUnjittable()    { value = const_cast<js::mjit::JITScript *>(UNJITTABLE); }
        void setValid(js::mjit::JITScript *jit) {
            value = jit;
            JS_ASSERT(isValid());
        }

        static void staticAsserts();
    };
#endif  // JS_METHODJIT

    //
    // We order fields according to their size in order to avoid wasting space
    // for alignment.
    //

    // Larger-than-word-sized fields.

  public:
    js::Bindings    bindings;   /* names of top-level variables in this script
                                   (and arguments if this is a function script) */

    // Word-sized fields.

  public:
    jsbytecode      *code;      /* bytecodes and their immediate operands */
    uint8_t         *data;      /* pointer to variable-length data array (see 
                                   comment above NewScript() for details) */

    const char      *filename;  /* source filename or null */
    js::HeapPtrAtom *atoms;     /* maps immediate index to literal struct */

    JSPrincipals    *principals;/* principals for this script */
    JSPrincipals    *originPrincipals; /* see jsapi.h 'originPrincipals' comment */

    jschar          *sourceMap; /* source map file or null */

    /*
     * A global object for the script.
     * - All scripts returned by JSAPI functions (JS_CompileScript,
     *   JS_CompileUTF8File, etc.) have a non-null globalObject.
     * - A function script has a globalObject if the function comes from a
     *   compile-and-go script.
     * - Temporary scripts created by obj_eval, JS_EvaluateScript, and
     *   similar functions never have the globalObject field set; for such
     *   scripts the global should be extracted from the JS frame that
     *   execute scripts.
     */
    js::HeapPtr<js::GlobalObject, JSScript*> globalObject;

    /* Execution and profiling information for JIT code in the script. */
    js::ScriptCounts scriptCounts;

    /* Persistent type information retained across GCs. */
    js::types::TypeScript *types;

  public:
#ifdef JS_METHODJIT
    JITScriptHandle jitHandleNormal; // extra JIT info for normal scripts
    JITScriptHandle jitHandleCtor;   // extra JIT info for constructors
#endif

  private:
    js::DebugScript     *debug;
    js::HeapPtrFunction function_;

    size_t          useCount;   /* Number of times the script has been called
                                 * or has had backedges taken. Reset if the
                                 * script's JIT code is forcibly discarded. */
#if JS_BITS_PER_WORD == 32
    void *padding_;
#endif

    // 32-bit fields.

  public:
    uint32_t        length;     /* length of code vector */

    uint32_t        lineno;     /* base line number of script */

    uint32_t        mainOffset; /* offset of main entry point from code, after
                                   predef'ing prolog */

    uint32_t        natoms;     /* length of atoms array */

#ifdef DEBUG
    // Unique identifier within the compartment for this script, used for
    // printing analysis information.
    uint32_t        id_;
  private:
    uint32_t        idpad;
  public:
#endif

    // 16-bit fields.

  private:
    uint16_t        version;    /* JS version under which script was compiled */

  public:
    uint16_t        nfixed;     /* number of slots besides stack operands in
                                   slot array */

    uint16_t        nTypeSets;  /* number of type sets used in this script for
                                   dynamic type monitoring */

    uint16_t        nslots;     /* vars plus maximum stack depth */
    uint16_t        staticLevel;/* static level for display maintenance */

    // 8-bit fields.

  public:
    // Offsets to various array structures from the end of this script, or
    // JSScript::INVALID_OFFSET if the array has length 0.
    uint8_t         constsOffset;   /* offset to the array of constants */
    uint8_t         objectsOffset;  /* offset to the array of nested function,
                                       block, scope, xml and one-time regexps
                                       objects */
    uint8_t         regexpsOffset;  /* offset to the array of to-be-cloned
                                       regexps  */
    uint8_t         trynotesOffset; /* offset to the array of try notes */
    uint8_t         globalsOffset;  /* offset to the array of global slots */
    uint8_t         closedArgsOffset; /* offset to the array of closed args */
    uint8_t         closedVarsOffset; /* offset to the array of closed vars */

    // 1-bit fields.

  public:
    bool            noScriptRval:1; /* no need for result value of last
                                       expression statement */
    bool            savedCallerFun:1; /* can call getCallerFunction() */
    bool            strictModeCode:1; /* code is in strict mode */
    bool            compileAndGo:1;   /* script was compiled with TCF_COMPILE_N_GO */
    bool            bindingsAccessedDynamically:1; /* see TCF_BINDINGS_ACCESSED_DYNAMICALLY */
    bool            warnedAboutTwoArgumentEval:1; /* have warned about use of
                                                     obsolete eval(s, o) in
                                                     this script */
    bool            warnedAboutUndefinedProp:1; /* have warned about uses of
                                                   undefined properties in this
                                                   script */
    bool            hasSingletons:1;  /* script has singleton objects */
    bool            isOuterFunction:1; /* function is heavyweight, with inner functions */
    bool            isInnerFunction:1; /* function is directly nested in a heavyweight
                                        * outer function */
    bool            isActiveEval:1;   /* script came from eval(), and is still active */
    bool            isCachedEval:1;   /* script came from eval(), and is in eval cache */
    bool            uninlineable:1;   /* script is considered uninlineable by analysis */
    bool            reentrantOuterFunction:1; /* outer function marked reentrant */
    bool            typesPurged:1;    /* TypeScript has been purged at some point */
#ifdef JS_METHODJIT
    bool            debugMode:1;      /* script was compiled in debug mode */
    bool            failedBoundsCheck:1; /* script has had hoisted bounds checks fail */
#endif
    bool            callDestroyHook:1;/* need to call destroy hook */

    /*
     * An arguments object is created for a function script (when the function
     * is first called) iff script->needsArgsObj(). There are several cases
     * where the 'arguments' keyword is technically used but which don't really
     * need an object (e.g., 'arguments[i]', 'f.apply(null, arguments')'). This
     * determination is made during script analysis which occurs lazily (right
     * before a script is run). Thus, the output of the front-end is a
     * conservative 'mayNeedArgsObj' which leads to further analysis in
     * analyzeBytecode and analyzeSSA. To avoid the complexity of spurious
     * argument objects creation, we maintain the invariant that needsArgsObj()
     * is only queried after this analysis has occurred (analyzedArgsUsage()).
     */
  private:
    bool            mayNeedArgsObj_:1;
    bool            analyzedArgsUsage_:1;
    bool            needsArgsObj_:1;

    //
    // End of fields.  Start methods.
    //

    /*
     * Two successively less primitive ways to make a new JSScript.  The first
     * does *not* call a non-null cx->runtime->newScriptHook -- only the second,
     * NewScriptFromEmitter, calls this optional debugger hook.
     *
     * The NewScript function can't know whether the script it creates belongs
     * to a function, or is top-level or eval code, but the debugger wants access
     * to the newly made script's function, if any -- so callers of NewScript
     * are responsible for notifying the debugger after successfully creating any
     * kind (function or other) of new JSScript.
     */
  public:
    static JSScript *NewScript(JSContext *cx, uint32_t length, uint32_t nsrcnotes, uint32_t natoms,
                               uint32_t nobjects, uint32_t nregexps,
                               uint32_t ntrynotes, uint32_t nconsts, uint32_t nglobals,
                               uint16_t nClosedArgs, uint16_t nClosedVars, uint32_t nTypeSets,
                               JSVersion version);
    static JSScript *NewScriptFromEmitter(JSContext *cx, js::BytecodeEmitter *bce);

    bool mayNeedArgsObj() const { return mayNeedArgsObj_; }
    bool analyzedArgsUsage() const { return analyzedArgsUsage_; }
    bool needsArgsObj() const { JS_ASSERT(analyzedArgsUsage()); return needsArgsObj_; }
    void setNeedsArgsObj(bool needsArgsObj);
    bool applySpeculationFailed(JSContext *cx);

    void setMayNeedArgsObj() {
        mayNeedArgsObj_ = true;
    }

    /* Hash table chaining for JSCompartment::evalCache. */
    JSScript *&evalHashLink() { return *globalObject.unsafeGetUnioned(); }

    /*
     * Original compiled function for the script, if it has a function.
     * NULL for global and eval scripts.
     */
    JSFunction *function() const { return function_; }
    void setFunction(JSFunction *fun);

#ifdef DEBUG
    unsigned id();
#else
    unsigned id() { return 0; }
#endif

    /* Ensure the script has a TypeScript. */
    inline bool ensureHasTypes(JSContext *cx);

    /*
     * Ensure the script has scope and bytecode analysis information.
     * Performed when the script first runs, or first runs after a TypeScript
     * GC purge. If scope is NULL then the script must already have types with
     * scope information.
     */
    inline bool ensureRanAnalysis(JSContext *cx, JSObject *scope);

    /* Ensure the script has type inference analysis information. */
    inline bool ensureRanInference(JSContext *cx);

    inline bool hasAnalysis();
    inline void clearAnalysis();
    inline js::analyze::ScriptAnalysis *analysis();

    /*
     * Associates this script with a specific function, constructing a new type
     * object for the function if necessary.
     */
    bool typeSetFunction(JSContext *cx, JSFunction *fun, bool singleton = false);

    inline bool hasGlobal() const;
    inline bool hasClearedGlobal() const;

    inline js::GlobalObject *global() const;
    inline js::types::TypeScriptNesting *nesting() const;

    inline void clearNesting();

    /* Return creation time global or null. */
    js::GlobalObject *getGlobalObjectOrNull() const {
        return isCachedEval ? NULL : globalObject.get();
    }

  private:
    bool makeTypes(JSContext *cx);
    bool makeAnalysis(JSContext *cx);

#ifdef JS_METHODJIT
  private:
    // CallCompiler must be a friend because it generates code that directly
    // accesses jitHandleNormal/jitHandleCtor, via jitHandleOffset().
    friend class js::mjit::CallCompiler;

    static size_t jitHandleOffset(bool constructing) {
        return constructing ? offsetof(JSScript, jitHandleCtor)
                            : offsetof(JSScript, jitHandleNormal);
    }

  public:
    bool hasJITCode()   { return jitHandleNormal.isValid() || jitHandleCtor.isValid(); }

    JITScriptHandle *jitHandle(bool constructing) {
        return constructing ? &jitHandleCtor : &jitHandleNormal;
    }

    js::mjit::JITScript *getJIT(bool constructing) {
        JITScriptHandle *jith = jitHandle(constructing);
        return jith->isValid() ? jith->getValid() : NULL;
    }

    static void ReleaseCode(js::FreeOp *fop, JITScriptHandle *jith);

    // These methods are implemented in MethodJIT.h.
    inline void **nativeMap(bool constructing);
    inline void *nativeCodeForPC(bool constructing, jsbytecode *pc);

    size_t getUseCount() const  { return useCount; }
    size_t incUseCount() { return ++useCount; }
    size_t *addressOfUseCount() { return &useCount; }
    void resetUseCount() { useCount = 0; }

    /*
     * Size of the JITScript and all sections.  If |mallocSizeOf| is NULL, the
     * size is computed analytically.  (This method is implemented in
     * MethodJIT.cpp.)
     */
    size_t sizeOfJitScripts(JSMallocSizeOfFun mallocSizeOf);
#endif

  public:
    js::PCCounts getPCCounts(jsbytecode *pc) {
        JS_ASSERT(size_t(pc - code) < length);
        return scriptCounts.pcCountsVector[pc - code];
    }

    bool initScriptCounts(JSContext *cx);
    void destroyScriptCounts(js::FreeOp *fop);

    jsbytecode *main() {
        return code + mainOffset;
    }

    /*
     * computedSizeOfData() is the in-use size of all the data sections.
     * sizeOfData() is the size of the block allocated to hold all the data sections
     * (which can be larger than the in-use size).
     */
    size_t computedSizeOfData();
    size_t sizeOfData(JSMallocSizeOfFun mallocSizeOf);

    uint32_t numNotes();  /* Number of srcnote slots in the srcnotes section */

    /* Script notes are allocated right after the code. */
    jssrcnote *notes() { return (jssrcnote *)(code + length); }

    static const uint8_t INVALID_OFFSET = 0xFF;
    static bool isValidOffset(uint8_t offset) { return offset != INVALID_OFFSET; }

    JSConstArray *consts() {
        JS_ASSERT(isValidOffset(constsOffset));
        return reinterpret_cast<JSConstArray *>(data + constsOffset);
    }

    JSObjectArray *objects() {
        JS_ASSERT(isValidOffset(objectsOffset));
        return reinterpret_cast<JSObjectArray *>(data + objectsOffset);
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

    js::ClosedSlotArray *closedArgs() {
        JS_ASSERT(isValidOffset(closedArgsOffset));
        return reinterpret_cast<js::ClosedSlotArray *>(data + closedArgsOffset);
    }

    js::ClosedSlotArray *closedVars() {
        JS_ASSERT(isValidOffset(closedVarsOffset));
        return reinterpret_cast<js::ClosedSlotArray *>(data + closedVarsOffset);
    }

    uint32_t numClosedArgs() {
        return isValidOffset(closedArgsOffset) ? closedArgs()->length : 0;
    }

    uint32_t numClosedVars() {
        return isValidOffset(closedVarsOffset) ? closedVars()->length : 0;
    }

    js::HeapPtrAtom &getAtom(size_t index) const {
        JS_ASSERT(index < natoms);
        return atoms[index];
    }

    js::PropertyName *getName(size_t index) {
        return getAtom(index)->asPropertyName();
    }

    JSObject *getObject(size_t index) {
        JSObjectArray *arr = objects();
        JS_ASSERT(index < arr->length);
        return arr->vector[index];
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

    uint32_t getClosedArg(uint32_t index) {
        js::ClosedSlotArray *arr = closedArgs();
        JS_ASSERT(index < arr->length);
        return arr->vector[index];
    }

    uint32_t getClosedVar(uint32_t index) {
        js::ClosedSlotArray *arr = closedVars();
        JS_ASSERT(index < arr->length);
        return arr->vector[index];
    }


#ifdef DEBUG
    bool varIsAliased(unsigned varSlot);
    bool argIsAliased(unsigned argSlot);
#endif
  private:
    /*
     * Recompile with or without single-stepping support, as directed
     * by stepModeEnabled().
     */
    void recompileForStepMode(js::FreeOp *fop);

    /* Attempt to change this->stepMode to |newValue|. */
    bool tryNewStepMode(JSContext *cx, uint32_t newValue);

    bool ensureHasDebug(JSContext *cx);

  public:
    bool hasBreakpointsAt(jsbytecode *pc) { return !!getBreakpointSite(pc); }
    bool hasAnyBreakpointsOrStepMode() { return !!debug; }

    js::BreakpointSite *getBreakpointSite(jsbytecode *pc)
    {
        JS_ASSERT(size_t(pc - code) < length);
        return debug ? debug->breakpoints[pc - code] : NULL;
    }

    js::BreakpointSite *getOrCreateBreakpointSite(JSContext *cx, jsbytecode *pc,
                                                  js::GlobalObject *scriptGlobal);

    void destroyBreakpointSite(js::FreeOp *fop, jsbytecode *pc);

    void clearBreakpointsIn(js::FreeOp *fop, js::Debugger *dbg, JSObject *handler);
    void clearTraps(js::FreeOp *fop);

    void markTrapClosures(JSTracer *trc);

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

    bool stepModeEnabled() { return debug && !!debug->stepMode; }

#ifdef DEBUG
    uint32_t stepModeCount() { return debug ? (debug->stepMode & stepCountMask) : 0; }
#endif

    void finalize(js::FreeOp *fop);

    static inline void writeBarrierPre(JSScript *script);
    static inline void writeBarrierPost(JSScript *script, void *addr);

    static inline js::ThingRootKind rootKind() { return js::THING_ROOT_SCRIPT; }

    static JSPrincipals *normalizeOriginPrincipals(JSPrincipals *principals,
                                                   JSPrincipals *originPrincipals) {
        return originPrincipals ? originPrincipals : principals;
    }

    void markChildren(JSTracer *trc);
};

/* If this fails, padding_ can be removed. */
JS_STATIC_ASSERT(sizeof(JSScript) % js::gc::Cell::CellSize == 0);

static JS_INLINE unsigned
StackDepth(JSScript *script)
{
    return script->nslots - script->nfixed;
}

/*
 * New-script-hook calling is factored from NewScriptFromEmitter so that it
 * and callers of XDRScript can share this code.  In the case of callers
 * of XDRScript, the hook should be invoked only after successful decode
 * of any owning function (the fun parameter) or script object (null fun).
 */
extern JS_FRIEND_API(void)
js_CallNewScriptHook(JSContext *cx, JSScript *script, JSFunction *fun);

namespace js {

extern void
CallDestroyScriptHook(FreeOp *fop, JSScript *script);

extern const char *
SaveScriptFilename(JSContext *cx, const char *filename);

extern void
MarkScriptFilename(const char *filename);

extern void
SweepScriptFilenames(JSCompartment *comp);

extern void
FreeScriptFilenames(JSCompartment *comp);

struct ScriptAndCounts
{
    JSScript *script;
    ScriptCounts scriptCounts;

    PCCounts &getPCCounts(jsbytecode *pc) const {
        JS_ASSERT(unsigned(pc - script->code) < script->length);
        return scriptCounts.pcCountsVector[pc - script->code];
    }
};

} /* namespace js */

/*
 * To perturb as little code as possible, we introduce a js_GetSrcNote lookup
 * cache without adding an explicit cx parameter.  Thus js_GetSrcNote becomes
 * a macro that uses cx from its calls' lexical environments.
 */
#define js_GetSrcNote(script,pc) js_GetSrcNoteCached(cx, script, pc)

extern jssrcnote *
js_GetSrcNoteCached(JSContext *cx, JSScript *script, jsbytecode *pc);

extern jsbytecode *
js_LineNumberToPC(JSScript *script, unsigned lineno);

extern JS_FRIEND_API(unsigned)
js_GetScriptLineExtent(JSScript *script);

namespace js {

extern unsigned
PCToLineNumber(JSScript *script, jsbytecode *pc);

extern unsigned
PCToLineNumber(unsigned startLine, jssrcnote *notes, jsbytecode *code, jsbytecode *pc);

extern unsigned
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

inline void
CurrentScriptFileLineOrigin(JSContext *cx, unsigned *linenop, LineOption = NOT_CALLED_FROM_JSOP_EVAL);

extern JSScript *
CloneScript(JSContext *cx, JSScript *script);

/*
 * NB: after a successful XDR_DECODE, XDRScript callers must do any required
 * subsequent set-up of owning function or script object and then call
 * js_CallNewScriptHook.
 */
template<XDRMode mode>
bool
XDRScript(XDRState<mode> *xdr, JSScript **scriptp, JSScript *parentScript);

} /* namespace js */

#endif /* jsscript_h___ */
