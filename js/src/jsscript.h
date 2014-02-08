/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS script descriptor. */

#ifndef jsscript_h
#define jsscript_h

#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"

#include "jsatom.h"
#ifdef JS_THREADSAFE
#include "jslock.h"
#endif
#include "jsobj.h"
#include "jsopcode.h"
#include "jstypes.h"

#include "gc/Barrier.h"
#include "gc/Rooting.h"
#include "jit/IonCode.h"
#include "vm/Shape.h"

namespace js {

namespace jit {
    struct BaselineScript;
    struct IonScriptCounts;
}

# define ION_DISABLED_SCRIPT ((js::jit::IonScript *)0x1)
# define ION_COMPILING_SCRIPT ((js::jit::IonScript *)0x2)

# define BASELINE_DISABLED_SCRIPT ((js::jit::BaselineScript *)0x1)

class BreakpointSite;
class BindingIter;
class LazyScript;
class RegExpObject;
struct SourceCompressionTask;
class Shape;
class WatchpointMap;
class NestedScopeObject;

namespace analyze {
    class ScriptAnalysis;
}

namespace frontend {
    class BytecodeEmitter;
}

}

/*
 * Type of try note associated with each catch or finally block, and also with
 * for-in and other kinds of loops. Non-for-in loops do not need these notes
 * for exception unwinding, but storing their boundaries here is helpful for
 * heuristics that need to know whether a given op is inside a loop.
 */
typedef enum JSTryNoteKind {
    JSTRY_CATCH,
    JSTRY_FINALLY,
    JSTRY_ITER,
    JSTRY_LOOP
} JSTryNoteKind;

/*
 * Exception handling record.
 */
struct JSTryNote {
    uint8_t         kind;       /* one of JSTryNoteKind */
    uint32_t        stackDepth; /* stack depth upon exception handler entry */
    uint32_t        start;      /* start of the try statement or loop
                                   relative to script->main */
    uint32_t        length;     /* length of the try statement or loop */
};

namespace js {

// A block scope has a range in bytecode: it is entered at some offset, and left
// at some later offset.  Scopes can be nested.  Given an offset, the
// BlockScopeNote containing that offset whose with the highest start value
// indicates the block scope.  The block scope list is sorted by increasing
// start value.
//
// It is possible to leave a scope nonlocally, for example via a "break"
// statement, so there may be short bytecode ranges in a block scope in which we
// are popping the block chain in preparation for a goto.  These exits are also
// nested with respect to outer scopes.  The scopes in these exits are indicated
// by the "index" field, just like any other block.  If a nonlocal exit pops the
// last block scope, the index will be NoBlockScopeIndex.
//
struct BlockScopeNote {
    static const uint32_t NoBlockScopeIndex = UINT32_MAX;

    uint32_t        index;      // Index of NestedScopeObject in the object
                                // array, or NoBlockScopeIndex if there is no
                                // block scope in this range.
    uint32_t        start;      // Bytecode offset at which this scope starts,
                                // from script->main().
    uint32_t        length;     // Bytecode length of scope.
    uint32_t        parent;     // Index of parent block scope in notes, or UINT32_MAX.
};

struct ConstArray {
    js::HeapValue   *vector;    /* array of indexed constant values */
    uint32_t        length;
};

struct ObjectArray {
    js::HeapPtrObject *vector;  // Array of indexed objects.
    uint32_t        length;     // Count of indexed objects.
};

struct TryNoteArray {
    JSTryNote       *vector;    // Array of indexed try notes.
    uint32_t        length;     // Count of indexed try notes.
};

struct BlockScopeArray {
    BlockScopeNote *vector;     // Array of indexed BlockScopeNote records.
    uint32_t        length;     // Count of indexed try notes.
};

/*
 * A "binding" is a formal, 'var' or 'const' declaration. A function's lexical
 * scope is composed of these three kinds of bindings.
 */

enum BindingKind { ARGUMENT, VARIABLE, CONSTANT };

class Binding
{
    /*
     * One JSScript stores one Binding per formal/variable so we use a
     * packed-word representation.
     */
    uintptr_t bits_;

    static const uintptr_t KIND_MASK = 0x3;
    static const uintptr_t ALIASED_BIT = 0x4;
    static const uintptr_t NAME_MASK = ~(KIND_MASK | ALIASED_BIT);

  public:
    explicit Binding() : bits_(0) {}

    Binding(PropertyName *name, BindingKind kind, bool aliased) {
        JS_STATIC_ASSERT(CONSTANT <= KIND_MASK);
        JS_ASSERT((uintptr_t(name) & ~NAME_MASK) == 0);
        JS_ASSERT((uintptr_t(kind) & ~KIND_MASK) == 0);
        bits_ = uintptr_t(name) | uintptr_t(kind) | (aliased ? ALIASED_BIT : 0);
    }

    PropertyName *name() const {
        return (PropertyName *)(bits_ & NAME_MASK);
    }

    BindingKind kind() const {
        return BindingKind(bits_ & KIND_MASK);
    }

    bool aliased() const {
        return bool(bits_ & ALIASED_BIT);
    }
};

JS_STATIC_ASSERT(sizeof(Binding) == sizeof(uintptr_t));

class Bindings;
typedef InternalHandle<Bindings *> InternalBindingsHandle;

/*
 * Formal parameters and local variables are stored in a shape tree
 * path encapsulated within this class.  This class represents bindings for
 * both function and top-level scripts (the latter is needed to track names in
 * strict mode eval code, to give such code its own lexical environment).
 */
class Bindings
{
    friend class BindingIter;
    friend class AliasedFormalIter;

    HeapPtr<Shape> callObjShape_;
    uintptr_t bindingArrayAndFlag_;
    uint16_t numArgs_;
    uint32_t numVars_;

    /*
     * During parsing, bindings are allocated out of a temporary LifoAlloc.
     * After parsing, a JSScript object is created and the bindings are
     * permanently transferred to it. On error paths, the JSScript object may
     * end up with bindings that still point to the (new released) LifoAlloc
     * memory. To avoid tracing these bindings during GC, we keep track of
     * whether the bindings are temporary or permanent in the low bit of
     * bindingArrayAndFlag_.
     */
    static const uintptr_t TEMPORARY_STORAGE_BIT = 0x1;
    bool bindingArrayUsingTemporaryStorage() const {
        return bindingArrayAndFlag_ & TEMPORARY_STORAGE_BIT;
    }

  public:

    Binding *bindingArray() const {
        return reinterpret_cast<Binding *>(bindingArrayAndFlag_ & ~TEMPORARY_STORAGE_BIT);
    }

    inline Bindings();

    /*
     * Initialize a Bindings with a pointer into temporary storage.
     * bindingArray must have length numArgs+numVars. Before the temporary
     * storage is release, switchToScriptStorage must be called, providing a
     * pointer into the Binding array stored in script->data.
     */
    static bool initWithTemporaryStorage(ExclusiveContext *cx, InternalBindingsHandle self,
                                         unsigned numArgs, uint32_t numVars,
                                         Binding *bindingArray);

    uint8_t *switchToScriptStorage(Binding *newStorage);

    /*
     * Clone srcScript's bindings (as part of js::CloneScript). dstScriptData
     * is the pointer to what will eventually be dstScript->data.
     */
    static bool clone(JSContext *cx, InternalBindingsHandle self, uint8_t *dstScriptData,
                      HandleScript srcScript);

    unsigned numArgs() const { return numArgs_; }
    uint32_t numVars() const { return numVars_; }
    uint32_t count() const { return numArgs() + numVars(); }

    /* Return the initial shape of call objects created for this scope. */
    Shape *callObjShape() const { return callObjShape_; }

    /* Convenience method to get the var index of 'arguments'. */
    static uint32_t argumentsVarIndex(ExclusiveContext *cx, InternalBindingsHandle);

    /* Return whether the binding at bindingIndex is aliased. */
    bool bindingIsAliased(uint32_t bindingIndex);

    /* Return whether this scope has any aliased bindings. */
    bool hasAnyAliasedBindings() const {
        if (!callObjShape_)
            return false;

        // Binding shapes are immutable once constructed.
        AutoThreadSafeAccess ts(callObjShape_);
        return !callObjShape_->isEmptyShape();
    }

    void trace(JSTracer *trc);
};

template <>
struct GCMethods<Bindings> {
    static Bindings initial();
    static ThingRootKind kind() { return THING_ROOT_BINDINGS; }
    static bool poisoned(const Bindings &bindings) {
        return IsPoisonedPtr(static_cast<Shape *>(bindings.callObjShape()));
    }
};

class ScriptCounts
{
    friend class ::JSScript;
    friend struct ScriptAndCounts;

    /*
     * This points to a single block that holds an array of PCCounts followed
     * by an array of doubles.  Each element in the PCCounts array has a
     * pointer into the array of doubles.
     */
    PCCounts *pcCountsVector;

    /* Information about any Ion compilations for the script. */
    jit::IonScriptCounts *ionCounts;

 public:
    ScriptCounts() : pcCountsVector(nullptr), ionCounts(nullptr) { }

    inline void destroy(FreeOp *fop);

    void set(js::ScriptCounts counts) {
        pcCountsVector = counts.pcCountsVector;
        ionCounts = counts.ionCounts;
    }
};

typedef HashMap<JSScript *,
                ScriptCounts,
                DefaultHasher<JSScript *>,
                SystemAllocPolicy> ScriptCountsMap;

class DebugScript
{
    friend class ::JSScript;

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

typedef HashMap<JSScript *,
                DebugScript *,
                DefaultHasher<JSScript *>,
                SystemAllocPolicy> DebugScriptMap;

class ScriptSource;

class SourceDataCache
{
    typedef HashMap<ScriptSource *,
                    const jschar *,
                    DefaultHasher<ScriptSource *>,
                    SystemAllocPolicy> Map;
    Map *map_;
    size_t numSuppressPurges_;

  public:
    SourceDataCache() : map_(nullptr), numSuppressPurges_(0) {}

    class AutoSuppressPurge
    {
        SourceDataCache &cache_;
        mozilla::DebugOnly<size_t> oldValue_;
      public:
        explicit AutoSuppressPurge(JSContext *cx);
        ~AutoSuppressPurge();
        SourceDataCache &cache() const { return cache_; }
    };

    const jschar *lookup(ScriptSource *ss, const AutoSuppressPurge &asp);
    bool put(ScriptSource *ss, const jschar *chars, const AutoSuppressPurge &asp);

    void purge();

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);
};

class ScriptSource
{
    friend class SourceCompressionTask;

    union {
        // Before setSourceCopy or setSource are successfully called, this union
        // has a nullptr pointer. When the script source is ready,
        // compressedLength_ != 0 implies compressed holds the compressed data;
        // otherwise, source holds the uncompressed source. There is a special
        // pointer |emptySource| for source code for length 0.
        //
        // The only function allowed to malloc, realloc, or free the pointers in
        // this union is adjustDataSize(). Don't do it elsewhere.
        jschar *source;
        unsigned char *compressed;
    } data;
    uint32_t refs;
    uint32_t length_;
    uint32_t compressedLength_;
    char *filename_;
    jschar *displayURL_;
    jschar *sourceMapURL_;
    JSPrincipals *originPrincipals_;

    // bytecode offset in caller script that generated this code.
    // This is present for eval-ed code, as well as "new Function(...)"-introduced
    // scripts.
    uint32_t introductionOffset_;

    // If this ScriptSource was generated by a code-introduction mechanism such as |eval|
    // or |new Function|, the debugger needs access to the "raw" filename of the top-level
    // script that contains the eval-ing code.  To keep track of this, we must preserve
    // the original outermost filename (of the original introducer script), so that instead
    // of a filename of "foo.js line 30 > eval line 10 > Function", we can obtain the
    // original raw filename of "foo.js".
    char *introducerFilename_;

    // A string indicating how this source code was introduced into the system.
    // This accessor returns one of the following values:
    //      "eval" for code passed to |eval|.
    //      "Function" for code passed to the |Function| constructor.
    //      "Worker" for code loaded by calling the Web worker constructor&mdash;the worker's main script.
    //      "importScripts" for code by calling |importScripts| in a web worker.
    //      "handler" for code assigned to DOM elements' event handler IDL attributes.
    //      "scriptElement" for code belonging to <script> elements.
    //      undefined if the implementation doesn't know how the code was introduced.
    // This is a constant, statically allocated C string, so does not need
    // memory management.
    const char *introductionType_;

    // True if we can call JSRuntime::sourceHook to load the source on
    // demand. If sourceRetrievable_ and hasSourceData() are false, it is not
    // possible to get source at all.
    bool sourceRetrievable_:1;
    bool argumentsNotIncluded_:1;
    bool ready_:1;
    bool hasIntroductionOffset_:1;

  public:
    ScriptSource(JSPrincipals *originPrincipals)
      : refs(0),
        length_(0),
        compressedLength_(0),
        filename_(nullptr),
        displayURL_(nullptr),
        sourceMapURL_(nullptr),
        originPrincipals_(originPrincipals),
        introductionOffset_(0),
        introducerFilename_(nullptr),
        introductionType_(nullptr),
        sourceRetrievable_(false),
        argumentsNotIncluded_(false),
        ready_(true),
        hasIntroductionOffset_(false)
    {
        data.source = nullptr;
        if (originPrincipals_)
            JS_HoldPrincipals(originPrincipals_);
    }
    void incref() { refs++; }
    void decref() {
        JS_ASSERT(refs != 0);
        if (--refs == 0)
            destroy();
    }
    bool setSourceCopy(ExclusiveContext *cx,
                       const jschar *src,
                       uint32_t length,
                       bool argumentsNotIncluded,
                       SourceCompressionTask *tok);
    void setSource(const jschar *src, size_t length);
    bool ready() const { return ready_; }
    void setSourceRetrievable() { sourceRetrievable_ = true; }
    bool sourceRetrievable() const { return sourceRetrievable_; }
    bool hasSourceData() const { return !!data.source || !ready(); }
    uint32_t length() const {
        JS_ASSERT(hasSourceData());
        return length_;
    }
    bool argumentsNotIncluded() const {
        JS_ASSERT(hasSourceData());
        return argumentsNotIncluded_;
    }
    const jschar *chars(JSContext *cx, const SourceDataCache::AutoSuppressPurge &asp);
    JSFlatString *substring(JSContext *cx, uint32_t start, uint32_t stop);
    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

    // XDR handling
    template <XDRMode mode>
    bool performXDR(XDRState<mode> *xdr);

    bool setFilename(ExclusiveContext *cx, const char *filename);
    bool setIntroducedFilename(ExclusiveContext *cx,
                               const char *callerFilename, unsigned callerLineno,
                               const char *introductionType, const char *introducerFilename);
    const char *introducerFilename() const {
        return introducerFilename_;
    }
    bool hasIntroductionType() const {
        return introductionType_;
    }
    const char *introductionType() const {
        JS_ASSERT(hasIntroductionType());
        return introductionType_;
    }
    const char *filename() const {
        return filename_;
    }

    // Display URLs
    bool setDisplayURL(ExclusiveContext *cx, const jschar *displayURL);
    const jschar *displayURL();
    bool hasDisplayURL() const { return displayURL_ != nullptr; }

    // Source maps
    bool setSourceMapURL(ExclusiveContext *cx, const jschar *sourceMapURL);
    const jschar *sourceMapURL();
    bool hasSourceMapURL() const { return sourceMapURL_ != nullptr; }

    JSPrincipals *originPrincipals() const { return originPrincipals_; }

    bool hasIntroductionOffset() const { return hasIntroductionOffset_; }
    uint32_t introductionOffset() const {
        JS_ASSERT(hasIntroductionOffset());
        return introductionOffset_;
    }
    void setIntroductionOffset(uint32_t offset) {
        JS_ASSERT(!hasIntroductionOffset());
        JS_ASSERT(offset <= (uint32_t)INT32_MAX);
        introductionOffset_ = offset;
        hasIntroductionOffset_ = true;
    }

  private:
    void destroy();
    bool compressed() const { return compressedLength_ != 0; }
    size_t computedSizeOfData() const {
        return compressed() ? compressedLength_ : sizeof(jschar) * length_;
    }
    bool adjustDataSize(size_t nbytes);
    const jschar *getOffThreadCompressionChars(ExclusiveContext *cx);
};

class ScriptSourceHolder
{
    ScriptSource *ss;
  public:
    explicit ScriptSourceHolder(ScriptSource *ss)
      : ss(ss)
    {
        ss->incref();
    }
    ~ScriptSourceHolder()
    {
        ss->decref();
    }
};

class ScriptSourceObject : public JSObject
{
  public:
    static const Class class_;

    static void finalize(FreeOp *fop, JSObject *obj);
    static ScriptSourceObject *create(ExclusiveContext *cx, ScriptSource *source,
                                      const ReadOnlyCompileOptions &options);

    ScriptSource *source() {
        // Script source objects are immutable.
        AutoThreadSafeAccess ts0(this);
        AutoThreadSafeAccess ts1(lastProperty());
        AutoThreadSafeAccess ts2(lastProperty()->base());
        return static_cast<ScriptSource *>(getReservedSlot(SOURCE_SLOT).toPrivate());
    }

    void setSource(ScriptSource *source);

    JSObject *element() const;
    void initElement(HandleObject element);
    const Value &elementAttributeName() const;

  private:
    static const uint32_t SOURCE_SLOT = 0;
    static const uint32_t ELEMENT_SLOT = 1;
    static const uint32_t ELEMENT_PROPERTY_SLOT = 2;
    static const uint32_t RESERVED_SLOTS = 3;
};

enum GeneratorKind { NotGenerator, LegacyGenerator, StarGenerator };

static inline unsigned
GeneratorKindAsBits(GeneratorKind generatorKind) {
    return static_cast<unsigned>(generatorKind);
}

static inline GeneratorKind
GeneratorKindFromBits(unsigned val) {
    JS_ASSERT(val <= StarGenerator);
    return static_cast<GeneratorKind>(val);
}

/*
 * NB: after a successful XDR_DECODE, XDRScript callers must do any required
 * subsequent set-up of owning function or script object and then call
 * js_CallNewScriptHook.
 */
template<XDRMode mode>
bool
XDRScript(XDRState<mode> *xdr, HandleObject enclosingScope, HandleScript enclosingScript,
          HandleFunction fun, MutableHandleScript scriptp);

JSScript *
CloneScript(JSContext *cx, HandleObject enclosingScope, HandleFunction fun, HandleScript script,
            NewObjectKind newKind = GenericObject);

/*
 * Code any constant value.
 */
template<XDRMode mode>
bool
XDRScriptConst(XDRState<mode> *xdr, MutableHandleValue vp);

} /* namespace js */

class JSScript : public js::gc::BarrieredCell<JSScript>
{
    static const uint32_t stepFlagMask = 0x80000000U;
    static const uint32_t stepCountMask = 0x7fffffffU;

    template <js::XDRMode mode>
    friend
    bool
    js::XDRScript(js::XDRState<mode> *xdr, js::HandleObject enclosingScope, js::HandleScript enclosingScript,
                  js::HandleFunction fun, js::MutableHandleScript scriptp);

    friend JSScript *
    js::CloneScript(JSContext *cx, js::HandleObject enclosingScope, js::HandleFunction fun, js::HandleScript src,
                    js::NewObjectKind newKind);

  public:
    //
    // We order fields according to their size in order to avoid wasting space
    // for alignment.
    //

    // Larger-than-word-sized fields.

  public:
    js::Bindings    bindings;   /* names of top-level variables in this script
                                   (and arguments if this is a function script) */

    bool hasAnyAliasedBindings() const {
        js::AutoThreadSafeAccess ts(this);
        return bindings.hasAnyAliasedBindings();
    }

    js::Binding *bindingArray() const {
        js::AutoThreadSafeAccess ts(this);
        return bindings.bindingArray();
    }

    unsigned numArgs() const {
        js::AutoThreadSafeAccess ts(this);
        return bindings.numArgs();
    }

    js::Shape *callObjShape() const {
        js::AutoThreadSafeAccess ts(this);
        return bindings.callObjShape();
    }

    // Word-sized fields.

  private:
    jsbytecode      *code_;     /* bytecodes and their immediate operands */
  public:
    uint8_t         *data;      /* pointer to variable-length data array (see
                                   comment above Create() for details) */

    js::HeapPtrAtom *atoms;     /* maps immediate index to literal struct */

    JSCompartment   *compartment_;

    /* Persistent type information retained across GCs. */
    js::types::TypeScript *types;

  private:
    // This script's ScriptSourceObject, or a CCW thereof.
    //
    // (When we clone a JSScript into a new compartment, we don't clone its
    // source object. Instead, the clone refers to a wrapper.)
    js::HeapPtrObject sourceObject_;

    js::HeapPtrFunction function_;

    // For callsite clones, which cannot have enclosing scopes, the original
    // function; otherwise the enclosing scope
    js::HeapPtrObject   enclosingScopeOrOriginalFunction_;

    /* Information attached by Baseline/Ion for sequential mode execution. */
    js::jit::IonScript *ion;
    js::jit::BaselineScript *baseline;

    /* Information attached by Ion for parallel mode execution */
    js::jit::IonScript *parallelIon;

    /* Information used to re-lazify a lazily-parsed interpreted function. */
    js::LazyScript *lazyScript;

    /*
     * Pointer to either baseline->method()->raw() or ion->method()->raw(), or
     * nullptr if there's no Baseline or Ion script.
     */
    uint8_t *baselineOrIonRaw;
    uint8_t *baselineOrIonSkipArgCheck;

    // 32-bit fields.

    uint32_t        length_;    /* length of code vector */
    uint32_t        dataSize_;  /* size of the used part of the data array */

    uint32_t        lineno_;    /* base line number of script */
    uint32_t        column_;    /* base column of script, optionally set */

    uint32_t        mainOffset_;/* offset of main entry point from code, after
                                   predef'ing prolog */

    uint32_t        natoms_;    /* length of atoms array */
    uint32_t        nslots_;    /* vars plus maximum stack depth */

    /* Range of characters in scriptSource which contains this script's source. */
    uint32_t        sourceStart_;
    uint32_t        sourceEnd_;

    uint32_t        useCount;   /* Number of times the script has been called
                                 * or has had backedges taken. When running in
                                 * ion, also increased for any inlined scripts.
                                 * Reset if the script's JIT code is forcibly
                                 * discarded. */

#ifdef DEBUG
    // Unique identifier within the compartment for this script, used for
    // printing analysis information.
    uint32_t        id_;
    uint32_t        idpad;
#endif

    // 16-bit fields.

    uint16_t        version;    /* JS version under which script was compiled */

    uint16_t        funLength_; /* ES6 function length */

    uint16_t        nTypeSets_; /* number of type sets used in this script for
                                   dynamic type monitoring */

    uint16_t        staticLevel_;/* static level for display maintenance */

    // Bit fields.

  public:
    // The kinds of the optional arrays.
    enum ArrayKind {
        CONSTS,
        OBJECTS,
        REGEXPS,
        TRYNOTES,
        BLOCK_SCOPES,
        ARRAY_KIND_BITS
    };

  private:
    // The bits in this field indicate the presence/non-presence of several
    // optional arrays in |data|.  See the comments above Create() for details.
    uint8_t         hasArrayBits:ARRAY_KIND_BITS;

    // The GeneratorKind of the script.
    uint8_t         generatorKindBits_:2;

    // 1-bit fields.

    // No need for result value of last expression statement.
    bool noScriptRval_:1;

    // Can call getCallerFunction().
    bool savedCallerFun_:1;

    // Code is in strict mode.
    bool strict_:1;

    // Code has "use strict"; explicitly.
    bool explicitUseStrict_:1;

    // See Parser::compileAndGo.
    bool compileAndGo_:1;

    // see Parser::selfHostingMode.
    bool selfHosted_:1;

    // See FunctionContextFlags.
    bool bindingsAccessedDynamically_:1;
    bool funHasExtensibleScope_:1;
    bool funNeedsDeclEnvObject_:1;

    // True if any formalIsAliased(i).
    bool funHasAnyAliasedFormal_:1;

    // Have warned about uses of undefined properties in this script.
    bool warnedAboutUndefinedProp_:1;

    // Script has singleton objects.
    bool hasSingletons_:1;

    // Script is a lambda to treat as running once.
    bool treatAsRunOnce_:1;

    // If treatAsRunOnce, whether script has executed.
    bool hasRunOnce_:1;

    // Script has been reused for a clone.
    bool hasBeenCloned_:1;

    // Script came from eval(), and is still active.
    bool isActiveEval_:1;

    // Script came from eval(), and is in eval cache.
    bool isCachedEval_:1;

    // Set for functions defined at the top level within an 'eval' script.
    bool directlyInsideEval_:1;

    // Both 'arguments' and f.apply() are used. This is likely to be a wrapper.
    bool usesArgumentsAndApply_:1;

    /* script is attempted to be cloned anew at each callsite. This is
       temporarily needed for ParallelArray selfhosted code until type
       information can be made context sensitive. See discussion in
       bug 826148. */
    bool shouldCloneAtCallsite_:1;
    bool isCallsiteClone_:1; /* is a callsite clone; has a link to the original function */
    bool shouldInline_:1;    /* hint to inline when possible */

    // IonMonkey compilation hints.
    bool failedBoundsCheck_:1; /* script has had hoisted bounds checks fail */
    bool failedShapeGuard_:1; /* script has had hoisted shape guard fail */
    bool hadFrequentBailouts_:1;
    bool uninlineable_:1;    /* explicitly marked as uninlineable */

    // Idempotent cache has triggered invalidation.
    bool invalidatedIdempotentCache_:1;

    // If the generator was created implicitly via a generator expression,
    // isGeneratorExp will be true.
    bool isGeneratorExp_:1;

    // Script has an entry in JSCompartment::scriptCountsMap.
    bool hasScriptCounts_:1;

    // Script has an entry in JSCompartment::debugScriptMap.
    bool hasDebugScript_:1;

    // Freeze constraints for stack type sets have been generated.
    bool hasFreezeConstraints_:1;

    /* See comments below. */
    bool argsHasVarBinding_:1;
    bool needsArgsAnalysis_:1;
    bool needsArgsObj_:1;

    //
    // End of fields.  Start methods.
    //

  public:
    static JSScript *Create(js::ExclusiveContext *cx,
                            js::HandleObject enclosingScope, bool savedCallerFun,
                            const JS::ReadOnlyCompileOptions &options, unsigned staticLevel,
                            js::HandleObject sourceObject, uint32_t sourceStart,
                            uint32_t sourceEnd);

    void initCompartment(js::ExclusiveContext *cx);

    // Three ways ways to initialize a JSScript. Callers of partiallyInit()
    // and fullyInitTrivial() are responsible for notifying the debugger after
    // successfully creating any kind (function or other) of new JSScript.
    // However, callers of fullyInitFromEmitter() do not need to do this.
    static bool partiallyInit(js::ExclusiveContext *cx, JS::Handle<JSScript*> script,
                              uint32_t nconsts, uint32_t nobjects, uint32_t nregexps,
                              uint32_t ntrynotes, uint32_t nblockscopes,
                              uint32_t nTypeSets);
    static bool fullyInitFromEmitter(js::ExclusiveContext *cx, JS::Handle<JSScript*> script,
                                     js::frontend::BytecodeEmitter *bce);
    // Initialize a no-op script.
    static bool fullyInitTrivial(js::ExclusiveContext *cx, JS::Handle<JSScript*> script);

    inline JSPrincipals *principals();

    JSCompartment *compartment() const { return compartment_; }

    void setVersion(JSVersion v) { version = v; }

    // Script bytecode is immutable after creation.
    jsbytecode *code() const {
        js::AutoThreadSafeAccess ts(this);
        return code_;
    }
    size_t length() const {
        js::AutoThreadSafeAccess ts(this);
        return length_;
    }

    void setCode(jsbytecode *code) { code_ = code; }
    void setLength(size_t length) { length_ = length; }

    jsbytecode *codeEnd() const { return code() + length(); }

    bool containsPC(const jsbytecode *pc) const {
        return pc >= code() && pc < codeEnd();
    }

    size_t pcToOffset(const jsbytecode *pc) const {
        JS_ASSERT(containsPC(pc));
        return size_t(pc - code());
    }

    jsbytecode *offsetToPC(size_t offset) const {
        JS_ASSERT(offset < length());
        return code() + offset;
    }

    size_t mainOffset() const {
        js::AutoThreadSafeAccess ts(this);
        return mainOffset_;
    }

    size_t lineno() const {
        js::AutoThreadSafeAccess ts(this);
        return lineno_;
    }

    size_t column() const {
        js::AutoThreadSafeAccess ts(this);
        return column_;
    }

    void setColumn(size_t column) { column_ = column; }

    size_t nfixed() const {
        js::AutoThreadSafeAccess ts(this);
        return function_ ? bindings.numVars() : 0;
    }

    size_t nslots() const {
        js::AutoThreadSafeAccess ts(this);
        return nslots_;
    }

    size_t staticLevel() const {
        js::AutoThreadSafeAccess ts(this);
        return staticLevel_;
    }

    size_t nTypeSets() const {
        js::AutoThreadSafeAccess ts(this);
        return nTypeSets_;
    }

    size_t funLength() const {
        js::AutoThreadSafeAccess ts(this);
        return funLength_;
    }

    size_t sourceStart() const {
        js::AutoThreadSafeAccess ts(this);
        return sourceStart_;
    }

    size_t sourceEnd() const {
        js::AutoThreadSafeAccess ts(this);
        return sourceEnd_;
    }

    bool noScriptRval() const {
        js::AutoThreadSafeAccess ts(this);
        return noScriptRval_;
    }

    bool savedCallerFun() const { return savedCallerFun_; }

    bool strict() const {
        js::AutoThreadSafeAccess ts(this);
        return strict_;
    }

    bool explicitUseStrict() const { return explicitUseStrict_; }

    bool compileAndGo() const {
        js::AutoThreadSafeAccess ts(this);
        return compileAndGo_;
    }

    bool selfHosted() const { return selfHosted_; }
    bool bindingsAccessedDynamically() const { return bindingsAccessedDynamically_; }
    bool funHasExtensibleScope() const {
        js::AutoThreadSafeAccess ts(this);
        return funHasExtensibleScope_;
    }
    bool funNeedsDeclEnvObject() const {
        js::AutoThreadSafeAccess ts(this);
        return funNeedsDeclEnvObject_;
    }
    bool funHasAnyAliasedFormal() const {
        js::AutoThreadSafeAccess ts(this);
        return funHasAnyAliasedFormal_;
    }

    bool hasSingletons() const { return hasSingletons_; }
    bool treatAsRunOnce() const {
        js::AutoThreadSafeAccess ts(this);
        return treatAsRunOnce_;
    }
    bool hasRunOnce() const { return hasRunOnce_; }
    bool hasBeenCloned() const { return hasBeenCloned_; }

    void setTreatAsRunOnce() { treatAsRunOnce_ = true; }
    void setHasRunOnce() { hasRunOnce_ = true; }
    void setHasBeenCloned() { hasBeenCloned_ = true; }

    bool isActiveEval() const { return isActiveEval_; }
    bool isCachedEval() const { return isCachedEval_; }
    bool directlyInsideEval() const { return directlyInsideEval_; }

    void cacheForEval() {
        JS_ASSERT(isActiveEval() && !isCachedEval());
        isActiveEval_ = false;
        isCachedEval_ = true;
    }

    void uncacheForEval() {
        JS_ASSERT(isCachedEval() && !isActiveEval());
        isCachedEval_ = false;
        isActiveEval_ = true;
    }

    void setActiveEval() { isActiveEval_ = true; }
    void setDirectlyInsideEval() { directlyInsideEval_ = true; }

    bool usesArgumentsAndApply() const {
        js::AutoThreadSafeAccess ts(this);
        return usesArgumentsAndApply_;
    }
    void setUsesArgumentsAndApply() { usesArgumentsAndApply_ = true; }

    bool shouldCloneAtCallsite() const {
        js::AutoThreadSafeAccess ts(this);
        return shouldCloneAtCallsite_;
    }
    bool shouldInline() const {
        js::AutoThreadSafeAccess ts(this);
        return shouldInline_;
    }

    void setShouldCloneAtCallsite() { shouldCloneAtCallsite_ = true; }
    void setShouldInline() { shouldInline_ = true; }

    bool isCallsiteClone() const {
        js::AutoThreadSafeAccess ts(this);
        return isCallsiteClone_;
    }
    bool isGeneratorExp() const { return isGeneratorExp_; }

    bool failedBoundsCheck() const {
        js::AutoThreadSafeAccess ts(this);
        return failedBoundsCheck_;
    }
    bool failedShapeGuard() const {
        js::AutoThreadSafeAccess ts(this);
        return failedShapeGuard_;
    }
    bool hadFrequentBailouts() const {
        js::AutoThreadSafeAccess ts(this);
        return hadFrequentBailouts_;
    }
    bool uninlineable() const {
        js::AutoThreadSafeAccess ts(this);
        return uninlineable_;
    }
    bool invalidatedIdempotentCache() const {
        js::AutoThreadSafeAccess ts(this);
        return invalidatedIdempotentCache_;
    }

    void setFailedBoundsCheck() { failedBoundsCheck_ = true; }
    void setFailedShapeGuard() { failedShapeGuard_ = true; }
    void setHadFrequentBailouts() { hadFrequentBailouts_ = true; }
    void setUninlineable() { uninlineable_ = true; }
    void setInvalidatedIdempotentCache() { invalidatedIdempotentCache_ = true; }

    bool hasScriptCounts() const { return hasScriptCounts_; }

    bool hasFreezeConstraints() const { return hasFreezeConstraints_; }
    void setHasFreezeConstraints() { hasFreezeConstraints_ = true; }
    void clearHasFreezeConstraints() { hasFreezeConstraints_ = false; }

    bool warnedAboutUndefinedProp() const { return warnedAboutUndefinedProp_; }
    void setWarnedAboutUndefinedProp() { warnedAboutUndefinedProp_ = true; }

    /* See ContextFlags::funArgumentsHasLocalBinding comment. */
    bool argumentsHasVarBinding() const {
        js::AutoThreadSafeAccess ts(this);
        return argsHasVarBinding_;
    }
    jsbytecode *argumentsBytecode() const { JS_ASSERT(code()[0] == JSOP_ARGUMENTS); return code(); }
    void setArgumentsHasVarBinding();
    bool argumentsAliasesFormals() const {
        return argumentsHasVarBinding() && !strict();
    }

    js::GeneratorKind generatorKind() const {
        js::AutoThreadSafeAccess ts(this);
        return js::GeneratorKindFromBits(generatorKindBits_);
    }
    bool isGenerator() const { return generatorKind() != js::NotGenerator; }
    bool isLegacyGenerator() const { return generatorKind() == js::LegacyGenerator; }
    bool isStarGenerator() const { return generatorKind() == js::StarGenerator; }
    void setGeneratorKind(js::GeneratorKind kind) {
        // A script only gets its generator kind set as part of initialization,
        // so it can only transition from not being a generator.
        JS_ASSERT(!isGenerator());
        generatorKindBits_ = GeneratorKindAsBits(kind);
    }

    /*
     * As an optimization, even when argsHasLocalBinding, the function prologue
     * may not need to create an arguments object. This is determined by
     * needsArgsObj which is set by ScriptAnalysis::analyzeSSA before running
     * the script the first time. When !needsArgsObj, the prologue may simply
     * write MagicValue(JS_OPTIMIZED_ARGUMENTS) to 'arguments's slot and any
     * uses of 'arguments' will be guaranteed to handle this magic value.
     * So avoid spurious arguments object creation, we maintain the invariant
     * that needsArgsObj is only called after the script has been analyzed.
     */
    bool analyzedArgsUsage() const { return !needsArgsAnalysis_; }
    bool needsArgsObj() const {
        js::AutoThreadSafeAccess ts(this);
        JS_ASSERT(js::CurrentThreadCanReadCompilationData());
        JS_ASSERT(analyzedArgsUsage());
        return needsArgsObj_;
    }
    void setNeedsArgsObj(bool needsArgsObj);
    static bool argumentsOptimizationFailed(JSContext *cx, js::HandleScript script);

    /*
     * Arguments access (via JSOP_*ARG* opcodes) must access the canonical
     * location for the argument. If an arguments object exists AND this is a
     * non-strict function (where 'arguments' aliases formals), then all access
     * must go through the arguments object. Otherwise, the local slot is the
     * canonical location for the arguments. Note: if a formal is aliased
     * through the scope chain, then script->formalIsAliased and JSOP_*ARG*
     * opcodes won't be emitted at all.
     */
    bool argsObjAliasesFormals() const {
        return needsArgsObj() && !strict();
    }

    bool hasAnyIonScript() const {
        return hasIonScript() || hasParallelIonScript();
    }

    bool hasIonScript() const {
        // Note: While a script's baseline script is protected by the
        // compilation lock, writes to the ion script are not. This helps lock
        // ordering issues in CodeGenerator::link. Tests of script->ion during
        // off thread compilation can therefore race, though these are fairly
        // benign and the IonScript itself is never accessed.
        js::AutoThreadSafeAccess ts(this);
        return ion && ion != ION_DISABLED_SCRIPT && ion != ION_COMPILING_SCRIPT;
    }
    bool canIonCompile() const {
        // Note: see above comment.
        js::AutoThreadSafeAccess ts(this);
        return ion != ION_DISABLED_SCRIPT;
    }

    bool isIonCompilingOffThread() const {
        return ion == ION_COMPILING_SCRIPT;
    }

    js::jit::IonScript *ionScript() const {
        JS_ASSERT(hasIonScript());
        return ion;
    }
    js::jit::IonScript *maybeIonScript() const {
        return ion;
    }
    js::jit::IonScript *const *addressOfIonScript() const {
        return &ion;
    }
    void setIonScript(js::jit::IonScript *ionScript) {
        if (hasIonScript())
            js::jit::IonScript::writeBarrierPre(tenuredZone(), ion);
        ion = ionScript;
        updateBaselineOrIonRaw();
    }

    bool hasBaselineScript() const {
        JS_ASSERT(js::CurrentThreadCanReadCompilationData());
        js::AutoThreadSafeAccess ts(this);
        return baseline && baseline != BASELINE_DISABLED_SCRIPT;
    }
    bool canBaselineCompile() const {
        return baseline != BASELINE_DISABLED_SCRIPT;
    }
    js::jit::BaselineScript *baselineScript() const {
        JS_ASSERT(hasBaselineScript());
        js::AutoThreadSafeAccess ts(this);
        return baseline;
    }
    inline void setBaselineScript(JSContext *maybecx, js::jit::BaselineScript *baselineScript);

    void updateBaselineOrIonRaw();

    bool hasParallelIonScript() const {
        return parallelIon && parallelIon != ION_DISABLED_SCRIPT && parallelIon != ION_COMPILING_SCRIPT;
    }

    bool canParallelIonCompile() const {
        js::AutoThreadSafeAccess ts(this);
        return parallelIon != ION_DISABLED_SCRIPT;
    }

    bool isParallelIonCompilingOffThread() const {
        return parallelIon == ION_COMPILING_SCRIPT;
    }

    js::jit::IonScript *parallelIonScript() const {
        JS_ASSERT(hasParallelIonScript());
        return parallelIon;
    }
    js::jit::IonScript *maybeParallelIonScript() const {
        return parallelIon;
    }
    void setParallelIonScript(js::jit::IonScript *ionScript) {
        if (hasParallelIonScript())
            js::jit::IonScript::writeBarrierPre(tenuredZone(), parallelIon);
        parallelIon = ionScript;
    }

    static size_t offsetOfBaselineScript() {
        return offsetof(JSScript, baseline);
    }
    static size_t offsetOfIonScript() {
        return offsetof(JSScript, ion);
    }
    static size_t offsetOfParallelIonScript() {
        return offsetof(JSScript, parallelIon);
    }
    static size_t offsetOfBaselineOrIonRaw() {
        return offsetof(JSScript, baselineOrIonRaw);
    }
    static size_t offsetOfBaselineOrIonSkipArgCheck() {
        return offsetof(JSScript, baselineOrIonSkipArgCheck);
    }

    bool isRelazifiable() const {
        return (selfHosted() || lazyScript) &&
               !isGenerator() && !hasBaselineScript() && !hasAnyIonScript();
    }
    void setLazyScript(js::LazyScript *lazy) {
        lazyScript = lazy;
    }
    js::LazyScript *maybeLazyScript() {
        return lazyScript;
    }

    /*
     * Original compiled function for the script, if it has a function.
     * nullptr for global and eval scripts.
     * The delazifying variant ensures that the function isn't lazy, but can
     * only be used under a compilation lock. The non-delazifying variant
     * can be used off-thread and without the lock, but must only be used
     * after earlier code has called ensureNonLazyCanonicalFunction and
     * while the function can't have been relazified.
     */
    inline JSFunction *functionDelazifying() const;
    JSFunction *functionNonDelazifying() const {
        js::AutoThreadSafeAccess ts(this);
        return function_;
    }
    inline void setFunction(JSFunction *fun);
    /*
     * Takes a compilation lock and de-lazifies the canonical function. Must
     * be called before entering code that expects the function to be non-lazy.
     */
    inline void ensureNonLazyCanonicalFunction(JSContext *cx);

    /*
     * Donor provided itself to callsite clone; null if this is non-clone.
     */
    JSFunction *donorFunction() const;
    void setIsCallsiteClone(JSObject *fun);

    JSFlatString *sourceData(JSContext *cx);

    static bool loadSource(JSContext *cx, js::ScriptSource *ss, bool *worked);

    void setSourceObject(JSObject *object);
    JSObject *sourceObject() const {
        js::AutoThreadSafeAccess ts(this);
        return sourceObject_;
    }
    js::ScriptSource *scriptSource() const;
    JSPrincipals *originPrincipals() const { return scriptSource()->originPrincipals(); }
    const char *filename() const { return scriptSource()->filename(); }

  public:

    /* Return whether this script was compiled for 'eval' */
    bool isForEval() { return isCachedEval() || isActiveEval(); }

#ifdef DEBUG
    unsigned id();
#else
    unsigned id() { return 0; }
#endif

    /* Ensure the script has a TypeScript. */
    inline bool ensureHasTypes(JSContext *cx);

    /*
     * Ensure the script has bytecode analysis information. Performed when the
     * script first runs, or first runs after a TypeScript GC purge.
     */
    inline bool ensureRanAnalysis(JSContext *cx);

    /* Ensure the script has type inference analysis information. */
    inline bool ensureRanInference(JSContext *cx);

    inline bool hasAnalysis();
    inline void clearAnalysis();
    inline js::analyze::ScriptAnalysis *analysis();

    inline js::GlobalObject &global() const;
    js::GlobalObject &uninlinedGlobal() const;

    /* See StaticScopeIter comment. */
    JSObject *enclosingStaticScope() const {
        if (isCallsiteClone())
            return nullptr;
        js::AutoThreadSafeAccess ts(this);
        return enclosingScopeOrOriginalFunction_;
    }

  private:
    bool makeTypes(JSContext *cx);
    bool makeAnalysis(JSContext *cx);

  public:
    uint32_t getUseCount() const {
        // Note: We ignore races when reading the use count of a script off thread.
        js::AutoThreadSafeAccess ts(this);
        return useCount;
    }
    uint32_t incUseCount(uint32_t amount = 1) { return useCount += amount; }
    uint32_t *addressOfUseCount() { return &useCount; }
    static size_t offsetOfUseCount() { return offsetof(JSScript, useCount); }
    void resetUseCount() { useCount = 0; }

  public:
    bool initScriptCounts(JSContext *cx);
    js::PCCounts getPCCounts(jsbytecode *pc);
    void addIonCounts(js::jit::IonScriptCounts *ionCounts);
    js::jit::IonScriptCounts *getIonCounts();
    js::ScriptCounts releaseScriptCounts();
    void destroyScriptCounts(js::FreeOp *fop);

    jsbytecode *main() {
        return code() + mainOffset();
    }

    /*
     * computedSizeOfData() is the in-use size of all the data sections.
     * sizeOfData() is the size of the block allocated to hold all the data
     * sections (which can be larger than the in-use size).
     */
    size_t computedSizeOfData() const;
    size_t sizeOfData(mozilla::MallocSizeOf mallocSizeOf) const;
    size_t sizeOfTypeScript(mozilla::MallocSizeOf mallocSizeOf) const;

    uint32_t numNotes();  /* Number of srcnote slots in the srcnotes section */

    /* Script notes are allocated right after the code. */
    jssrcnote *notes() { return (jssrcnote *)(code() + length()); }

    bool hasArray(ArrayKind kind) {
        js::AutoThreadSafeAccess ts(this);
        return (hasArrayBits & (1 << kind));
    }
    void setHasArray(ArrayKind kind) { hasArrayBits |= (1 << kind); }
    void cloneHasArray(JSScript *script) { hasArrayBits = script->hasArrayBits; }

    bool hasConsts()        { return hasArray(CONSTS);      }
    bool hasObjects()       { return hasArray(OBJECTS);     }
    bool hasRegexps()       { return hasArray(REGEXPS);     }
    bool hasTrynotes()      { return hasArray(TRYNOTES);    }
    bool hasBlockScopes()   { return hasArray(BLOCK_SCOPES); }

    #define OFF(fooOff, hasFoo, t)   (fooOff() + (hasFoo() ? sizeof(t) : 0))

    size_t constsOffset()     { return 0; }
    size_t objectsOffset()    { return OFF(constsOffset,     hasConsts,     js::ConstArray);      }
    size_t regexpsOffset()    { return OFF(objectsOffset,    hasObjects,    js::ObjectArray);     }
    size_t trynotesOffset()   { return OFF(regexpsOffset,    hasRegexps,    js::ObjectArray);     }
    size_t blockScopesOffset(){ return OFF(trynotesOffset,   hasTrynotes,   js::TryNoteArray);    }

    size_t dataSize() const { return dataSize_; }

    js::ConstArray *consts() {
        JS_ASSERT(hasConsts());
        js::AutoThreadSafeAccess ts(this);
        return reinterpret_cast<js::ConstArray *>(data + constsOffset());
    }

    js::ObjectArray *objects() {
        JS_ASSERT(hasObjects());
        js::AutoThreadSafeAccess ts(this);
        return reinterpret_cast<js::ObjectArray *>(data + objectsOffset());
    }

    js::ObjectArray *regexps() {
        JS_ASSERT(hasRegexps());
        js::AutoThreadSafeAccess ts(this);
        return reinterpret_cast<js::ObjectArray *>(data + regexpsOffset());
    }

    js::TryNoteArray *trynotes() {
        JS_ASSERT(hasTrynotes());
        js::AutoThreadSafeAccess ts(this);
        return reinterpret_cast<js::TryNoteArray *>(data + trynotesOffset());
    }

    js::BlockScopeArray *blockScopes() {
        JS_ASSERT(hasBlockScopes());
        js::AutoThreadSafeAccess ts(this);
        return reinterpret_cast<js::BlockScopeArray *>(data + blockScopesOffset());
    }

    bool hasLoops();

    size_t natoms() const { return natoms_; }

    js::HeapPtrAtom &getAtom(size_t index) const {
        js::AutoThreadSafeAccess ts(this);
        JS_ASSERT(index < natoms());
        return atoms[index];
    }

    js::HeapPtrAtom &getAtom(jsbytecode *pc) const {
        JS_ASSERT(containsPC(pc) && containsPC(pc + sizeof(uint32_t)));
        return getAtom(GET_UINT32_INDEX(pc));
    }

    js::PropertyName *getName(size_t index) {
        return getAtom(index)->asPropertyName();
    }

    js::PropertyName *getName(jsbytecode *pc) const {
        JS_ASSERT(containsPC(pc) && containsPC(pc + sizeof(uint32_t)));
        return getAtom(GET_UINT32_INDEX(pc))->asPropertyName();
    }

    JSObject *getObject(size_t index) {
        js::ObjectArray *arr = objects();
        JS_ASSERT(index < arr->length);
        return arr->vector[index];
    }

    size_t innerObjectsStart() {
        // The first object contains the caller if savedCallerFun is used.
        return savedCallerFun() ? 1 : 0;
    }

    JSObject *getObject(jsbytecode *pc) {
        JS_ASSERT(containsPC(pc) && containsPC(pc + sizeof(uint32_t)));
        return getObject(GET_UINT32_INDEX(pc));
    }

    JSVersion getVersion() const {
        return JSVersion(version);
    }

    inline JSFunction *getFunction(size_t index);
    inline JSFunction *getCallerFunction();
    inline JSFunction *functionOrCallerFunction();

    inline js::RegExpObject *getRegExp(size_t index);
    inline js::RegExpObject *getRegExp(jsbytecode *pc);

    const js::Value &getConst(size_t index) {
        js::ConstArray *arr = consts();
        JS_ASSERT(index < arr->length);
        return arr->vector[index];
    }

    js::NestedScopeObject *getStaticScope(jsbytecode *pc);

    /*
     * The isEmpty method tells whether this script has code that computes any
     * result (not return value, result AKA normal completion value) other than
     * JSVAL_VOID, or any other effects.
     */
    bool isEmpty() const {
        if (length() > 3)
            return false;

        jsbytecode *pc = code();
        if (noScriptRval() && JSOp(*pc) == JSOP_FALSE)
            ++pc;
        return JSOp(*pc) == JSOP_RETRVAL;
    }

    bool varIsAliased(uint32_t varSlot);
    bool formalIsAliased(unsigned argSlot);
    bool formalLivesInArgumentsObject(unsigned argSlot);

  private:
    /*
     * Recompile with or without single-stepping support, as directed
     * by stepModeEnabled().
     */
    void recompileForStepMode(js::FreeOp *fop);

    /* Attempt to change this->stepMode to |newValue|. */
    bool tryNewStepMode(JSContext *cx, uint32_t newValue);

    bool ensureHasDebugScript(JSContext *cx);
    js::DebugScript *debugScript();
    js::DebugScript *releaseDebugScript();
    void destroyDebugScript(js::FreeOp *fop);

  public:
    bool hasBreakpointsAt(jsbytecode *pc);
    bool hasAnyBreakpointsOrStepMode() { return hasDebugScript_; }

    js::BreakpointSite *getBreakpointSite(jsbytecode *pc)
    {
        return hasDebugScript_ ? debugScript()->breakpoints[pcToOffset(pc)] : nullptr;
    }

    js::BreakpointSite *getOrCreateBreakpointSite(JSContext *cx, jsbytecode *pc);

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

    bool stepModeEnabled() { return hasDebugScript_ && !!debugScript()->stepMode; }

#ifdef DEBUG
    uint32_t stepModeCount() { return hasDebugScript_ ? (debugScript()->stepMode & stepCountMask) : 0; }
#endif

    void finalize(js::FreeOp *fop);

    static inline js::ThingRootKind rootKind() { return js::THING_ROOT_SCRIPT; }

    void markChildren(JSTracer *trc);
};

/* If this fails, add/remove padding within JSScript. */
static_assert(sizeof(JSScript) % js::gc::CellSize == 0,
              "Size of JSScript must be an integral multiple of js::gc::CellSize");

namespace js {

/*
 * Iterator over a script's bindings (formals and variables).
 * The order of iteration is:
 *  - first, formal arguments, from index 0 to numArgs
 *  - next, variables, from index 0 to numVars
 */
class BindingIter
{
    const InternalBindingsHandle bindings_;
    uint32_t i_;

    friend class Bindings;

  public:
    explicit BindingIter(const InternalBindingsHandle &bindings) : bindings_(bindings), i_(0) {}
    explicit BindingIter(const HandleScript &script) : bindings_(script, &script->bindings), i_(0) {}

    bool done() const { return i_ == bindings_->count(); }
    operator bool() const { return !done(); }
    void operator++(int) { JS_ASSERT(!done()); i_++; }
    BindingIter &operator++() { (*this)++; return *this; }

    uint32_t frameIndex() const {
        JS_ASSERT(!done());
        return i_ < bindings_->numArgs() ? i_ : i_ - bindings_->numArgs();
    }

    const Binding &operator*() const { JS_ASSERT(!done()); return bindings_->bindingArray()[i_]; }
    const Binding *operator->() const { JS_ASSERT(!done()); return &bindings_->bindingArray()[i_]; }
};

/*
 * This helper function fills the given BindingVector with the sequential
 * values of BindingIter.
 */

typedef Vector<Binding, 32> BindingVector;

extern bool
FillBindingVector(HandleScript fromScript, BindingVector *vec);

/*
 * Iterator over the aliased formal bindings in ascending index order. This can
 * be veiwed as a filtering of BindingIter with predicate
 *   bi->aliased() && bi->kind() == ARGUMENT
 */
class AliasedFormalIter
{
    const Binding *begin_, *p_, *end_;
    unsigned slot_;

    void settle() {
        while (p_ != end_ && !p_->aliased())
            p_++;
    }

  public:
    explicit inline AliasedFormalIter(JSScript *script);

    bool done() const { return p_ == end_; }
    operator bool() const { return !done(); }
    void operator++(int) { JS_ASSERT(!done()); p_++; slot_++; settle(); }

    const Binding &operator*() const { JS_ASSERT(!done()); return *p_; }
    const Binding *operator->() const { JS_ASSERT(!done()); return p_; }
    unsigned frameIndex() const { JS_ASSERT(!done()); return p_ - begin_; }
    unsigned scopeSlot() const { JS_ASSERT(!done()); return slot_; }
};

// Information about a script which may be (or has been) lazily compiled to
// bytecode from its source.
class LazyScript : public gc::BarrieredCell<LazyScript>
{
    // If non-nullptr, the script has been compiled and this is a forwarding
    // pointer to the result.
    HeapPtrScript script_;

    // Original function with which the lazy script is associated.
    HeapPtrFunction function_;

    // Function or block chain in which the script is nested, or nullptr.
    HeapPtrObject enclosingScope_;

    // ScriptSourceObject, or nullptr if the script in which this is nested
    // has not been compiled yet. This is never a CCW; we don't clone
    // LazyScripts into other compartments.
    HeapPtrObject sourceObject_;

    // Heap allocated table with any free variables or inner functions.
    void *table_;

#if JS_BITS_PER_WORD == 32
    uint32_t padding;
#endif

    // Assorted bits that should really be in ScriptSourceObject.
    uint32_t version_ : 8;

    uint32_t numFreeVariables_ : 24;
    uint32_t numInnerFunctions_ : 23;

    uint32_t generatorKindBits_:2;

    // N.B. These are booleans but need to be uint32_t to pack correctly on MSVC.
    uint32_t strict_ : 1;
    uint32_t bindingsAccessedDynamically_ : 1;
    uint32_t hasDebuggerStatement_ : 1;
    uint32_t directlyInsideEval_:1;
    uint32_t usesArgumentsAndApply_:1;
    uint32_t hasBeenCloned_:1;
    uint32_t treatAsRunOnce_:1;

    // Source location for the script.
    uint32_t begin_;
    uint32_t end_;
    uint32_t lineno_;
    uint32_t column_;

    LazyScript(JSFunction *fun, void *table,
               uint32_t numFreeVariables, uint32_t numInnerFunctions, JSVersion version,
               uint32_t begin, uint32_t end, uint32_t lineno, uint32_t column);

  public:
    static LazyScript *Create(ExclusiveContext *cx, HandleFunction fun,
                              uint32_t numFreeVariables, uint32_t numInnerFunctions,
                              JSVersion version, uint32_t begin, uint32_t end,
                              uint32_t lineno, uint32_t column);

    inline JSFunction *functionDelazifying(JSContext *cx) const;
    JSFunction *functionNonDelazifying() const {
        return function_;
    }

    void initScript(JSScript *script);
    void resetScript();
    JSScript *maybeScript() {
        return script_;
    }

    JSObject *enclosingScope() const {
        return enclosingScope_;
    }
    ScriptSourceObject *sourceObject() const;
    ScriptSource *scriptSource() const {
        return sourceObject()->source();
    }
    JSPrincipals *originPrincipals() const {
        return scriptSource()->originPrincipals();
    }
    JSVersion version() const {
        JS_STATIC_ASSERT(JSVERSION_UNKNOWN == -1);
        return (version_ == JS_BIT(8) - 1) ? JSVERSION_UNKNOWN : JSVersion(version_);
    }

    void setParent(JSObject *enclosingScope, ScriptSourceObject *sourceObject);

    uint32_t numFreeVariables() const {
        return numFreeVariables_;
    }
    HeapPtrAtom *freeVariables() {
        return (HeapPtrAtom *)table_;
    }

    uint32_t numInnerFunctions() const {
        return numInnerFunctions_;
    }
    HeapPtrFunction *innerFunctions() {
        return (HeapPtrFunction *)&freeVariables()[numFreeVariables()];
    }

    GeneratorKind generatorKind() const { return GeneratorKindFromBits(generatorKindBits_); }

    bool isGenerator() const { return generatorKind() != NotGenerator; }

    bool isLegacyGenerator() const { return generatorKind() == LegacyGenerator; }

    bool isStarGenerator() const { return generatorKind() == StarGenerator; }

    void setGeneratorKind(GeneratorKind kind) {
        // A script only gets its generator kind set as part of initialization,
        // so it can only transition from NotGenerator.
        JS_ASSERT(!isGenerator());
        // Legacy generators cannot currently be lazy.
        JS_ASSERT(kind != LegacyGenerator);
        generatorKindBits_ = GeneratorKindAsBits(kind);
    }

    bool strict() const {
        return strict_;
    }
    void setStrict() {
        strict_ = true;
    }

    bool bindingsAccessedDynamically() const {
        return bindingsAccessedDynamically_;
    }
    void setBindingsAccessedDynamically() {
        bindingsAccessedDynamically_ = true;
    }

    bool hasDebuggerStatement() const {
        return hasDebuggerStatement_;
    }
    void setHasDebuggerStatement() {
        hasDebuggerStatement_ = true;
    }

    bool directlyInsideEval() const {
        return directlyInsideEval_;
    }
    void setDirectlyInsideEval() {
        directlyInsideEval_ = true;
    }

    bool usesArgumentsAndApply() const {
        AutoThreadSafeAccess ts(this);
        return usesArgumentsAndApply_;
    }
    void setUsesArgumentsAndApply() {
        usesArgumentsAndApply_ = true;
    }

    bool hasBeenCloned() const {
        return hasBeenCloned_;
    }
    void setHasBeenCloned() {
        hasBeenCloned_ = true;
    }

    bool treatAsRunOnce() const {
        return treatAsRunOnce_;
    }
    void setTreatAsRunOnce() {
        treatAsRunOnce_ = true;
    }

    ScriptSource *source() const {
        return sourceObject()->source();
    }
    uint32_t begin() const {
        AutoThreadSafeAccess ts(this);
        return begin_;
    }
    uint32_t end() const {
        AutoThreadSafeAccess ts(this);
        return end_;
    }
    uint32_t lineno() const {
        return lineno_;
    }
    uint32_t column() const {
        return column_;
    }

    uint32_t staticLevel(JSContext *cx) const;

    void markChildren(JSTracer *trc);
    void finalize(js::FreeOp *fop);

    static inline js::ThingRootKind rootKind() { return js::THING_ROOT_LAZY_SCRIPT; }

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
    {
        return mallocSizeOf(table_);
    }
};

/* If this fails, add/remove padding within LazyScript. */
JS_STATIC_ASSERT(sizeof(LazyScript) % js::gc::CellSize == 0);

/*
 * New-script-hook calling is factored from JSScript::fullyInitFromEmitter() so
 * that it and callers of XDRScript() can share this code.  In the case of
 * callers of XDRScript(), the hook should be invoked only after successful
 * decode of any owning function (the fun parameter) or script object (null
 * fun).
 */
extern void
CallNewScriptHook(JSContext *cx, JS::HandleScript script, JS::HandleFunction fun);

extern void
CallDestroyScriptHook(FreeOp *fop, JSScript *script);

struct SharedScriptData
{
    uint32_t length;
    uint32_t natoms;
    bool marked;
    jsbytecode data[1];

    static SharedScriptData *new_(ExclusiveContext *cx, uint32_t codeLength,
                                  uint32_t srcnotesLength, uint32_t natoms);

    HeapPtrAtom *atoms() {
        if (!natoms)
            return nullptr;
        return reinterpret_cast<HeapPtrAtom *>(data + length - sizeof(JSAtom *) * natoms);
    }

    static SharedScriptData *fromBytecode(const jsbytecode *bytecode) {
        return (SharedScriptData *)(bytecode - offsetof(SharedScriptData, data));
    }

  private:
    SharedScriptData() MOZ_DELETE;
    SharedScriptData(const SharedScriptData&) MOZ_DELETE;
};

struct ScriptBytecodeHasher
{
    struct Lookup
    {
        jsbytecode          *code;
        uint32_t            length;

        Lookup(SharedScriptData *ssd) : code(ssd->data), length(ssd->length) {}
    };
    static HashNumber hash(const Lookup &l) { return mozilla::HashBytes(l.code, l.length); }
    static bool match(SharedScriptData *entry, const Lookup &lookup) {
        if (entry->length != lookup.length)
            return false;
        return mozilla::PodEqual<jsbytecode>(entry->data, lookup.code, lookup.length);
    }
};

typedef HashSet<SharedScriptData*,
                ScriptBytecodeHasher,
                SystemAllocPolicy> ScriptDataTable;

extern void
UnmarkScriptData(JSRuntime *rt);

extern void
SweepScriptData(JSRuntime *rt);

extern void
FreeScriptData(JSRuntime *rt);

struct ScriptAndCounts
{
    /* This structure is stored and marked from the JSRuntime. */
    JSScript *script;
    ScriptCounts scriptCounts;

    PCCounts &getPCCounts(jsbytecode *pc) const {
        return scriptCounts.pcCountsVector[script->pcToOffset(pc)];
    }

    jit::IonScriptCounts *getIonCounts() const {
        return scriptCounts.ionCounts;
    }
};

struct GSNCache;

jssrcnote *
GetSrcNote(GSNCache &cache, JSScript *script, jsbytecode *pc);

} /* namespace js */

extern jssrcnote *
js_GetSrcNote(JSContext *cx, JSScript *script, jsbytecode *pc);

extern jsbytecode *
js_LineNumberToPC(JSScript *script, unsigned lineno);

extern JS_FRIEND_API(unsigned)
js_GetScriptLineExtent(JSScript *script);

namespace js {

extern unsigned
PCToLineNumber(JSScript *script, jsbytecode *pc, unsigned *columnp = nullptr);

extern unsigned
PCToLineNumber(unsigned startLine, jssrcnote *notes, jsbytecode *code, jsbytecode *pc,
               unsigned *columnp = nullptr);

/*
 * This function returns the file and line number of the script currently
 * executing on cx. If there is no current script executing on cx (e.g., a
 * native called directly through JSAPI (e.g., by setTimeout)), nullptr and 0
 * are returned as the file and line. Additionally, this function avoids the
 * full linear scan to compute line number when the caller guarantees that the
 * script compilation occurs at a JSOP_EVAL/JSOP_SPREADEVAL.
 */

enum LineOption {
    CALLED_FROM_JSOP_EVAL,
    NOT_CALLED_FROM_JSOP_EVAL
};

extern void
CurrentScriptFileLineOrigin(JSContext *cx, JSScript **script,
                            const char **file, unsigned *linenop,
                            uint32_t *pcOffset, JSPrincipals **origin,
                            LineOption opt = NOT_CALLED_FROM_JSOP_EVAL);

bool
CloneFunctionScript(JSContext *cx, HandleFunction original, HandleFunction clone,
                    NewObjectKind newKind = GenericObject);

/*
 * JSAPI clients are allowed to leave CompileOptions.originPrincipals nullptr in
 * which case the JS engine sets options.originPrincipals = origin.principals.
 * This normalization step must occur before the originPrincipals get stored in
 * the JSScript/ScriptSource.
 */

static inline JSPrincipals *
NormalizeOriginPrincipals(JSPrincipals *principals, JSPrincipals *originPrincipals)
{
    return originPrincipals ? originPrincipals : principals;
}

} /* namespace js */

#endif /* jsscript_h */
