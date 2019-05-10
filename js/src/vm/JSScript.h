/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS script descriptor. */

#ifndef vm_JSScript_h
#define vm_JSScript_h

#include "mozilla/ArrayUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/Maybe.h"
#include "mozilla/MaybeOneOf.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Span.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Utf8.h"
#include "mozilla/Variant.h"

#include <type_traits>  // std::is_same
#include <utility>      // std::move

#include "jstypes.h"

#include "frontend/BinASTRuntimeSupport.h"
#include "frontend/NameAnalysisTypes.h"
#include "gc/Barrier.h"
#include "gc/Rooting.h"
#include "jit/IonCode.h"
#include "js/CompileOptions.h"
#include "js/UbiNode.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"
#include "util/StructuredSpewer.h"
#include "vm/BytecodeIterator.h"
#include "vm/BytecodeLocation.h"
#include "vm/BytecodeUtil.h"
#include "vm/JSAtom.h"
#include "vm/NativeObject.h"
#include "vm/Scope.h"
#include "vm/Shape.h"
#include "vm/SharedImmutableStringsCache.h"
#include "vm/Time.h"

namespace JS {
struct ScriptSourceInfo;
template <typename UnitT>
class SourceText;
}  // namespace JS

namespace js {

namespace jit {
struct BaselineScript;
class ICScript;
struct IonScriptCounts;
}  // namespace jit

#define ION_DISABLED_SCRIPT ((js::jit::IonScript*)0x1)
#define ION_COMPILING_SCRIPT ((js::jit::IonScript*)0x2)
#define ION_PENDING_SCRIPT ((js::jit::IonScript*)0x3)

#define BASELINE_DISABLED_SCRIPT ((js::jit::BaselineScript*)0x1)

class AutoKeepTypeScripts;
class AutoSweepTypeScript;
class BreakpointSite;
class Debugger;
class LazyScript;
class ModuleObject;
class RegExpObject;
class SourceCompressionTask;
class Shape;
class TypeScript;

namespace frontend {
struct BytecodeEmitter;
class FunctionBox;
class ModuleSharedContext;
}  // namespace frontend

namespace detail {

// Do not call this directly! It is exposed for the friend declarations in
// this file.
JSScript* CopyScript(JSContext* cx, HandleScript src,
                     HandleScriptSourceObject sourceObject,
                     MutableHandle<GCVector<Scope*>> scopes);

}  // namespace detail

}  // namespace js

/*
 * [SMDOC] Try Notes
 *
 * Trynotes are attached to regions that are involved with
 * exception unwinding. They can be broken up into four categories:
 *
 * 1. CATCH and FINALLY: Basic exception handling. A CATCH trynote
 *    covers the range of the associated try. A FINALLY trynote covers
 *    the try and the catch.

 * 2. FOR_IN and DESTRUCTURING: These operations create an iterator
 *    which must be cleaned up (by calling IteratorClose) during
 *    exception unwinding.
 *
 * 3. FOR_OF and FOR_OF_ITERCLOSE: For-of loops handle unwinding using
 *    catch blocks. These trynotes are used for for-of breaks/returns,
 *    which create regions that are lexically within a for-of block,
 *    but logically outside of it. See TryNoteIter::settle for more
 *    details.
 *
 * 4. LOOP: This represents normal for/while/do-while loops. It is
 *    unnecessary for exception unwinding, but storing the boundaries
 *    of loops here is helpful for heuristics that need to know
 *    whether a given op is inside a loop.
 */
enum JSTryNoteKind {
  JSTRY_CATCH,
  JSTRY_FINALLY,
  JSTRY_FOR_IN,
  JSTRY_DESTRUCTURING,
  JSTRY_FOR_OF,
  JSTRY_FOR_OF_ITERCLOSE,
  JSTRY_LOOP
};

/*
 * Exception handling record.
 */
struct JSTryNote {
  uint32_t kind;       /* one of JSTryNoteKind */
  uint32_t stackDepth; /* stack depth upon exception handler entry */
  uint32_t start;      /* start of the try statement or loop relative
                          to script->code() */
  uint32_t length;     /* length of the try statement or loop */

  template <js::XDRMode mode>
  js::XDRResult XDR(js::XDRState<mode>* xdr);
};

namespace js {

// A block scope has a range in bytecode: it is entered at some offset, and left
// at some later offset.  Scopes can be nested.  Given an offset, the
// ScopeNote containing that offset whose with the highest start value
// indicates the block scope.  The block scope list is sorted by increasing
// start value.
//
// It is possible to leave a scope nonlocally, for example via a "break"
// statement, so there may be short bytecode ranges in a block scope in which we
// are popping the block chain in preparation for a goto.  These exits are also
// nested with respect to outer scopes.  The scopes in these exits are indicated
// by the "index" field, just like any other block.  If a nonlocal exit pops the
// last block scope, the index will be NoScopeIndex.
//
struct ScopeNote {
  // Sentinel index for no Scope.
  static const uint32_t NoScopeIndex = UINT32_MAX;

  // Sentinel index for no ScopeNote.
  static const uint32_t NoScopeNoteIndex = UINT32_MAX;

  uint32_t index;   // Index of Scope in the scopes array, or
                    // NoScopeIndex if there is no block scope in
                    // this range.
  uint32_t start;   // Bytecode offset at which this scope starts
                    // relative to script->code().
  uint32_t length;  // Bytecode length of scope.
  uint32_t parent;  // Index of parent block scope in notes, or NoScopeNote.

  template <js::XDRMode mode>
  js::XDRResult XDR(js::XDRState<mode>* xdr);
};

class ScriptCounts {
 public:
  typedef mozilla::Vector<PCCounts, 0, SystemAllocPolicy> PCCountsVector;

  inline ScriptCounts();
  inline explicit ScriptCounts(PCCountsVector&& jumpTargets);
  inline ScriptCounts(ScriptCounts&& src);
  inline ~ScriptCounts();

  inline ScriptCounts& operator=(ScriptCounts&& src);

  // Return the counter used to count the number of visits. Returns null if
  // the element is not found.
  PCCounts* maybeGetPCCounts(size_t offset);
  const PCCounts* maybeGetPCCounts(size_t offset) const;

  // PCCounts are stored at jump-target offsets. This function looks for the
  // previous PCCount which is in the same basic block as the current offset.
  PCCounts* getImmediatePrecedingPCCounts(size_t offset);

  // Return the counter used to count the number of throws. Returns null if
  // the element is not found.
  const PCCounts* maybeGetThrowCounts(size_t offset) const;

  // Throw counts are stored at the location of each throwing
  // instruction. This function looks for the previous throw count.
  //
  // Note: if the offset of the returned count is higher than the offset of
  // the immediate preceding PCCount, then this throw happened in the same
  // basic block.
  const PCCounts* getImmediatePrecedingThrowCounts(size_t offset) const;

  // Return the counter used to count the number of throws. Allocate it if
  // none exists yet. Returns null if the allocation failed.
  PCCounts* getThrowCounts(size_t offset);

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf);

 private:
  friend class ::JSScript;
  friend struct ScriptAndCounts;

  // This sorted array is used to map an offset to the number of times a
  // branch got visited.
  PCCountsVector pcCounts_;

  // This sorted vector is used to map an offset to the number of times an
  // instruction throw.
  PCCountsVector throwCounts_;

  // Information about any Ion compilations for the script.
  jit::IonScriptCounts* ionCounts_;
};

// Note: The key of this hash map is a weak reference to a JSScript.  We do not
// use the WeakMap implementation provided in gc/WeakMap.h because it would be
// collected at the beginning of the sweeping of the realm, thus before the
// calls to the JSScript::finalize function which are used to aggregate code
// coverage results on the realm.
using UniqueScriptCounts = js::UniquePtr<ScriptCounts>;
using ScriptCountsMap = HashMap<JSScript*, UniqueScriptCounts,
                                DefaultHasher<JSScript*>, SystemAllocPolicy>;

using ScriptNameMap = HashMap<JSScript*, JS::UniqueChars,
                              DefaultHasher<JSScript*>, SystemAllocPolicy>;

#ifdef MOZ_VTUNE
using ScriptVTuneIdMap =
    HashMap<JSScript*, uint32_t, DefaultHasher<JSScript*>, SystemAllocPolicy>;
#endif

class DebugScript {
  friend class ::JSScript;
  friend class JS::Realm;

  /*
   * When greater than zero, compile script in single-step mode, with VM calls
   * to HandleDebugTrap before each bytecode instruction's code. This is a
   * counter, adjusted by the incrementStepModeCount and decrementStepModeCount
   * methods.
   */
  uint32_t stepMode;

  /*
   * Number of breakpoint sites at opcodes in the script. This is the number
   * of populated entries in DebugScript::breakpoints, below.
   */
  uint32_t numSites;

  /*
   * Breakpoints set in our script. For speed and simplicity, this array is
   * parallel to script->code(): the BreakpointSite for the opcode at
   * script->code()[offset] is debugScript->breakpoints[offset]. Naturally,
   * this array's true length is script->length().
   */
  BreakpointSite* breakpoints[1];
};

using UniqueDebugScript = js::UniquePtr<DebugScript, JS::FreePolicy>;
using DebugScriptMap = HashMap<JSScript*, UniqueDebugScript,
                               DefaultHasher<JSScript*>, SystemAllocPolicy>;

class ScriptSource;

struct ScriptSourceChunk {
  ScriptSource* ss = nullptr;
  uint32_t chunk = 0;

  ScriptSourceChunk() = default;

  ScriptSourceChunk(ScriptSource* ss, uint32_t chunk) : ss(ss), chunk(chunk) {
    MOZ_ASSERT(valid());
  }

  bool valid() const { return ss != nullptr; }

  bool operator==(const ScriptSourceChunk& other) const {
    return ss == other.ss && chunk == other.chunk;
  }
};

struct ScriptSourceChunkHasher {
  using Lookup = ScriptSourceChunk;

  static HashNumber hash(const ScriptSourceChunk& ssc) {
    return mozilla::AddToHash(DefaultHasher<ScriptSource*>::hash(ssc.ss),
                              ssc.chunk);
  }
  static bool match(const ScriptSourceChunk& c1, const ScriptSourceChunk& c2) {
    return c1 == c2;
  }
};

template <typename Unit>
using EntryUnits = mozilla::UniquePtr<Unit[], JS::FreePolicy>;

// The uncompressed source cache contains *either* UTF-8 source data *or*
// UTF-16 source data.  ScriptSourceChunk implies a ScriptSource that
// contains either UTF-8 data or UTF-16 data, so the nature of the key to
// Map below indicates how each SourceData ought to be interpreted.
using SourceData = mozilla::UniquePtr<void, JS::FreePolicy>;

template <typename Unit>
inline SourceData ToSourceData(EntryUnits<Unit> chars) {
  static_assert(std::is_same<SourceData::DeleterType,
                             typename EntryUnits<Unit>::DeleterType>::value,
                "EntryUnits and SourceData must share the same deleter "
                "type, that need not know the type of the data being freed, "
                "for the upcast below to be safe");
  return SourceData(chars.release());
}

class UncompressedSourceCache {
  using Map = HashMap<ScriptSourceChunk, SourceData, ScriptSourceChunkHasher,
                      SystemAllocPolicy>;

 public:
  // Hold an entry in the source data cache and prevent it from being purged on
  // GC.
  class AutoHoldEntry {
    UncompressedSourceCache* cache_ = nullptr;
    ScriptSourceChunk sourceChunk_ = {};
    SourceData data_ = nullptr;

   public:
    explicit AutoHoldEntry() = default;

    ~AutoHoldEntry() {
      if (cache_) {
        MOZ_ASSERT(sourceChunk_.valid());
        cache_->releaseEntry(*this);
      }
    }

    template <typename Unit>
    void holdUnits(EntryUnits<Unit> units) {
      MOZ_ASSERT(!cache_);
      MOZ_ASSERT(!sourceChunk_.valid());
      MOZ_ASSERT(!data_);

      data_ = ToSourceData(std::move(units));
    }

   private:
    void holdEntry(UncompressedSourceCache* cache,
                   const ScriptSourceChunk& sourceChunk) {
      // Initialise the holder for a specific cache and script source.
      // This will hold on to the cached source chars in the event that
      // the cache is purged.
      MOZ_ASSERT(!cache_);
      MOZ_ASSERT(!sourceChunk_.valid());
      MOZ_ASSERT(!data_);

      cache_ = cache;
      sourceChunk_ = sourceChunk;
    }

    void deferDelete(SourceData data) {
      // Take ownership of source chars now the cache is being purged. Remove
      // our reference to the ScriptSource which might soon be destroyed.
      MOZ_ASSERT(cache_);
      MOZ_ASSERT(sourceChunk_.valid());
      MOZ_ASSERT(!data_);

      cache_ = nullptr;
      sourceChunk_ = ScriptSourceChunk();

      data_ = std::move(data);
    }

    const ScriptSourceChunk& sourceChunk() const { return sourceChunk_; }
    friend class UncompressedSourceCache;
  };

 private:
  UniquePtr<Map> map_ = nullptr;
  AutoHoldEntry* holder_ = nullptr;

 public:
  UncompressedSourceCache() = default;

  template <typename Unit>
  const Unit* lookup(const ScriptSourceChunk& ssc, AutoHoldEntry& asp);

  bool put(const ScriptSourceChunk& ssc, SourceData data, AutoHoldEntry& asp);

  void purge();

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf);

 private:
  void holdEntry(AutoHoldEntry& holder, const ScriptSourceChunk& ssc);
  void releaseEntry(AutoHoldEntry& holder);
};

template <typename Unit>
struct SourceTypeTraits;

template <>
struct SourceTypeTraits<mozilla::Utf8Unit> {
  using CharT = char;
  using SharedImmutableString = js::SharedImmutableString;

  static const mozilla::Utf8Unit* units(const SharedImmutableString& string) {
    // Casting |char| data to |Utf8Unit| is safe because |Utf8Unit|
    // contains a |char|.  See the long comment in |Utf8Unit|'s definition.
    return reinterpret_cast<const mozilla::Utf8Unit*>(string.chars());
  }

  static char* toString(const mozilla::Utf8Unit* units) {
    auto asUnsigned =
        const_cast<unsigned char*>(mozilla::Utf8AsUnsignedChars(units));
    return reinterpret_cast<char*>(asUnsigned);
  }

  static UniqueChars toCacheable(EntryUnits<mozilla::Utf8Unit> str) {
    // The cache only stores strings of |char| or |char16_t|, and right now
    // it seems best not to gunk up the cache with |Utf8Unit| too.  So
    // cache |Utf8Unit| strings by interpreting them as |char| strings.
    char* chars = toString(str.release());
    return UniqueChars(chars);
  }
};

template <>
struct SourceTypeTraits<char16_t> {
  using CharT = char16_t;
  using SharedImmutableString = js::SharedImmutableTwoByteString;

  static const char16_t* units(const SharedImmutableString& string) {
    return string.chars();
  }

  static char16_t* toString(const char16_t* units) {
    return const_cast<char16_t*>(units);
  }

  static UniqueTwoByteChars toCacheable(EntryUnits<char16_t> str) {
    return UniqueTwoByteChars(std::move(str));
  }
};

class ScriptSourceHolder;

class ScriptSource {
  friend class SourceCompressionTask;

  class PinnedUnitsBase {
   protected:
    PinnedUnitsBase** stack_ = nullptr;
    PinnedUnitsBase* prev_ = nullptr;

    ScriptSource* source_;

    explicit PinnedUnitsBase(ScriptSource* source) : source_(source) {}
  };

 public:
  // Any users that wish to manipulate the char buffer of the ScriptSource
  // needs to do so via PinnedUnits for GC safety. A GC may compress
  // ScriptSources. If the source were initially uncompressed, then any raw
  // pointers to the char buffer would now point to the freed, uncompressed
  // chars. This is analogous to Rooted.
  template <typename Unit>
  class PinnedUnits : public PinnedUnitsBase {
    const Unit* units_;

   public:
    PinnedUnits(JSContext* cx, ScriptSource* source,
                UncompressedSourceCache::AutoHoldEntry& holder, size_t begin,
                size_t len);

    ~PinnedUnits();

    const Unit* get() const { return units_; }

    const typename SourceTypeTraits<Unit>::CharT* asChars() const {
      return SourceTypeTraits<Unit>::toString(get());
    }
  };

 private:
  mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire,
                  mozilla::recordreplay::Behavior::DontPreserve>
      refs;

  // Note: while ScriptSources may be compressed off thread, they are only
  // modified by the main thread, and all members are always safe to access
  // on the main thread.

  // Indicate which field in the |data| union is active.

  // Uncompressed source text.
  template <typename Unit>
  class Uncompressed {
    typename SourceTypeTraits<Unit>::SharedImmutableString string_;

   public:
    explicit Uncompressed(
        typename SourceTypeTraits<Unit>::SharedImmutableString str)
        : string_(std::move(str)) {}

    const Unit* units() const { return SourceTypeTraits<Unit>::units(string_); }

    size_t length() const { return string_.length(); }
  };

  // Compressed source text.
  template <typename Unit>
  struct Compressed {
    // Single-byte compressed text, regardless whether the original text
    // was single-byte or two-byte.
    SharedImmutableString raw;
    size_t uncompressedLength;

    Compressed(SharedImmutableString raw, size_t uncompressedLength)
        : raw(std::move(raw)), uncompressedLength(uncompressedLength) {}
  };

  // Source that can be retrieved using the registered source hook.  |Unit|
  // records the source type so that source-text coordinates in functions and
  // scripts that depend on this |ScriptSource| are correct.
  template <typename Unit>
  struct Retrievable {
    // The source hook and script URL required to retrieve source are stored
    // elsewhere, so nothing is needed here.  It'd be better hygiene to store
    // something source-hook-like in each |ScriptSource| that needs it, but that
    // requires reimagining a source-hook API that currently depends on source
    // hooks being uniquely-owned pointers...
  };

  // Missing source text that isn't retrievable using the source hook.  (All
  // ScriptSources initially begin in this state.  Users that are compiling
  // source text will overwrite |data| to store a different state.)
  struct Missing {};

  // BinAST source.
  struct BinAST {
    SharedImmutableString string;
    explicit BinAST(SharedImmutableString&& str) : string(std::move(str)) {}
  };

  using SourceType =
      mozilla::Variant<Compressed<mozilla::Utf8Unit>,
                       Uncompressed<mozilla::Utf8Unit>, Compressed<char16_t>,
                       Uncompressed<char16_t>, Retrievable<mozilla::Utf8Unit>,
                       Retrievable<char16_t>, Missing, BinAST>;
  SourceType data;

  // If the GC attempts to call convertToCompressedSource with PinnedUnits
  // present, the first PinnedUnits (that is, bottom of the stack) will set
  // the compressed chars upon destruction.
  PinnedUnitsBase* pinnedUnitsStack_;
  mozilla::MaybeOneOf<Compressed<mozilla::Utf8Unit>, Compressed<char16_t>>
      pendingCompressed_;

  // The filename of this script.
  UniqueChars filename_;

  UniqueTwoByteChars displayURL_;
  UniqueTwoByteChars sourceMapURL_;
  bool mutedErrors_;

  // bytecode offset in caller script that generated this code.
  // This is present for eval-ed code, as well as "new Function(...)"-introduced
  // scripts.
  uint32_t introductionOffset_;

  // If this source is for Function constructor, the position of ")" after
  // parameter list in the source.  This is used to get function body.
  // 0 for other cases.
  uint32_t parameterListEnd_;

  // If this ScriptSource was generated by a code-introduction mechanism such
  // as |eval| or |new Function|, the debugger needs access to the "raw"
  // filename of the top-level script that contains the eval-ing code.  To
  // keep track of this, we must preserve the original outermost filename (of
  // the original introducer script), so that instead of a filename of
  // "foo.js line 30 > eval line 10 > Function", we can obtain the original
  // raw filename of "foo.js".
  //
  // In the case described above, this field will be non-null and will be the
  // original raw filename from above.  Otherwise this field will be null.
  UniqueChars introducerFilename_;

  // A string indicating how this source code was introduced into the system.
  // This accessor returns one of the following values:
  //      "eval" for code passed to |eval|.
  //      "Function" for code passed to the |Function| constructor.
  //      "Worker" for code loaded by calling the Web worker
  //      constructor&mdash;the worker's main script. "importScripts" for code
  //      by calling |importScripts| in a web worker. "handler" for code
  //      assigned to DOM elements' event handler IDL attributes.
  //      "scriptElement" for code belonging to <script> elements.
  //      undefined if the implementation doesn't know how the code was
  //      introduced.
  // This is a constant, statically allocated C string, so does not need
  // memory management.
  const char* introductionType_;

  // The bytecode cache encoder is used to encode only the content of function
  // which are delazified.  If this value is not nullptr, then each delazified
  // function should be recorded before their first execution.
  UniquePtr<XDRIncrementalEncoder> xdrEncoder_;

  // Instant at which the first parse of this source ended, or null
  // if the source hasn't been parsed yet.
  //
  // Used for statistics purposes, to determine how much time code spends
  // syntax parsed before being full parsed, to help determine whether
  // our syntax parse vs. full parse heuristics are correct.
  mozilla::TimeStamp parseEnded_;

  // An id for this source that is unique across the process. This can be used
  // to refer to this source from places that don't want to hold a strong
  // reference on the source itself.
  //
  // This is a 32 bit ID and could overflow, in which case the ID will not be
  // unique anymore.
  uint32_t id_;

  // How many ids have been handed out to sources.
  static mozilla::Atomic<uint32_t, mozilla::SequentiallyConsistent,
                         mozilla::recordreplay::Behavior::DontPreserve>
      idCount_;

  // If this field is true, we can call JSRuntime::sourceHook to load the source
  // on demand.  Thus if this contains compressed/uncompressed data, we don't
  // have to preserve it while we're not using it, because we can use the source
  // hook to load it when we need it.
  //
  // This field is always true for retrievable source.  It *may* be true for
  // compressed/uncompressed source (if retrievable source was rewritten to
  // compressed/uncompressed source loaded using the source hook).  It is always
  // false for missing or BinAST source.
  bool sourceRetrievable_ : 1;

  bool hasIntroductionOffset_ : 1;
  bool containsAsmJS_ : 1;

  UniquePtr<frontend::BinASTSourceMetadata> binASTMetadata_;

  template <typename Unit>
  const Unit* chunkUnits(JSContext* cx,
                         UncompressedSourceCache::AutoHoldEntry& holder,
                         size_t chunk);

  // Return a string containing the chars starting at |begin| and ending at
  // |begin + len|.
  //
  // Warning: this is *not* GC-safe! Any chars to be handed out should use
  // PinnedUnits. See comment below.
  template <typename Unit>
  const Unit* units(JSContext* cx, UncompressedSourceCache::AutoHoldEntry& asp,
                    size_t begin, size_t len);

  template <typename Unit>
  void movePendingCompressedSource();

 public:
  // When creating a JSString* from TwoByte source characters, we don't try to
  // to deflate to Latin1 for longer strings, because this can be slow.
  static const size_t SourceDeflateLimit = 100;

  explicit ScriptSource()
      : refs(0),
        data(SourceType(Missing())),
        pinnedUnitsStack_(nullptr),
        filename_(nullptr),
        displayURL_(nullptr),
        sourceMapURL_(nullptr),
        mutedErrors_(false),
        introductionOffset_(0),
        parameterListEnd_(0),
        introducerFilename_(nullptr),
        introductionType_(nullptr),
        xdrEncoder_(nullptr),
        id_(++idCount_),
        sourceRetrievable_(false),
        hasIntroductionOffset_(false),
        containsAsmJS_(false) {}

  ~ScriptSource() { MOZ_ASSERT(refs == 0); }

  void incref() { refs++; }
  void decref() {
    MOZ_ASSERT(refs != 0);
    if (--refs == 0) {
      js_delete(this);
    }
  }
  MOZ_MUST_USE bool initFromOptions(
      JSContext* cx, const JS::ReadOnlyCompileOptions& options,
      const mozilla::Maybe<uint32_t>& parameterListEnd = mozilla::Nothing());

  /**
   * The minimum script length (in code units) necessary for a script to be
   * eligible to be compressed.
   */
  static constexpr size_t MinimumCompressibleLength = 256;

 private:
  class LoadSourceMatcher;

 public:
  // Attempt to load usable source for |ss| -- source text on which substring
  // operations and the like can be performed.  On success return true and set
  // |*loaded| to indicate whether usable source could be loaded; otherwise
  // return false.
  static bool loadSource(JSContext* cx, ScriptSource* ss, bool* loaded);

  // Assign source data from |srcBuf| to this recently-created |ScriptSource|.
  template <typename Unit>
  MOZ_MUST_USE bool assignSource(JSContext* cx,
                                 const JS::ReadOnlyCompileOptions& options,
                                 JS::SourceText<Unit>& srcBuf);

  bool sourceRetrievable() const { return sourceRetrievable_; }
  bool hasSourceText() const {
    return hasUncompressedSource() || hasCompressedSource();
  }
  bool hasBinASTSource() const { return data.is<BinAST>(); }

  void setBinASTSourceMetadata(frontend::BinASTSourceMetadata* metadata) {
    MOZ_ASSERT(hasBinASTSource());
    binASTMetadata_.reset(metadata);
  }
  frontend::BinASTSourceMetadata* binASTSourceMetadata() const {
    MOZ_ASSERT(hasBinASTSource());
    return binASTMetadata_.get();
  }

 private:
  struct UncompressedDataMatcher {
    template <typename Unit>
    const void* operator()(const Uncompressed<Unit>& u) {
      return u.units();
    }

    template <typename T>
    const void* operator()(const T&) {
      MOZ_CRASH(
          "attempting to access uncompressed data in a "
          "ScriptSource not containing it");
      return nullptr;
    }
  };

 public:
  template <typename Unit>
  const Unit* uncompressedData() {
    return static_cast<const Unit*>(data.match(UncompressedDataMatcher()));
  }

 private:
  struct CompressedDataMatcher {
    template <typename Unit>
    char* operator()(const Compressed<Unit>& c) {
      return const_cast<char*>(c.raw.chars());
    }

    template <typename T>
    char* operator()(const T&) {
      MOZ_CRASH(
          "attempting to access compressed data in a ScriptSource "
          "not containing it");
      return nullptr;
    }
  };

 public:
  template <typename Unit>
  char* compressedData() {
    return data.match(CompressedDataMatcher());
  }

 private:
  struct BinASTDataMatcher {
    void* operator()(const BinAST& b) {
      return const_cast<char*>(b.string.chars());
    }

    void notBinAST() { MOZ_CRASH("ScriptSource isn't backed by BinAST data"); }

    template <typename T>
    void* operator()(const T&) {
      notBinAST();
      return nullptr;
    }
  };

 public:
  void* binASTData() { return data.match(BinASTDataMatcher()); }

 private:
  struct HasUncompressedSource {
    template <typename Unit>
    bool operator()(const Uncompressed<Unit>&) {
      return true;
    }

    template <typename Unit>
    bool operator()(const Compressed<Unit>&) {
      return false;
    }

    template <typename Unit>
    bool operator()(const Retrievable<Unit>&) {
      return false;
    }

    bool operator()(const BinAST&) { return false; }

    bool operator()(const Missing&) { return false; }
  };

 public:
  bool hasUncompressedSource() const {
    return data.match(HasUncompressedSource());
  }

  template <typename Unit>
  bool uncompressedSourceIs() const {
    MOZ_ASSERT(hasUncompressedSource());
    return data.is<Uncompressed<Unit>>();
  }

 private:
  struct HasCompressedSource {
    template <typename Unit>
    bool operator()(const Compressed<Unit>&) {
      return true;
    }

    template <typename Unit>
    bool operator()(const Uncompressed<Unit>&) {
      return false;
    }

    template <typename Unit>
    bool operator()(const Retrievable<Unit>&) {
      return false;
    }

    bool operator()(const BinAST&) { return false; }

    bool operator()(const Missing&) { return false; }
  };

 public:
  bool hasCompressedSource() const { return data.match(HasCompressedSource()); }

  template <typename Unit>
  bool compressedSourceIs() const {
    MOZ_ASSERT(hasCompressedSource());
    return data.is<Compressed<Unit>>();
  }

 private:
  template <typename Unit>
  struct SourceTypeMatcher {
    template <template <typename C> class Data>
    bool operator()(const Data<Unit>&) {
      return true;
    }

    template <template <typename C> class Data, typename NotUnit>
    bool operator()(const Data<NotUnit>&) {
      return false;
    }

    bool operator()(const BinAST&) {
      MOZ_CRASH("doesn't make sense to ask source type of BinAST data");
      return false;
    }

    bool operator()(const Missing&) {
      MOZ_CRASH("doesn't make sense to ask source type when missing");
      return false;
    }
  };

 public:
  template <typename Unit>
  bool hasSourceType() const {
    return data.match(SourceTypeMatcher<Unit>());
  }

 private:
  struct UncompressedLengthMatcher {
    template <typename Unit>
    size_t operator()(const Uncompressed<Unit>& u) {
      return u.length();
    }

    template <typename Unit>
    size_t operator()(const Compressed<Unit>& u) {
      return u.uncompressedLength;
    }

    template <typename Unit>
    size_t operator()(const Retrievable<Unit>&) {
      MOZ_CRASH("ScriptSource::length on a missing-but-retrievable source");
      return 0;
    }

    size_t operator()(const BinAST& b) { return b.string.length(); }

    size_t operator()(const Missing& m) {
      MOZ_CRASH("ScriptSource::length on a missing source");
      return 0;
    }
  };

 public:
  size_t length() const {
    MOZ_ASSERT(hasSourceText() || hasBinASTSource());
    return data.match(UncompressedLengthMatcher());
  }

  JSFlatString* substring(JSContext* cx, size_t start, size_t stop);
  JSFlatString* substringDontDeflate(JSContext* cx, size_t start, size_t stop);

  MOZ_MUST_USE bool appendSubstring(JSContext* cx, js::StringBuffer& buf,
                                    size_t start, size_t stop);

  bool isFunctionBody() { return parameterListEnd_ != 0; }
  JSFlatString* functionBodyString(JSContext* cx);

  void addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                              JS::ScriptSourceInfo* info) const;

 private:
  // Overwrites |data| with the uncompressed data from |source|.
  //
  // This function asserts nothing about |data|.  Users should use assertions to
  // double-check their own understandings of the |data| state transition being
  // performed.
  template <typename Unit>
  MOZ_MUST_USE bool setUncompressedSourceHelper(JSContext* cx,
                                                EntryUnits<Unit>&& source,
                                                size_t length);

 public:
  // Initialize a fresh |ScriptSource| with uncompressed source.
  template <typename Unit>
  MOZ_MUST_USE bool initializeUncompressedSource(JSContext* cx,
                                                 EntryUnits<Unit>&& source,
                                                 size_t length);

  // Set the retrieved source for a |ScriptSource| whose source was recorded as
  // missing but retrievable.
  template <typename Unit>
  MOZ_MUST_USE bool setRetrievedSource(JSContext* cx, EntryUnits<Unit>&& source,
                                       size_t length);

  MOZ_MUST_USE bool tryCompressOffThread(JSContext* cx);

  // Convert this ScriptSource from storing uncompressed source of the given
  // type, to storing compressed source.  (Raw compressed source is always
  // single-byte; |Unit| just records the encoding of the uncompressed source.)
  template <typename Unit>
  void convertToCompressedSource(SharedImmutableString compressed,
                                 size_t sourceLength);

  // Initialize a fresh ScriptSource as containing compressed source of the
  // indicated original encoding.
  template <typename Unit>
  MOZ_MUST_USE bool initializeWithCompressedSource(JSContext* cx,
                                                   UniqueChars&& raw,
                                                   size_t rawLength,
                                                   size_t sourceLength);

#if defined(JS_BUILD_BINAST)

  /*
   * Do not take ownership of the given `buf`. Store the canonical, shared
   * and de-duplicated version. If there is no extant shared version of
   * `buf`, make a copy.
   */
  MOZ_MUST_USE bool setBinASTSourceCopy(JSContext* cx, const uint8_t* buf,
                                        size_t len);

  const uint8_t* binASTSource();

#endif /* JS_BUILD_BINAST */

 private:
  void performTaskWork(SourceCompressionTask* task);

  struct ConvertToCompressedSourceFromTask {
    ScriptSource* const source_;
    SharedImmutableString& compressed_;

    ConvertToCompressedSourceFromTask(ScriptSource* source,
                                      SharedImmutableString& compressed)
        : source_(source), compressed_(compressed) {}

    template <typename Unit>
    void operator()(const Uncompressed<Unit>&) {
      source_->convertToCompressedSource<Unit>(std::move(compressed_),
                                               source_->length());
    }

    template <typename Unit>
    void operator()(const Compressed<Unit>&) {
      MOZ_CRASH(
          "can't set compressed source when source is already compressed -- "
          "ScriptSource::tryCompressOffThread shouldn't have queued up this "
          "task?");
    }

    template <typename Unit>
    void operator()(const Retrievable<Unit>&) {
      MOZ_CRASH("shouldn't compressing unloaded-but-retrievable source");
    }

    void operator()(const BinAST&) {
      MOZ_CRASH("doesn't make sense to set compressed source for BinAST data");
    }

    void operator()(const Missing&) {
      MOZ_CRASH(
          "doesn't make sense to set compressed source for missing source -- "
          "ScriptSource::tryCompressOffThread shouldn't have queued up this "
          "task?");
    }
  };

  void convertToCompressedSourceFromTask(SharedImmutableString compressed);

 private:
  // It'd be better to make this function take <XDRMode, Unit>, as both
  // specializations of this function contain nested Unit-parametrized
  // helper classes that do everything the function needs to do.  But then
  // we'd need template function partial specialization to hold XDRMode
  // constant while varying Unit, so that idea's no dice.
  template <XDRMode mode>
  MOZ_MUST_USE XDRResult xdrUncompressedSource(XDRState<mode>* xdr,
                                               uint8_t sourceCharSize,
                                               uint32_t uncompressedLength);

 public:
  MOZ_MUST_USE bool setFilename(JSContext* cx, const char* filename);
  const char* introducerFilename() const {
    return introducerFilename_ ? introducerFilename_.get() : filename_.get();
  }
  bool hasIntroductionType() const { return introductionType_; }
  const char* introductionType() const {
    MOZ_ASSERT(hasIntroductionType());
    return introductionType_;
  }
  const char* filename() const { return filename_.get(); }

  uint32_t id() const { return id_; }

  // Display URLs
  MOZ_MUST_USE bool setDisplayURL(JSContext* cx, const char16_t* displayURL);
  bool hasDisplayURL() const { return displayURL_ != nullptr; }
  const char16_t* displayURL() {
    MOZ_ASSERT(hasDisplayURL());
    return displayURL_.get();
  }

  // Source maps
  MOZ_MUST_USE bool setSourceMapURL(JSContext* cx,
                                    const char16_t* sourceMapURL);
  bool hasSourceMapURL() const { return sourceMapURL_ != nullptr; }
  const char16_t* sourceMapURL() {
    MOZ_ASSERT(hasSourceMapURL());
    return sourceMapURL_.get();
  }

  bool mutedErrors() const { return mutedErrors_; }

  bool hasIntroductionOffset() const { return hasIntroductionOffset_; }
  uint32_t introductionOffset() const {
    MOZ_ASSERT(hasIntroductionOffset());
    return introductionOffset_;
  }
  void setIntroductionOffset(uint32_t offset) {
    MOZ_ASSERT(!hasIntroductionOffset());
    MOZ_ASSERT(offset <= (uint32_t)INT32_MAX);
    introductionOffset_ = offset;
    hasIntroductionOffset_ = true;
  }

  bool containsAsmJS() const { return containsAsmJS_; }
  void setContainsAsmJS() { containsAsmJS_ = true; }

  // Return wether an XDR encoder is present or not.
  bool hasEncoder() const { return bool(xdrEncoder_); }

  // Create a new XDR encoder, and encode the top-level JSScript. The result
  // of the encoding would be available in the |buffer| provided as argument,
  // as soon as |xdrFinalize| is called and all xdr function calls returned
  // successfully.
  bool xdrEncodeTopLevel(JSContext* cx, HandleScript script);

  // Encode a delazified JSFunction.  In case of errors, the XDR encoder is
  // freed and the |buffer| provided as argument to |xdrEncodeTopLevel| is
  // considered undefined.
  //
  // The |sourceObject| argument is the object holding the current
  // ScriptSource.
  bool xdrEncodeFunction(JSContext* cx, HandleFunction fun,
                         HandleScriptSourceObject sourceObject);

  // Linearize the encoded content in the |buffer| provided as argument to
  // |xdrEncodeTopLevel|, and free the XDR encoder.  In case of errors, the
  // |buffer| is considered undefined.
  bool xdrFinalizeEncoder(JS::TranscodeBuffer& buffer);

  const mozilla::TimeStamp parseEnded() const { return parseEnded_; }
  // Inform `this` source that it has been fully parsed.
  void recordParseEnded() {
    MOZ_ASSERT(parseEnded_.IsNull());
    parseEnded_ = ReallyNow();
  }

 private:
  template <typename Unit, XDRMode mode>
  static MOZ_MUST_USE XDRResult codeUncompressedData(XDRState<mode>* const xdr,
                                                     ScriptSource* const ss,
                                                     bool retrievable);

  template <typename Unit, XDRMode mode>
  static MOZ_MUST_USE XDRResult codeCompressedData(XDRState<mode>* const xdr,
                                                   ScriptSource* const ss,
                                                   bool retrievable);

  template <XDRMode mode>
  static MOZ_MUST_USE XDRResult codeBinASTData(XDRState<mode>* const xdr,
                                               ScriptSource* const ss);

  template <typename Unit, XDRMode mode>
  static void codeRetrievableData(ScriptSource* ss);

  template <XDRMode mode>
  static MOZ_MUST_USE XDRResult xdrData(XDRState<mode>* const xdr,
                                        ScriptSource* const ss);

 public:
  template <XDRMode mode>
  static MOZ_MUST_USE XDRResult
  XDR(XDRState<mode>* xdr, const mozilla::Maybe<JS::CompileOptions>& options,
      MutableHandle<ScriptSourceHolder> ss);

  void trace(JSTracer* trc);
};

class ScriptSourceHolder {
  ScriptSource* ss;

 public:
  ScriptSourceHolder() : ss(nullptr) {}
  explicit ScriptSourceHolder(ScriptSource* ss) : ss(ss) { ss->incref(); }
  ~ScriptSourceHolder() {
    if (ss) {
      ss->decref();
    }
  }
  void reset(ScriptSource* newss) {
    // incref before decref just in case ss == newss.
    if (newss) {
      newss->incref();
    }
    if (ss) {
      ss->decref();
    }
    ss = newss;
  }
  ScriptSource* get() const { return ss; }

  void trace(JSTracer* trc) { ss->trace(trc); }
};

// [SMDOC] ScriptSourceObject
//
// ScriptSourceObject stores the ScriptSource and GC pointers related to it.
//
// ScriptSourceObjects can be cloned when we clone the JSScript (in order to
// execute the script in a different compartment). In this case we create a new
// SSO that stores (a wrapper for) the original SSO in its "canonical slot".
// The canonical SSO is always used for the private, introductionScript,
// element, elementAttributeName slots. This means their accessors may return an
// object in a different compartment, hence the "unwrapped" prefix.
//
// Note that we don't clone the SSO when cloning the script for a different
// realm in the same compartment, so sso->realm() does not necessarily match the
// script's realm.
//
// We need ScriptSourceObject (instead of storing these GC pointers in the
// ScriptSource itself) to properly account for cross-zone pointers: the
// canonical SSO will be stored in the wrapper map if necessary so GC will do
// the right thing.
class ScriptSourceObject : public NativeObject {
  static const ClassOps classOps_;

  static ScriptSourceObject* createInternal(JSContext* cx, ScriptSource* source,
                                            HandleObject canonical);

  bool isCanonical() const {
    return &getReservedSlot(CANONICAL_SLOT).toObject() == this;
  }
  ScriptSourceObject* unwrappedCanonical() const;

 public:
  static const Class class_;

  static void trace(JSTracer* trc, JSObject* obj);
  static void finalize(FreeOp* fop, JSObject* obj);

  static ScriptSourceObject* create(JSContext* cx, ScriptSource* source);
  static ScriptSourceObject* clone(JSContext* cx, HandleScriptSourceObject sso);

  // Initialize those properties of this ScriptSourceObject whose values
  // are provided by |options|, re-wrapping as necessary.
  static bool initFromOptions(JSContext* cx, HandleScriptSourceObject source,
                              const JS::ReadOnlyCompileOptions& options);

  static bool initElementProperties(JSContext* cx,
                                    HandleScriptSourceObject source,
                                    HandleObject element,
                                    HandleString elementAttrName);

  bool hasSource() const { return !getReservedSlot(SOURCE_SLOT).isUndefined(); }
  ScriptSource* source() const {
    return static_cast<ScriptSource*>(getReservedSlot(SOURCE_SLOT).toPrivate());
  }

  JSObject* unwrappedElement() const {
    return unwrappedCanonical()->getReservedSlot(ELEMENT_SLOT).toObjectOrNull();
  }
  const Value& unwrappedElementAttributeName() const {
    const Value& v =
        unwrappedCanonical()->getReservedSlot(ELEMENT_PROPERTY_SLOT);
    MOZ_ASSERT(!v.isMagic());
    return v;
  }
  JSScript* unwrappedIntroductionScript() const {
    Value value =
        unwrappedCanonical()->getReservedSlot(INTRODUCTION_SCRIPT_SLOT);
    if (value.isUndefined()) {
      return nullptr;
    }
    return value.toGCThing()->as<JSScript>();
  }

  void setPrivate(JSRuntime* rt, const Value& value);

  Value canonicalPrivate() const {
    Value value = getReservedSlot(PRIVATE_SLOT);
    MOZ_ASSERT_IF(!isCanonical(), value.isUndefined());
    return value;
  }

 private:
  enum {
    SOURCE_SLOT = 0,
    CANONICAL_SLOT,
    ELEMENT_SLOT,
    ELEMENT_PROPERTY_SLOT,
    INTRODUCTION_SCRIPT_SLOT,
    PRIVATE_SLOT,
    RESERVED_SLOTS
  };
};

enum class GeneratorKind : bool { NotGenerator, Generator };
enum class FunctionAsyncKind : bool { SyncFunction, AsyncFunction };

struct FieldInitializers {
#ifdef DEBUG
  bool valid;
#endif
  // This struct will eventually have a vector of constant values for optimizing
  // field initializers.
  size_t numFieldInitializers;

  explicit FieldInitializers(size_t numFieldInitializers)
      :
#ifdef DEBUG
        valid(true),
#endif
        numFieldInitializers(numFieldInitializers) {
  }

  static FieldInitializers Invalid() { return FieldInitializers(); }

 private:
  FieldInitializers()
      :
#ifdef DEBUG
        valid(false),
#endif
        numFieldInitializers(0) {
  }
};

/*
 * NB: after a successful XDR_DECODE, XDRScript callers must do any required
 * subsequent set-up of owning function or script object and then call
 * CallNewScriptHook.
 */
template <XDRMode mode>
XDRResult XDRScript(XDRState<mode>* xdr, HandleScope enclosingScope,
                    HandleScriptSourceObject sourceObject, HandleFunction fun,
                    MutableHandleScript scriptp);

template <XDRMode mode>
XDRResult XDRLazyScript(XDRState<mode>* xdr, HandleScope enclosingScope,
                        HandleScriptSourceObject sourceObject,
                        HandleFunction fun, MutableHandle<LazyScript*> lazy);

/*
 * Code any constant value.
 */
template <XDRMode mode>
XDRResult XDRScriptConst(XDRState<mode>* xdr, MutableHandleValue vp);

// [SMDOC] - JSScript data layout (unshared)
//
// PrivateScriptData stores variable-length data associated with a script.
// Abstractly a PrivateScriptData consists of all these arrays:
//
//   * A non-empty array of GCPtrScope in scopes()
//   * A possibly-empty array of GCPtrValue in consts()
//   * A possibly-empty array of JSObject* in objects()
//   * A possibly-empty array of JSTryNote in tryNotes()
//   * A possibly-empty array of ScopeNote in scopeNotes()
//   * A possibly-empty array of uint32_t in resumeOffsets()
//
// Accessing any of these arrays just requires calling the appropriate public
// Span-computing function.
//
// Under the hood, PrivateScriptData is a small class followed by a memory
// layout that compactly encodes all these arrays, in this manner (only
// explicit padding, "--" separators for readability only):
//
//   <PrivateScriptData itself>
//   --
//   (OPTIONAL) PackedSpan for consts()
//   (OPTIONAL) PackedSpan for objects()
//   (OPTIONAL) PackedSpan for tryNotes()
//   (OPTIONAL) PackedSpan for scopeNotes()
//   (OPTIONAL) PackedSpan for resumeOffsets()
//   --
//   (REQUIRED) All the GCPtrScopes that constitute scopes()
//   --
//   (OPTIONAL) If there are consts, padding needed for space so far to be
//              GCPtrValue-aligned
//   (OPTIONAL) All the GCPtrValues that constitute consts()
//   --
//   (OPTIONAL) All the GCPtrObjects that constitute objects()
//   --
//   (OPTIONAL) All the JSTryNotes that constitute tryNotes()
//   --
//   (OPTIONAL) All the ScopeNotes that constitute scopeNotes()
//   --
//   (OPTIONAL) All the uint32_t's that constitute resumeOffsets()
//
// The contents of PrivateScriptData indicate which optional items are present.
// PrivateScriptData::packedOffsets contains bit-fields, one per array.
// Multiply each packed offset by sizeof(uint32_t) to compute a *real* offset.
//
// PrivateScriptData::scopesOffset indicates where scopes() begins. The bound
// of five PackedSpans ensures we can encode this offset compactly.
// PrivateScriptData::nscopes indicates the number of GCPtrScopes in scopes().
//
// The other PackedScriptData::*Offset fields indicate where a potential
// corresponding PackedSpan resides. If the packed offset is 0, there is no
// PackedSpan, and the array is empty. Otherwise the PackedSpan's uint32_t
// offset and length fields store: 1) a *non-packed* offset (a literal count of
// bytes offset from the *start* of PrivateScriptData struct) to the
// corresponding array, and 2) the number of elements in the array,
// respectively.
//
// PrivateScriptData and PackedSpan are 64-bit-aligned, so manual alignment in
// trailing fields is only necessary before the first trailing fields with
// increased alignment -- before GCPtrValues for consts(), on 32-bit, where the
// preceding GCPtrScopes as pointers are only 32-bit-aligned.
class alignas(JS::Value) PrivateScriptData final {
  struct PackedOffsets {
    static constexpr size_t SCALE = sizeof(uint32_t);
    static constexpr size_t MAX_OFFSET = 0b1111;

    // (Scaled) offset to Scopes
    uint32_t scopesOffset : 8;

    // (Scaled) offset to Spans. These are set to 0 if they don't exist.
    uint32_t constsSpanOffset : 4;
    uint32_t objectsSpanOffset : 4;
    uint32_t tryNotesSpanOffset : 4;
    uint32_t scopeNotesSpanOffset : 4;
    uint32_t resumeOffsetsSpanOffset : 4;
  };

  // Detect accidental size regressions.
  static_assert(sizeof(PackedOffsets) == sizeof(uint32_t),
                "unexpected bit-field packing");

  // A span describes base offset and length of one variable length array in
  // the private data.
  struct alignas(uintptr_t) PackedSpan {
    uint32_t offset;
    uint32_t length;
  };

  // Concrete Fields
  PackedOffsets packedOffsets = {};  // zeroes
  uint32_t nscopes = 0;

  js::FieldInitializers fieldInitializers_ = js::FieldInitializers::Invalid();

  // Translate an offset into a concrete pointer.
  template <typename T>
  T* offsetToPointer(size_t offset) {
    uintptr_t base = reinterpret_cast<uintptr_t>(this);
    uintptr_t elem = base + offset;
    return reinterpret_cast<T*>(elem);
  }

  // Translate a PackedOffsets member into a pointer.
  template <typename T>
  T* packedOffsetToPointer(size_t packedOffset) {
    return offsetToPointer<T>(packedOffset * PackedOffsets::SCALE);
  }

  // Translates a PackedOffsets member into a PackedSpan* and then unpacks
  // that to a mozilla::Span.
  template <typename T>
  mozilla::Span<T> packedOffsetToSpan(size_t scaledSpanOffset) {
    PackedSpan* span = packedOffsetToPointer<PackedSpan>(scaledSpanOffset);
    T* base = offsetToPointer<T>(span->offset);
    return mozilla::MakeSpan(base, span->length);
  }

  // Helpers for creating initializing trailing data
  template <typename T>
  void initSpan(size_t* cursor, uint32_t scaledSpanOffset, size_t length);

  template <typename T>
  void initElements(size_t offset, size_t length);

  // Size to allocate
  static size_t AllocationSize(uint32_t nscopes, uint32_t nconsts,
                               uint32_t nobjects, uint32_t ntrynotes,
                               uint32_t nscopenotes, uint32_t nresumeoffsets);

  // Initialize header and PackedSpans
  PrivateScriptData(uint32_t nscopes_, uint32_t nconsts, uint32_t nobjects,
                    uint32_t ntrynotes, uint32_t nscopenotes,
                    uint32_t nresumeoffsets);

 public:
  // Accessors for typed array spans.
  mozilla::Span<GCPtrScope> scopes() {
    GCPtrScope* base =
        packedOffsetToPointer<GCPtrScope>(packedOffsets.scopesOffset);
    return mozilla::MakeSpan(base, nscopes);
  }
  mozilla::Span<GCPtrValue> consts() {
    return packedOffsetToSpan<GCPtrValue>(packedOffsets.constsSpanOffset);
  }
  mozilla::Span<GCPtrObject> objects() {
    return packedOffsetToSpan<GCPtrObject>(packedOffsets.objectsSpanOffset);
  }
  mozilla::Span<JSTryNote> tryNotes() {
    return packedOffsetToSpan<JSTryNote>(packedOffsets.tryNotesSpanOffset);
  }
  mozilla::Span<ScopeNote> scopeNotes() {
    return packedOffsetToSpan<ScopeNote>(packedOffsets.scopeNotesSpanOffset);
  }
  mozilla::Span<uint32_t> resumeOffsets() {
    return packedOffsetToSpan<uint32_t>(packedOffsets.resumeOffsetsSpanOffset);
  }

  // Fast tests for if array exists
  bool hasConsts() const { return packedOffsets.constsSpanOffset != 0; }
  bool hasObjects() const { return packedOffsets.objectsSpanOffset != 0; }
  bool hasTryNotes() const { return packedOffsets.tryNotesSpanOffset != 0; }
  bool hasScopeNotes() const { return packedOffsets.scopeNotesSpanOffset != 0; }
  bool hasResumeOffsets() const {
    return packedOffsets.resumeOffsetsSpanOffset != 0;
  }
  void setFieldInitializers(FieldInitializers fieldInitializers) {
    fieldInitializers_ = fieldInitializers;
  }
  const FieldInitializers& getFieldInitializers() { return fieldInitializers_; }

  // Allocate a new PrivateScriptData. Headers and GCPtrs are initialized.
  // The size of allocation is returned as an out parameter.
  static PrivateScriptData* new_(JSContext* cx, uint32_t nscopes,
                                 uint32_t nconsts, uint32_t nobjects,
                                 uint32_t ntrynotes, uint32_t nscopenotes,
                                 uint32_t nresumeoffsets, uint32_t* dataSize);

  template <XDRMode mode>
  static MOZ_MUST_USE XDRResult XDR(js::XDRState<mode>* xdr,
                                    js::HandleScript script,
                                    js::HandleScriptSourceObject sourceObject,
                                    js::HandleScope scriptEnclosingScope,
                                    js::HandleFunction fun);

  // Clone src script data into dst script.
  static bool Clone(JSContext* cx, js::HandleScript src, js::HandleScript dst,
                    js::MutableHandle<JS::GCVector<js::Scope*>> scopes);

  static bool InitFromEmitter(JSContext* cx, js::HandleScript script,
                              js::frontend::BytecodeEmitter* bce);

  void trace(JSTracer* trc);

  // PrivateScriptData has trailing data so isn't copyable or movable.
  PrivateScriptData(const PrivateScriptData&) = delete;
  PrivateScriptData& operator=(const PrivateScriptData&) = delete;
};

/*
 * Common data that can be shared between many scripts in a single runtime.
 */
class alignas(uintptr_t) SharedScriptData final {
  // This class is reference counted as follows: each pointer from a JSScript
  // counts as one reference plus there may be one reference from the shared
  // script data table.
  mozilla::Atomic<uint32_t, mozilla::SequentiallyConsistent,
                  mozilla::recordreplay::Behavior::DontPreserve>
      refCount_ = {};

  uint32_t codeOffset_ = 0;  // Byte-offset from 'this'
  uint32_t codeLength_ = 0;
  uint32_t tailOffset_ = 0;

  // Offset of main entry point from code, after predef'ing prologue.
  uint32_t mainOffset = 0;

  // Fixed frame slots.
  uint32_t nfixed = 0;

  // Slots plus maximum stack depth.
  uint32_t nslots = 0;

  // Index into the scopes array of the body scope.
  uint32_t bodyScopeIndex = 0;

  // Number of IC entries to allocate in ICScript for Baseline ICs.
  uint32_t numICEntries = 0;

  // ES6 function length.
  uint16_t funLength = 0;

  // Number of type sets used in this script for dynamic type monitoring.
  uint16_t numBytecodeTypeSets = 0;

  // NOTE: The raw bytes of this structure are used for hashing so use explicit
  // padding values as needed for predicatable results across compilers.

  friend class ::JSScript;

 private:
  // Layout of trailing arrays
  size_t atomOffset() const { return offsetOfAtoms(); }
  size_t codeOffset() const { return codeOffset_; }
  size_t noteOffset() const { return codeOffset_ + codeLength_; }

  // Size to allocate
  static size_t AllocationSize(uint32_t codeLength, uint32_t noteLength,
                               uint32_t natoms);

  template <typename T>
  void initElements(size_t offset, size_t length);

  // Initialize to GC-safe state
  SharedScriptData(uint32_t codeLength, uint32_t noteLength, uint32_t natoms);

 public:
  static SharedScriptData* new_(JSContext* cx, uint32_t codeLength,
                                uint32_t noteLength, uint32_t natoms);

  uint32_t refCount() const { return refCount_; }
  void AddRef() { refCount_++; }
  void Release() {
    MOZ_ASSERT(refCount_ != 0);
    uint32_t remain = --refCount_;
    if (remain == 0) {
      js_free(this);
    }
  }

  // Span over all raw bytes in this struct and its trailing arrays.
  mozilla::Span<const uint8_t> allocSpan() const {
    size_t allocSize = tailOffset_;
    return mozilla::MakeSpan(reinterpret_cast<const uint8_t*>(this), allocSize);
  }

  // Span over all immutable bytes in allocation. This excludes part of
  // structure used for reference counting and is the basis of how we
  // de-duplicate data.
  mozilla::Span<const uint8_t> immutableData() const {
    // The refCount_ must be first field of structure.
    static_assert(offsetof(SharedScriptData, refCount_) == 0,
                  "refCount_ must be at start of SharedScriptData");
    constexpr size_t dataOffset = sizeof(refCount_);

    static_assert(offsetof(SharedScriptData, numBytecodeTypeSets) +
                          sizeof(numBytecodeTypeSets) ==
                      sizeof(SharedScriptData),
                  "SharedScriptData should not have padding after last field");

    return allocSpan().From(dataOffset);
  }

  uint32_t natoms() const {
    return (codeOffset_ - atomOffset()) / sizeof(GCPtrAtom);
  }
  GCPtrAtom* atoms() {
    uintptr_t base = reinterpret_cast<uintptr_t>(this);
    return reinterpret_cast<GCPtrAtom*>(base + atomOffset());
  }

  uint32_t codeLength() const { return codeLength_; }
  jsbytecode* code() {
    uintptr_t base = reinterpret_cast<uintptr_t>(this);
    return reinterpret_cast<jsbytecode*>(base + codeOffset_);
  }

  uint32_t noteLength() const { return tailOffset_ - noteOffset(); }
  jssrcnote* notes() {
    uintptr_t base = reinterpret_cast<uintptr_t>(this);
    return reinterpret_cast<jssrcnote*>(base + noteOffset());
  }

  static constexpr size_t offsetOfCodeOffset() {
    return offsetof(SharedScriptData, codeOffset_);
  }
  static constexpr size_t offsetOfNfixed() {
    return offsetof(SharedScriptData, nfixed);
  }
  static constexpr size_t offsetOfNslots() {
    return offsetof(SharedScriptData, nslots);
  }
  static constexpr size_t offsetOfFunLength() {
    return offsetof(SharedScriptData, funLength);
  }
  static constexpr size_t offsetOfAtoms() { return sizeof(SharedScriptData); }

  void traceChildren(JSTracer* trc);

  template <XDRMode mode>
  static MOZ_MUST_USE XDRResult XDR(js::XDRState<mode>* xdr,
                                    js::HandleScript script);

  static bool InitFromEmitter(JSContext* cx, js::HandleScript script,
                              js::frontend::BytecodeEmitter* bce,
                              uint32_t nslots);

  // Mark this SharedScriptData for use in a new zone
  void markForCrossZone(JSContext* cx);

  // SharedScriptData has trailing data so isn't copyable or movable.
  SharedScriptData(const SharedScriptData&) = delete;
  SharedScriptData& operator=(const SharedScriptData&) = delete;
};

// Two SharedScriptData instances may be de-duplicated if they have the same
// data in their immutableData() span. This Hasher enables that comparison.
struct SharedScriptDataHasher {
  using Lookup = RefPtr<SharedScriptData>;

  static HashNumber hash(const Lookup& l) {
    mozilla::Span<const uint8_t> immutableData = l->immutableData();
    return mozilla::HashBytes(immutableData.data(), immutableData.size());
  }

  static bool match(SharedScriptData* entry, const Lookup& lookup) {
    return entry->immutableData() == lookup->immutableData();
  }
};

class AutoLockScriptData;

using ScriptDataTable =
    HashSet<SharedScriptData*, SharedScriptDataHasher, SystemAllocPolicy>;

extern void SweepScriptData(JSRuntime* rt);

extern void FreeScriptData(JSRuntime* rt);

} /* namespace js */

namespace JS {

// Define a GCManagedDeletePolicy to allow deleting type outside of normal
// sweeping.
template <>
struct DeletePolicy<js::PrivateScriptData>
    : public js::GCManagedDeletePolicy<js::PrivateScriptData> {};

} /* namespace JS */

class JSScript : public js::gc::TenuredCell {
 private:
  // Pointer to baseline->method()->raw(), ion->method()->raw(), a wasm jit
  // entry, the JIT's EnterInterpreter stub, or the lazy link stub. Must be
  // non-null.
  uint8_t* jitCodeRaw_ = nullptr;
  uint8_t* jitCodeSkipArgCheck_ = nullptr;

  // Shareable script data
  RefPtr<js::SharedScriptData> scriptData_ = {};

  // Unshared variable-length data
  js::PrivateScriptData* data_ = nullptr;

 public:
  JS::Realm* realm_ = nullptr;

 private:
  /* Persistent type information retained across GCs. */
  js::TypeScript* types_ = nullptr;

  // This script's ScriptSourceObject.
  js::GCPtr<js::ScriptSourceObject*> sourceObject_ = {};

  /*
   * Information attached by Ion. Nexto a valid IonScript this could be
   * ION_DISABLED_SCRIPT, ION_COMPILING_SCRIPT or ION_PENDING_SCRIPT.
   * The later is a ion compilation that is ready, but hasn't been linked
   * yet.
   */
  js::jit::IonScript* ion = nullptr;

  /* Information attached by Baseline. */
  js::jit::BaselineScript* baseline = nullptr;

  /* Information used to re-lazify a lazily-parsed interpreted function. */
  js::LazyScript* lazyScript = nullptr;

  // 32-bit fields.

  /* Size of the used part of the data array. */
  uint32_t dataSize_ = 0;

  /* Base line number of script. */
  uint32_t lineno_ = 0;

  /* Base column of script, optionally set. */
  uint32_t column_ = 0;

  // Range of characters in scriptSource which contains this script's
  // source, that is, the range used by the Parser to produce this script.
  //
  // Most scripted functions have sourceStart_ == toStringStart_ and
  // sourceEnd_ == toStringEnd_. However, for functions with extra
  // qualifiers (e.g. generators, async) and for class constructors (which
  // need to return the entire class source), their values differ.
  //
  // Each field points the following locations.
  //
  //   function * f(a, b) { return a + b; }
  //   ^          ^                        ^
  //   |          |                        |
  //   |          sourceStart_             sourceEnd_
  //   |                                   |
  //   toStringStart_                      toStringEnd_
  //
  // And, in the case of class constructors, an additional toStringEnd
  // offset is used.
  //
  //   class C { constructor() { this.field = 42; } }
  //   ^         ^                                 ^ ^
  //   |         |                                 | `---------`
  //   |         sourceStart_                      sourceEnd_  |
  //   |                                                       |
  //   toStringStart_                                          toStringEnd_
  uint32_t sourceStart_ = 0;
  uint32_t sourceEnd_ = 0;
  uint32_t toStringStart_ = 0;
  uint32_t toStringEnd_ = 0;

  // Number of times the script has been called or has had backedges taken.
  // When running in ion, also increased for any inlined scripts. Reset if
  // the script's JIT code is forcibly discarded.
  mozilla::Atomic<uint32_t, mozilla::Relaxed,
                  mozilla::recordreplay::Behavior::DontPreserve>
      warmUpCount = {};

  // Immutable flags should not be modified after this script has been
  // initialized. These flags should likely be preserved when serializing
  // (XDR) or copying (CopyScript) this script. This is only public for the
  // JITs.
 public:
  enum class ImmutableFlags : uint32_t {
    // No need for result value of last expression statement.
    NoScriptRval = 1 << 0,

    // Code is in strict mode.
    Strict = 1 << 1,

    // (1 << 2) is unused.

    // True if the script has a non-syntactic scope on its dynamic scope chain.
    // That is, there are objects about which we know nothing between the
    // outermost syntactic scope and the global.
    HasNonSyntacticScope = 1 << 3,

    // See Parser::selfHostingMode.
    SelfHosted = 1 << 4,

    // See FunctionBox.
    BindingsAccessedDynamically = 1 << 5,
    FunHasExtensibleScope = 1 << 6,

    // (1 << 7) is unused.

    // Script has singleton objects.
    HasSingletons = 1 << 8,

    FunctionHasThisBinding = 1 << 9,
    FunctionHasExtraBodyVarScope = 1 << 10,

    // Whether the arguments object for this script, if it needs one, should be
    // mapped (alias formal parameters).
    HasMappedArgsObj = 1 << 11,

    // Script contains inner functions. Used to check if we can relazify the
    // script.
    HasInnerFunctions = 1 << 12,

    NeedsHomeObject = 1 << 13,

    IsDerivedClassConstructor = 1 << 14,
    IsDefaultClassConstructor = 1 << 15,

    // Script is a lambda to treat as running once or a global or eval script
    // that will only run once.  Which one it is can be disambiguated by
    // checking whether function() is null.
    TreatAsRunOnce = 1 << 16,

    // 'this', 'arguments' and f.apply() are used. This is likely to be a
    // wrapper.
    IsLikelyConstructorWrapper = 1 << 17,

    // Set if this function is a generator function or async generator.
    IsGenerator = 1 << 18,

    // Set if this function is an async function or async generator.
    IsAsync = 1 << 19,

    // Set if this function has a rest parameter.
    HasRest = 1 << 20,

    // See comments below.
    ArgsHasVarBinding = 1 << 21,

    // Script came from eval().
    IsForEval = 1 << 22,

    // Whether this is a top-level module script.
    IsModule = 1 << 23,

    // Whether this function needs a call object or named lambda environment.
    NeedsFunctionEnvironmentObjects = 1 << 24,

    // LazyScript flags
    ShouldDeclareArguments = 1 << 25,
    IsBinAST = 1 << 26,
    HasDebuggerStatement = 1 << 27,
    HasDirectEval = 1 << 28,
  };

 private:
  // Note: don't make this a bitfield! It makes it hard to read these flags
  // from JIT code.
  uint32_t immutableFlags_ = 0;

  // Mutable flags typically store information about runtime or deoptimization
  // behavior of this script. This is only public for the JITs.
 public:
  enum class MutableFlags : uint32_t {
    // Number of times the |warmUpCount| was forcibly discarded. The counter is
    // reset when a script is successfully jit-compiled.
    WarmupResets_MASK = 0xFF,

    // Have warned about uses of undefined properties in this script.
    WarnedAboutUndefinedProp = 1 << 8,

    // If treatAsRunOnce, whether script has executed.
    HasRunOnce = 1 << 9,

    // Script has been reused for a clone.
    HasBeenCloned = 1 << 10,

    // Whether the record/replay execution progress counter (see RecordReplay.h)
    // should be updated as this script runs.
    TrackRecordReplayProgress = 1 << 11,

    // Script has an entry in Realm::scriptCountsMap.
    HasScriptCounts = 1 << 12,

    // Script has an entry in Realm::debugScriptMap.
    HasDebugScript = 1 << 13,

    // Do not relazify this script. This is used by the relazify() testing
    // function for scripts that are on the stack and also by the AutoDelazify
    // RAII class. Usually we don't relazify functions in compartments with
    // scripts on the stack, but the relazify() testing function overrides that,
    // and sometimes we're working with a cross-compartment function and need to
    // keep it from relazifying.
    DoNotRelazify = 1 << 14,

    // IonMonkey compilation hints.

    // Script has had hoisted bounds checks fail.
    FailedBoundsCheck = 1 << 15,

    // Script has had hoisted shape guard fail.
    FailedShapeGuard = 1 << 16,

    HadFrequentBailouts = 1 << 17,
    HadOverflowBailout = 1 << 18,

    // Explicitly marked as uninlineable.
    Uninlineable = 1 << 19,

    // Idempotent cache has triggered invalidation.
    InvalidatedIdempotentCache = 1 << 20,

    // Lexical check did fail and bail out.
    FailedLexicalCheck = 1 << 21,

    // See comments below.
    NeedsArgsAnalysis = 1 << 22,
    NeedsArgsObj = 1 << 23,

    // Set if the debugger's onNewScript hook has not yet been called.
    HideScriptFromDebugger = 1 << 24,

    // Set if the script has opted into spew
    SpewEnabled = 1 << 25,
  };

 private:
  // Note: don't make this a bitfield! It makes it hard to read these flags
  // from JIT code.
  uint32_t mutableFlags_ = 0;

  //
  // End of fields.  Start methods.
  //

 private:
  template <js::XDRMode mode>
  friend js::XDRResult js::XDRScript(js::XDRState<mode>* xdr,
                                     js::HandleScope enclosingScope,
                                     js::HandleScriptSourceObject sourceObject,
                                     js::HandleFunction fun,
                                     js::MutableHandleScript scriptp);

  template <js::XDRMode mode>
  friend js::XDRResult js::SharedScriptData::XDR(js::XDRState<mode>* xdr,
                                                 js::HandleScript script);

  friend bool js::SharedScriptData::InitFromEmitter(
      JSContext* cx, js::HandleScript script,
      js::frontend::BytecodeEmitter* bce, uint32_t nslot);

  template <js::XDRMode mode>
  friend js::XDRResult js::PrivateScriptData::XDR(
      js::XDRState<mode>* xdr, js::HandleScript script,
      js::HandleScriptSourceObject sourceObject,
      js::HandleScope scriptEnclosingScope, js::HandleFunction fun);

  friend bool js::PrivateScriptData::Clone(
      JSContext* cx, js::HandleScript src, js::HandleScript dst,
      js::MutableHandle<JS::GCVector<js::Scope*>> scopes);

  friend bool js::PrivateScriptData::InitFromEmitter(
      JSContext* cx, js::HandleScript script,
      js::frontend::BytecodeEmitter* bce);

  friend JSScript* js::detail::CopyScript(
      JSContext* cx, js::HandleScript src,
      js::HandleScriptSourceObject sourceObject,
      js::MutableHandle<JS::GCVector<js::Scope*>> scopes);

 private:
  JSScript(JS::Realm* realm, uint8_t* stubEntry,
           js::HandleScriptSourceObject sourceObject, uint32_t sourceStart,
           uint32_t sourceEnd, uint32_t toStringStart, uint32_t toStringend);

  static JSScript* New(JSContext* cx, js::HandleScriptSourceObject sourceObject,
                       uint32_t sourceStart, uint32_t sourceEnd,
                       uint32_t toStringStart, uint32_t toStringEnd);

 public:
  static JSScript* Create(JSContext* cx,
                          const JS::ReadOnlyCompileOptions& options,
                          js::HandleScriptSourceObject sourceObject,
                          uint32_t sourceStart, uint32_t sourceEnd,
                          uint32_t toStringStart, uint32_t toStringEnd);

  // NOTE: If you use createPrivateScriptData directly instead of via
  // fullyInitFromEmitter, you are responsible for notifying the debugger
  // after successfully creating the script.
  static bool createPrivateScriptData(JSContext* cx,
                                      JS::Handle<JSScript*> script,
                                      uint32_t nscopes, uint32_t nconsts,
                                      uint32_t nobjects, uint32_t ntrynotes,
                                      uint32_t nscopenotes,
                                      uint32_t nresumeoffsets);

 private:
  void initFromFunctionBox(js::frontend::FunctionBox* funbox);

 public:
  static bool fullyInitFromEmitter(JSContext* cx, js::HandleScript script,
                                   js::frontend::BytecodeEmitter* bce);

  // Initialize the Function.prototype script.
  static bool initFunctionPrototype(JSContext* cx, js::HandleScript script,
                                    JS::HandleFunction functionProto);

#ifdef DEBUG
 private:
  // Assert that jump targets are within the code array of the script.
  void assertValidJumpTargets() const;

 public:
#endif

  // MutableFlags accessors.

  MOZ_MUST_USE bool hasFlag(MutableFlags flag) const {
    return mutableFlags_ & uint32_t(flag);
  }
  void setFlag(MutableFlags flag) { mutableFlags_ |= uint32_t(flag); }
  void setFlag(MutableFlags flag, bool b) {
    if (b) {
      setFlag(flag);
    } else {
      clearFlag(flag);
    }
  }
  void clearFlag(MutableFlags flag) { mutableFlags_ &= ~uint32_t(flag); }

  // ImmutableFlags accessors.

 public:
  MOZ_MUST_USE bool hasFlag(ImmutableFlags flag) const {
    return immutableFlags_ & uint32_t(flag);
  }

 private:
  void setFlag(ImmutableFlags flag) { immutableFlags_ |= uint32_t(flag); }
  void setFlag(ImmutableFlags flag, bool b) {
    if (b) {
      setFlag(flag);
    } else {
      clearFlag(flag);
    }
  }
  void clearFlag(ImmutableFlags flag) { immutableFlags_ &= ~uint32_t(flag); }

 public:
  inline JSPrincipals* principals();

  JS::Compartment* compartment() const {
    return JS::GetCompartmentForRealm(realm_);
  }
  JS::Compartment* maybeCompartment() const { return compartment(); }
  JS::Realm* realm() const { return realm_; }

  js::SharedScriptData* scriptData() { return scriptData_; }

  // Script bytecode is immutable after creation.
  jsbytecode* code() const {
    if (!scriptData_) {
      return nullptr;
    }
    return scriptData_->code();
  }

  bool hasForceInterpreterOp() const {
    // JSOP_FORCEINTERPRETER, if present, must be the first op.
    MOZ_ASSERT(length() >= 1);
    return JSOp(*code()) == JSOP_FORCEINTERPRETER;
  }

  js::AllBytecodesIterable allLocations() {
    return js::AllBytecodesIterable(this);
  }

  js::BytecodeLocation location() { return js::BytecodeLocation(this, code()); }

  bool isUncompleted() const {
    // code() becomes non-null only if this script is complete.
    // See the comment in JSScript::fullyInitFromEmitter.
    return !code();
  }

  size_t length() const {
    MOZ_ASSERT(scriptData_);
    return scriptData_->codeLength();
  }

  jsbytecode* codeEnd() const { return code() + length(); }

  jsbytecode* lastPC() const {
    jsbytecode* pc = codeEnd() - js::JSOP_RETRVAL_LENGTH;
    MOZ_ASSERT(*pc == JSOP_RETRVAL);
    return pc;
  }

  bool containsPC(const jsbytecode* pc) const {
    return pc >= code() && pc < codeEnd();
  }

  bool contains(const js::BytecodeLocation& loc) const {
    return containsPC(loc.toRawBytecode());
  }

  size_t pcToOffset(const jsbytecode* pc) const {
    MOZ_ASSERT(containsPC(pc));
    return size_t(pc - code());
  }

  jsbytecode* offsetToPC(size_t offset) const {
    MOZ_ASSERT(offset < length());
    return code() + offset;
  }

  size_t mainOffset() const { return scriptData_->mainOffset; }

  uint32_t lineno() const { return lineno_; }

  uint32_t column() const { return column_; }

  void setColumn(size_t column) { column_ = column; }

  // The fixed part of a stack frame is comprised of vars (in function and
  // module code) and block-scoped locals (in all kinds of code).
  size_t nfixed() const { return scriptData_->nfixed; }

  // Number of fixed slots reserved for slots that are always live. Only
  // nonzero for function or module code.
  size_t numAlwaysLiveFixedSlots() const {
    if (bodyScope()->is<js::FunctionScope>()) {
      return bodyScope()->as<js::FunctionScope>().nextFrameSlot();
    }
    if (bodyScope()->is<js::ModuleScope>()) {
      return bodyScope()->as<js::ModuleScope>().nextFrameSlot();
    }
    return 0;
  }

  // Calculate the number of fixed slots that are live at a particular bytecode.
  size_t calculateLiveFixed(jsbytecode* pc);

  size_t nslots() const { return scriptData_->nslots; }

  unsigned numArgs() const {
    if (bodyScope()->is<js::FunctionScope>()) {
      return bodyScope()
          ->as<js::FunctionScope>()
          .numPositionalFormalParameters();
    }
    return 0;
  }

  inline js::Shape* initialEnvironmentShape() const;

  bool functionHasParameterExprs() const {
    // Only functions have parameters.
    js::Scope* scope = bodyScope();
    if (!scope->is<js::FunctionScope>()) {
      return false;
    }
    return scope->as<js::FunctionScope>().hasParameterExprs();
  }

  // If there are more than MaxBytecodeTypeSets JOF_TYPESET ops in the script,
  // the first MaxBytecodeTypeSets - 1 JOF_TYPESET ops have their own TypeSet
  // and all other JOF_TYPESET ops share the last TypeSet.
  static constexpr size_t MaxBytecodeTypeSets = UINT16_MAX;
  static_assert(sizeof(js::SharedScriptData::numBytecodeTypeSets) == 2,
                "MaxBytecodeTypeSets must match sizeof(numBytecodeTypeSets)");

  size_t numBytecodeTypeSets() const {
    return scriptData_->numBytecodeTypeSets;
  }

  size_t numICEntries() const { return scriptData_->numICEntries; }

  size_t funLength() const { return scriptData_->funLength; }

  uint32_t sourceStart() const { return sourceStart_; }

  uint32_t sourceEnd() const { return sourceEnd_; }

  uint32_t sourceLength() const { return sourceEnd_ - sourceStart_; }

  uint32_t toStringStart() const { return toStringStart_; }

  uint32_t toStringEnd() const { return toStringEnd_; }

  bool noScriptRval() const { return hasFlag(ImmutableFlags::NoScriptRval); }

  bool strict() const { return hasFlag(ImmutableFlags::Strict); }

  bool hasNonSyntacticScope() const {
    return hasFlag(ImmutableFlags::HasNonSyntacticScope);
  }

  bool selfHosted() const { return hasFlag(ImmutableFlags::SelfHosted); }
  bool bindingsAccessedDynamically() const {
    return hasFlag(ImmutableFlags::BindingsAccessedDynamically);
  }
  bool funHasExtensibleScope() const {
    return hasFlag(ImmutableFlags::FunHasExtensibleScope);
  }

  bool hasSingletons() const { return hasFlag(ImmutableFlags::HasSingletons); }
  bool treatAsRunOnce() const {
    return hasFlag(ImmutableFlags::TreatAsRunOnce);
  }
  bool hasRunOnce() const { return hasFlag(MutableFlags::HasRunOnce); }
  bool hasBeenCloned() const { return hasFlag(MutableFlags::HasBeenCloned); }

  void setTreatAsRunOnce() { setFlag(ImmutableFlags::TreatAsRunOnce); }
  void setHasRunOnce() { setFlag(MutableFlags::HasRunOnce); }
  void setHasBeenCloned() { setFlag(MutableFlags::HasBeenCloned); }

  void cacheForEval() {
    MOZ_ASSERT(isForEval());
    // IsEvalCacheCandidate will make sure that there's nothing in this
    // script that would prevent reexecution even if isRunOnce is
    // true.  So just pretend like we never ran this script.
    clearFlag(MutableFlags::HasRunOnce);
  }

  bool isLikelyConstructorWrapper() const {
    return hasFlag(ImmutableFlags::IsLikelyConstructorWrapper);
  }
  void setLikelyConstructorWrapper() {
    setFlag(ImmutableFlags::IsLikelyConstructorWrapper);
  }

  bool failedBoundsCheck() const {
    return hasFlag(MutableFlags::FailedBoundsCheck);
  }
  bool failedShapeGuard() const {
    return hasFlag(MutableFlags::FailedShapeGuard);
  }
  bool hadFrequentBailouts() const {
    return hasFlag(MutableFlags::HadFrequentBailouts);
  }
  bool hadOverflowBailout() const {
    return hasFlag(MutableFlags::HadOverflowBailout);
  }
  bool uninlineable() const { return hasFlag(MutableFlags::Uninlineable); }
  bool invalidatedIdempotentCache() const {
    return hasFlag(MutableFlags::InvalidatedIdempotentCache);
  }
  bool failedLexicalCheck() const {
    return hasFlag(MutableFlags::FailedLexicalCheck);
  }
  bool isDefaultClassConstructor() const {
    return hasFlag(ImmutableFlags::IsDefaultClassConstructor);
  }

  void setFailedBoundsCheck() { setFlag(MutableFlags::FailedBoundsCheck); }
  void setFailedShapeGuard() { setFlag(MutableFlags::FailedShapeGuard); }
  void setHadFrequentBailouts() { setFlag(MutableFlags::HadFrequentBailouts); }
  void setHadOverflowBailout() { setFlag(MutableFlags::HadOverflowBailout); }
  void setUninlineable() { setFlag(MutableFlags::Uninlineable); }
  void setInvalidatedIdempotentCache() {
    setFlag(MutableFlags::InvalidatedIdempotentCache);
  }
  void setFailedLexicalCheck() { setFlag(MutableFlags::FailedLexicalCheck); }
  void setIsDefaultClassConstructor() {
    setFlag(ImmutableFlags::IsDefaultClassConstructor);
  }

  bool hasScriptCounts() const {
    return hasFlag(MutableFlags::HasScriptCounts);
  }
  bool hasScriptName();

  bool warnedAboutUndefinedProp() const {
    return hasFlag(MutableFlags::WarnedAboutUndefinedProp);
  }
  void setWarnedAboutUndefinedProp() {
    setFlag(MutableFlags::WarnedAboutUndefinedProp);
  }

  /* See ContextFlags::funArgumentsHasLocalBinding comment. */
  bool argumentsHasVarBinding() const {
    return hasFlag(ImmutableFlags::ArgsHasVarBinding);
  }
  void setArgumentsHasVarBinding();
  bool argumentsAliasesFormals() const {
    return argumentsHasVarBinding() && hasMappedArgsObj();
  }

  js::GeneratorKind generatorKind() const {
    return isGenerator() ? js::GeneratorKind::Generator
                         : js::GeneratorKind::NotGenerator;
  }
  bool isGenerator() const { return hasFlag(ImmutableFlags::IsGenerator); }

  js::FunctionAsyncKind asyncKind() const {
    return isAsync() ? js::FunctionAsyncKind::AsyncFunction
                     : js::FunctionAsyncKind::SyncFunction;
  }
  bool isAsync() const { return hasFlag(ImmutableFlags::IsAsync); }

  bool hasRest() const { return hasFlag(ImmutableFlags::HasRest); }

  bool hideScriptFromDebugger() const {
    return hasFlag(MutableFlags::HideScriptFromDebugger);
  }
  void clearHideScriptFromDebugger() {
    clearFlag(MutableFlags::HideScriptFromDebugger);
  }

  bool spewEnabled() const { return hasFlag(MutableFlags::SpewEnabled); }
  void setSpewEnabled(bool enabled) {
    setFlag(MutableFlags::SpewEnabled, enabled);
  }

  bool needsHomeObject() const {
    return hasFlag(ImmutableFlags::NeedsHomeObject);
  }

  bool isDerivedClassConstructor() const {
    return hasFlag(ImmutableFlags::IsDerivedClassConstructor);
  }

  /*
   * As an optimization, even when argsHasLocalBinding, the function prologue
   * may not need to create an arguments object. This is determined by
   * needsArgsObj which is set by AnalyzeArgumentsUsage. When !needsArgsObj,
   * the prologue may simply write MagicValue(JS_OPTIMIZED_ARGUMENTS) to
   * 'arguments's slot and any uses of 'arguments' will be guaranteed to
   * handle this magic value. To avoid spurious arguments object creation, we
   * maintain the invariant that needsArgsObj is only called after the script
   * has been analyzed.
   */
  bool analyzedArgsUsage() const {
    return !hasFlag(MutableFlags::NeedsArgsAnalysis);
  }
  inline bool ensureHasAnalyzedArgsUsage(JSContext* cx);
  bool needsArgsObj() const {
    MOZ_ASSERT(analyzedArgsUsage());
    return hasFlag(MutableFlags::NeedsArgsObj);
  }
  void setNeedsArgsObj(bool needsArgsObj);
  static void argumentsOptimizationFailed(JSContext* cx,
                                          js::HandleScript script);

  bool hasMappedArgsObj() const {
    return hasFlag(ImmutableFlags::HasMappedArgsObj);
  }

  bool functionHasThisBinding() const {
    return hasFlag(ImmutableFlags::FunctionHasThisBinding);
  }

  void setFieldInitializers(js::FieldInitializers fieldInitializers) {
    MOZ_ASSERT(data_);
    data_->setFieldInitializers(fieldInitializers);
  }

  const js::FieldInitializers& getFieldInitializers() const {
    MOZ_ASSERT(data_);
    return data_->getFieldInitializers();
  }

  /*
   * Arguments access (via JSOP_*ARG* opcodes) must access the canonical
   * location for the argument. If an arguments object exists AND it's mapped
   * ('arguments' aliases formals), then all access must go through the
   * arguments object. Otherwise, the local slot is the canonical location for
   * the arguments. Note: if a formal is aliased through the scope chain, then
   * script->formalIsAliased and JSOP_*ARG* opcodes won't be emitted at all.
   */
  bool argsObjAliasesFormals() const {
    return needsArgsObj() && hasMappedArgsObj();
  }

  void setDoNotRelazify(bool b) { setFlag(MutableFlags::DoNotRelazify, b); }

  bool hasInnerFunctions() const {
    return hasFlag(ImmutableFlags::HasInnerFunctions);
  }

  static constexpr size_t offsetOfMutableFlags() {
    return offsetof(JSScript, mutableFlags_);
  }
  static size_t offsetOfImmutableFlags() {
    return offsetof(JSScript, immutableFlags_);
  }
  static constexpr size_t offsetOfScriptData() {
    return offsetof(JSScript, scriptData_);
  }
  static constexpr size_t offsetOfTypes() { return offsetof(JSScript, types_); }

  bool hasAnyIonScript() const { return hasIonScript(); }

  bool hasIonScript() const {
    bool res = ion && ion != ION_DISABLED_SCRIPT &&
               ion != ION_COMPILING_SCRIPT && ion != ION_PENDING_SCRIPT;
    MOZ_ASSERT_IF(res, baseline);
    return res;
  }
  bool canIonCompile() const { return ion != ION_DISABLED_SCRIPT; }
  bool isIonCompilingOffThread() const { return ion == ION_COMPILING_SCRIPT; }

  js::jit::IonScript* ionScript() const {
    MOZ_ASSERT(hasIonScript());
    return ion;
  }
  js::jit::IonScript* maybeIonScript() const { return ion; }
  js::jit::IonScript* const* addressOfIonScript() const { return &ion; }
  void setIonScript(JSRuntime* rt, js::jit::IonScript* ionScript);

  bool hasBaselineScript() const {
    bool res = baseline && baseline != BASELINE_DISABLED_SCRIPT;
    MOZ_ASSERT_IF(!res, !ion || ion == ION_DISABLED_SCRIPT);
    return res;
  }
  bool canBaselineCompile() const {
    return baseline != BASELINE_DISABLED_SCRIPT;
  }
  js::jit::BaselineScript* baselineScript() const {
    MOZ_ASSERT(hasBaselineScript());
    return baseline;
  }
  inline void setBaselineScript(JSRuntime* rt,
                                js::jit::BaselineScript* baselineScript);

  inline js::jit::ICScript* icScript() const;

  bool hasICScript() const {
    // ICScript is stored in TypeScript so we have an ICScript iff we have a
    // TypeScript.
    return !!types_;
  }

  void updateJitCodeRaw(JSRuntime* rt);

  static size_t offsetOfBaselineScript() {
    return offsetof(JSScript, baseline);
  }
  static size_t offsetOfIonScript() { return offsetof(JSScript, ion); }
  static constexpr size_t offsetOfJitCodeRaw() {
    return offsetof(JSScript, jitCodeRaw_);
  }
  static constexpr size_t offsetOfJitCodeSkipArgCheck() {
    return offsetof(JSScript, jitCodeSkipArgCheck_);
  }
  uint8_t* jitCodeRaw() const { return jitCodeRaw_; }

  // We don't relazify functions with a TypeScript or JIT code, but some
  // callers (XDR, testing functions) want to know whether this script is
  // relazifiable ignoring (or after) discarding JIT code.
  bool isRelazifiableIgnoringJitCode() const {
    return (selfHosted() || lazyScript) && !hasInnerFunctions() &&
           !isGenerator() && !isAsync() && !isDefaultClassConstructor() &&
           !hasFlag(MutableFlags::DoNotRelazify);
  }
  bool isRelazifiable() const {
    MOZ_ASSERT_IF(hasBaselineScript() || hasIonScript(), types_);
    return isRelazifiableIgnoringJitCode() && !types_;
  }
  void setLazyScript(js::LazyScript* lazy) { lazyScript = lazy; }
  js::LazyScript* maybeLazyScript() { return lazyScript; }

  /*
   * Original compiled function for the script, if it has a function.
   * nullptr for global and eval scripts.
   * The delazifying variant ensures that the function isn't lazy. The
   * non-delazifying variant must only be used after earlier code has
   * called ensureNonLazyCanonicalFunction and while the function can't
   * have been relazified.
   */
  inline JSFunction* functionDelazifying() const;
  JSFunction* functionNonDelazifying() const {
    if (bodyScope()->is<js::FunctionScope>()) {
      return bodyScope()->as<js::FunctionScope>().canonicalFunction();
    }
    return nullptr;
  }
  /*
   * De-lazifies the canonical function. Must be called before entering code
   * that expects the function to be non-lazy.
   */
  inline void ensureNonLazyCanonicalFunction();

  bool isModule() const {
    MOZ_ASSERT(hasFlag(ImmutableFlags::IsModule) ==
               bodyScope()->is<js::ModuleScope>());
    return hasFlag(ImmutableFlags::IsModule);
  }
  js::ModuleObject* module() const {
    if (isModule()) {
      return bodyScope()->as<js::ModuleScope>().module();
    }
    return nullptr;
  }

  bool isGlobalOrEvalCode() const {
    return bodyScope()->is<js::GlobalScope>() ||
           bodyScope()->is<js::EvalScope>();
  }
  bool isGlobalCode() const { return bodyScope()->is<js::GlobalScope>(); }

  // Returns true if the script may read formal arguments on the stack
  // directly, via lazy arguments or a rest parameter.
  bool mayReadFrameArgsDirectly();

  static JSFlatString* sourceData(JSContext* cx, JS::HandleScript script);

  MOZ_MUST_USE bool appendSourceDataForToString(JSContext* cx,
                                                js::StringBuffer& buf);

  void setSourceObject(js::ScriptSourceObject* object);
  js::ScriptSourceObject* sourceObject() const { return sourceObject_; }
  js::ScriptSource* scriptSource() const;
  js::ScriptSource* maybeForwardedScriptSource() const;

  void setDefaultClassConstructorSpan(js::ScriptSourceObject* sourceObject,
                                      uint32_t start, uint32_t end,
                                      unsigned line, unsigned column);

  bool mutedErrors() const { return scriptSource()->mutedErrors(); }
  const char* filename() const { return scriptSource()->filename(); }
  const char* maybeForwardedFilename() const {
    return maybeForwardedScriptSource()->filename();
  }

#ifdef MOZ_VTUNE
  // Unique Method ID passed to the VTune profiler. Allows attribution of
  // different jitcode to the same source script.
  uint32_t vtuneMethodID();
#endif

 public:
  /* Return whether this script was compiled for 'eval' */
  bool isForEval() const {
    bool forEval = hasFlag(ImmutableFlags::IsForEval);
    MOZ_ASSERT_IF(forEval, bodyScope()->is<js::EvalScope>());
    return forEval;
  }

  /* Return whether this is a 'direct eval' script in a function scope. */
  bool isDirectEvalInFunction() const {
    if (!isForEval()) {
      return false;
    }
    return bodyScope()->hasOnChain(js::ScopeKind::Function);
  }

  /*
   * Return whether this script is a top-level script.
   *
   * If we evaluate some code which contains a syntax error, then we might
   * produce a JSScript which has no associated bytecode. Testing with
   * |code()| filters out this kind of scripts.
   *
   * If this script has a function associated to it, then it is not the
   * top-level of a file.
   */
  bool isTopLevel() { return code() && !functionNonDelazifying(); }

  /* Ensure the script has a TypeScript. */
  inline bool ensureHasTypes(JSContext* cx, js::AutoKeepTypeScripts&);

  js::TypeScript* types() { return types_; }

  void maybeReleaseTypes();

  inline js::GlobalObject& global() const;
  inline bool hasGlobal(const js::GlobalObject* global) const;
  js::GlobalObject& uninlinedGlobal() const;

  uint32_t bodyScopeIndex() const { return scriptData_->bodyScopeIndex; }

  js::Scope* bodyScope() const { return getScope(bodyScopeIndex()); }

  js::Scope* outermostScope() const {
    // The body scope may not be the outermost scope in the script when
    // the decl env scope is present.
    size_t index = 0;
    return getScope(index);
  }

  bool needsFunctionEnvironmentObjects() const {
    return hasFlag(ImmutableFlags::NeedsFunctionEnvironmentObjects);
  }

  bool functionHasExtraBodyVarScope() const {
    bool res = hasFlag(ImmutableFlags::FunctionHasExtraBodyVarScope);
    MOZ_ASSERT_IF(res, functionHasParameterExprs());
    return res;
  }

  js::VarScope* functionExtraBodyVarScope() const {
    MOZ_ASSERT(functionHasExtraBodyVarScope());
    for (js::Scope* scope : scopes()) {
      if (scope->kind() == js::ScopeKind::FunctionBodyVar) {
        return &scope->as<js::VarScope>();
      }
    }
    MOZ_CRASH("Function extra body var scope not found");
  }

  bool needsBodyEnvironment() const {
    for (js::Scope* scope : scopes()) {
      if (ScopeKindIsInBody(scope->kind()) && scope->hasEnvironment()) {
        return true;
      }
    }
    return false;
  }

  inline js::LexicalScope* maybeNamedLambdaScope() const;

  js::Scope* enclosingScope() const { return outermostScope()->enclosing(); }

 private:
  bool makeTypes(JSContext* cx);

  bool createSharedScriptData(JSContext* cx, uint32_t codeLength,
                              uint32_t noteLength, uint32_t natoms);
  bool shareScriptData(JSContext* cx);
  void freeScriptData();

 public:
  uint32_t getWarmUpCount() const { return warmUpCount; }
  uint32_t incWarmUpCounter(uint32_t amount = 1) {
    return warmUpCount += amount;
  }
  uint32_t* addressOfWarmUpCounter() {
    return reinterpret_cast<uint32_t*>(&warmUpCount);
  }
  static size_t offsetOfWarmUpCounter() {
    return offsetof(JSScript, warmUpCount);
  }
  void resetWarmUpCounterForGC() {
    incWarmUpResetCounter();
    warmUpCount = 0;
  }

  void resetWarmUpCounterToDelayIonCompilation();

  unsigned getWarmUpResetCount() const {
    constexpr uint32_t MASK = uint32_t(MutableFlags::WarmupResets_MASK);
    return mutableFlags_ & MASK;
  }
  void incWarmUpResetCounter() {
    constexpr uint32_t MASK = uint32_t(MutableFlags::WarmupResets_MASK);
    uint32_t newCount = getWarmUpResetCount() + 1;
    if (newCount <= MASK) {
      mutableFlags_ &= ~MASK;
      mutableFlags_ |= newCount;
    }
  }
  void resetWarmUpResetCounter() {
    constexpr uint32_t MASK = uint32_t(MutableFlags::WarmupResets_MASK);
    mutableFlags_ &= ~MASK;
  }

 public:
  bool initScriptCounts(JSContext* cx);
  bool initScriptName(JSContext* cx);
  js::ScriptCounts& getScriptCounts();
  const char* getScriptName();
  js::PCCounts* maybeGetPCCounts(jsbytecode* pc);
  const js::PCCounts* maybeGetThrowCounts(jsbytecode* pc);
  js::PCCounts* getThrowCounts(jsbytecode* pc);
  uint64_t getHitCount(jsbytecode* pc);
  void incHitCount(jsbytecode* pc);  // Used when we bailout out of Ion.
  void addIonCounts(js::jit::IonScriptCounts* ionCounts);
  js::jit::IonScriptCounts* getIonCounts();
  void releaseScriptCounts(js::ScriptCounts* counts);
  void destroyScriptCounts();
  void destroyScriptName();
  void clearHasScriptCounts();
  void resetScriptCounts();

  jsbytecode* main() const { return code() + mainOffset(); }

  js::BytecodeLocation mainLocation() const {
    return js::BytecodeLocation(this, main());
  }

  js::BytecodeLocation endLocation() const {
    return js::BytecodeLocation(this, codeEnd());
  }

  /*
   * computedSizeOfData() is the in-use size of all the data sections.
   * sizeOfData() is the size of the block allocated to hold all the data
   * sections (which can be larger than the in-use size).
   */
  size_t computedSizeOfData() const;
  size_t sizeOfData(mozilla::MallocSizeOf mallocSizeOf) const;
  size_t sizeOfTypeScript(mozilla::MallocSizeOf mallocSizeOf) const;

  size_t dataSize() const { return dataSize_; }

  bool hasConsts() const { return data_->hasConsts(); }
  bool hasObjects() const { return data_->hasObjects(); }
  bool hasTrynotes() const { return data_->hasTryNotes(); }
  bool hasScopeNotes() const { return data_->hasScopeNotes(); }
  bool hasResumeOffsets() const { return data_->hasResumeOffsets(); }

  mozilla::Span<const js::GCPtrScope> scopes() const { return data_->scopes(); }

  mozilla::Span<const js::GCPtrValue> consts() const {
    MOZ_ASSERT(hasConsts());
    return data_->consts();
  }

  mozilla::Span<const js::GCPtrObject> objects() const {
    MOZ_ASSERT(hasObjects());
    return data_->objects();
  }

  mozilla::Span<const JSTryNote> trynotes() const {
    MOZ_ASSERT(hasTrynotes());
    return data_->tryNotes();
  }

  mozilla::Span<const js::ScopeNote> scopeNotes() const {
    MOZ_ASSERT(hasScopeNotes());
    return data_->scopeNotes();
  }

  mozilla::Span<const uint32_t> resumeOffsets() const {
    MOZ_ASSERT(hasResumeOffsets());
    return data_->resumeOffsets();
  }

  uint32_t tableSwitchCaseOffset(jsbytecode* pc, uint32_t caseIndex) const {
    MOZ_ASSERT(containsPC(pc));
    MOZ_ASSERT(*pc == JSOP_TABLESWITCH);
    uint32_t firstResumeIndex = GET_RESUMEINDEX(pc + 3 * JUMP_OFFSET_LEN);
    return resumeOffsets()[firstResumeIndex + caseIndex];
  }
  jsbytecode* tableSwitchCasePC(jsbytecode* pc, uint32_t caseIndex) const {
    return offsetToPC(tableSwitchCaseOffset(pc, caseIndex));
  }

  bool hasLoops();

  uint32_t numNotes() const {
    MOZ_ASSERT(scriptData_);
    return scriptData_->noteLength();
  }
  jssrcnote* notes() const {
    MOZ_ASSERT(scriptData_);
    return scriptData_->notes();
  }

  size_t natoms() const {
    MOZ_ASSERT(scriptData_);
    return scriptData_->natoms();
  }
  js::GCPtrAtom* atoms() const {
    MOZ_ASSERT(scriptData_);
    return scriptData_->atoms();
  }

  js::GCPtrAtom& getAtom(size_t index) const {
    MOZ_ASSERT(index < natoms());
    return atoms()[index];
  }

  js::GCPtrAtom& getAtom(jsbytecode* pc) const {
    MOZ_ASSERT(containsPC(pc) && containsPC(pc + sizeof(uint32_t)));
    MOZ_ASSERT(js::JOF_OPTYPE((JSOp)*pc) == JOF_ATOM);
    return getAtom(GET_UINT32_INDEX(pc));
  }

  js::PropertyName* getName(size_t index) {
    return getAtom(index)->asPropertyName();
  }

  js::PropertyName* getName(jsbytecode* pc) const {
    return getAtom(pc)->asPropertyName();
  }

  JSObject* getObject(size_t index) {
    MOZ_ASSERT(objects()[index]->isTenured());
    return objects()[index];
  }

  JSObject* getObject(jsbytecode* pc) {
    MOZ_ASSERT(containsPC(pc) && containsPC(pc + sizeof(uint32_t)));
    return getObject(GET_UINT32_INDEX(pc));
  }

  js::Scope* getScope(size_t index) const { return scopes()[index]; }

  js::Scope* getScope(jsbytecode* pc) const {
    // This method is used to get a scope directly using a JSOp with an
    // index. To search through ScopeNotes to look for a Scope using pc,
    // use lookupScope.
    MOZ_ASSERT(containsPC(pc) && containsPC(pc + sizeof(uint32_t)));
    MOZ_ASSERT(js::JOF_OPTYPE(JSOp(*pc)) == JOF_SCOPE,
               "Did you mean to use lookupScope(pc)?");
    return getScope(GET_UINT32_INDEX(pc));
  }

  inline JSFunction* getFunction(size_t index);
  inline JSFunction* getFunction(jsbytecode* pc);

  JSFunction* function() const {
    if (functionNonDelazifying()) {
      return functionNonDelazifying();
    }
    return nullptr;
  }

  inline js::RegExpObject* getRegExp(size_t index);
  inline js::RegExpObject* getRegExp(jsbytecode* pc);

  const js::Value& getConst(size_t index) { return consts()[index]; }

  // The following 3 functions find the static scope just before the
  // execution of the instruction pointed to by pc.

  js::Scope* lookupScope(jsbytecode* pc);

  js::Scope* innermostScope(jsbytecode* pc);
  js::Scope* innermostScope() { return innermostScope(main()); }

  /*
   * The isEmpty method tells whether this script has code that computes any
   * result (not return value, result AKA normal completion value) other than
   * JSVAL_VOID, or any other effects.
   */
  bool isEmpty() const {
    if (length() > 3) {
      return false;
    }

    jsbytecode* pc = code();
    if (noScriptRval() && JSOp(*pc) == JSOP_FALSE) {
      ++pc;
    }
    return JSOp(*pc) == JSOP_RETRVAL;
  }

  bool formalIsAliased(unsigned argSlot);
  bool formalLivesInArgumentsObject(unsigned argSlot);

 private:
  /* Change this->stepMode to |newValue|. */
  void setNewStepMode(js::FreeOp* fop, uint32_t newValue);

  bool ensureHasDebugScript(JSContext* cx);
  js::DebugScript* debugScript();
  js::DebugScript* releaseDebugScript();
  void destroyDebugScript(js::FreeOp* fop);

  bool hasDebugScript() const { return hasFlag(MutableFlags::HasDebugScript); }

 public:
  bool hasBreakpointsAt(jsbytecode* pc);
  bool hasAnyBreakpointsOrStepMode() { return hasDebugScript(); }

  // See comment above 'debugMode' in Realm.h for explanation of
  // invariants of debuggee compartments, scripts, and frames.
  inline bool isDebuggee() const;

  js::BreakpointSite* getBreakpointSite(jsbytecode* pc) {
    return hasDebugScript() ? debugScript()->breakpoints[pcToOffset(pc)]
                            : nullptr;
  }

  js::BreakpointSite* getOrCreateBreakpointSite(JSContext* cx, jsbytecode* pc);

  void destroyBreakpointSite(js::FreeOp* fop, jsbytecode* pc);

  void clearBreakpointsIn(js::FreeOp* fop, js::Debugger* dbg,
                          JSObject* handler);

  /*
   * Increment or decrement the single-step count. If the count is non-zero
   * then the script is in single-step mode.
   *
   * Only incrementing is fallible, as it could allocate a DebugScript.
   */
  bool incrementStepModeCount(JSContext* cx);
  void decrementStepModeCount(js::FreeOp* fop);

  bool stepModeEnabled() {
    return hasDebugScript() && !!debugScript()->stepMode;
  }

#ifdef DEBUG
  uint32_t stepModeCount() {
    return hasDebugScript() ? debugScript()->stepMode : 0;
  }
#endif

  void finalize(js::FreeOp* fop);

  static const JS::TraceKind TraceKind = JS::TraceKind::Script;

  void traceChildren(JSTracer* trc);

  // A helper class to prevent relazification of the given function's script
  // while it's holding on to it.  This class automatically roots the script.
  class AutoDelazify;
  friend class AutoDelazify;

  class AutoDelazify {
    JS::RootedScript script_;
    JSContext* cx_;
    bool oldDoNotRelazify_;

   public:
    explicit AutoDelazify(JSContext* cx, JS::HandleFunction fun = nullptr)
        : script_(cx), cx_(cx), oldDoNotRelazify_(false) {
      holdScript(fun);
    }

    ~AutoDelazify() { dropScript(); }

    void operator=(JS::HandleFunction fun) {
      dropScript();
      holdScript(fun);
    }

    operator JS::HandleScript() const { return script_; }
    explicit operator bool() const { return script_; }

   private:
    void holdScript(JS::HandleFunction fun);
    void dropScript();
  };

  bool trackRecordReplayProgress() const {
    return hasFlag(MutableFlags::TrackRecordReplayProgress);
  }
};

/* If this fails, add/remove padding within JSScript. */
static_assert(
    sizeof(JSScript) % js::gc::CellAlignBytes == 0,
    "Size of JSScript must be an integral multiple of js::gc::CellAlignBytes");

namespace js {

// Variable-length data for LazyScripts. Contains vector of inner functions and
// vector of captured property ids.
class alignas(uintptr_t) LazyScriptData final {
 private:
  uint32_t numClosedOverBindings_ = 0;
  uint32_t numInnerFunctions_ = 0;

  FieldInitializers fieldInitializers_ = FieldInitializers::Invalid();

  // Size to allocate
  static size_t AllocationSize(uint32_t numClosedOverBindings,
                               uint32_t numInnerFunctions);

  // Translate an offset into a concrete pointer.
  template <typename T>
  T* offsetToPointer(size_t offset) {
    uintptr_t base = reinterpret_cast<uintptr_t>(this);
    return reinterpret_cast<T*>(base + offset);
  }

  template <typename T>
  void initElements(size_t offset, size_t length);

  LazyScriptData(uint32_t numClosedOverBindings, uint32_t numInnerFunctions);

 public:
  static LazyScriptData* new_(JSContext* cx, uint32_t numClosedOverBindings,
                              uint32_t numInnerFunctions);

  friend class LazyScript;

  mozilla::Span<GCPtrAtom> closedOverBindings();
  mozilla::Span<GCPtrFunction> innerFunctions();

  void trace(JSTracer* trc);

  // LazyScriptData has trailing data so isn't copyable or movable.
  LazyScriptData(const LazyScriptData&) = delete;
  LazyScriptData& operator=(const LazyScriptData&) = delete;
};

// Information about a script which may be (or has been) lazily compiled to
// bytecode from its source.
class LazyScript : public gc::TenuredCell {
  // If non-nullptr, the script has been compiled and this is a forwarding
  // pointer to the result. This is a weak pointer: after relazification, we
  // can collect the script if there are no other pointers to it.
  WeakHeapPtrScript script_;

  // Original function with which the lazy script is associated.
  GCPtrFunction function_;

  // This field holds one of:
  //   * LazyScript in which the script is nested.  This case happens if the
  //     enclosing script is lazily parsed and have never been compiled.
  //
  //     This is used by the debugger to delazify the enclosing scripts
  //     recursively.  The all ancestor LazyScripts in this linked-list are
  //     kept alive as long as this LazyScript is alive, which doesn't result
  //     in keeping them unnecessarily alive outside of the debugger for the
  //     following reasons:
  //
  //       * Outside of the debugger, a LazyScript is visible to user (which
  //         means the LazyScript can be pointed from somewhere else than the
  //         enclosing script) only if the enclosing script is compiled and
  //         executed.  While compiling the enclosing script, this field is
  //         changed to point the enclosing scope.  So the enclosing
  //         LazyScript is no more in the list.
  //       * Before the enclosing script gets compiled, this LazyScript is
  //         kept alive only if the outermost LazyScript in the list is kept
  //         alive.
  //       * Once this field is changed to point the enclosing scope, this
  //         field will never point the enclosing LazyScript again, since
  //         relazification is not performed on non-leaf scripts.
  //
  //   * Scope in which the script is nested.  This case happens if the
  //     enclosing script has ever been compiled.
  //
  //   * nullptr for incomplete (initial or failure) state
  //
  // This field should be accessed via accessors:
  //   * enclosingScope
  //   * setEnclosingScope (cannot be called twice)
  //   * enclosingLazyScript
  //   * setEnclosingLazyScript (cannot be called twice)
  // after checking:
  //   * hasEnclosingLazyScript
  //   * hasEnclosingScope
  //
  // The transition of fields are following:
  //
  //  o                               o
  //  | when function is lazily       | when decoded from XDR,
  //  | parsed inside a function      | and enclosing script is lazy
  //  | which is lazily parsed        | (CreateForXDR without enclosingScope)
  //  | (Create)                      |
  //  v                               v
  // +---------+                     +---------+
  // | nullptr |                     | nullptr |
  // +---------+                     +---------+
  //  |                               |
  //  | when enclosing function is    | when enclosing script is decoded
  //  | lazily parsed and this        | and this script's function is put
  //  | script's function is put      | into innerFunctions()
  //  | into innerFunctions()         | (setEnclosingLazyScript)
  //  | (setEnclosingLazyScript)      |
  //  |                               |
  //  |                               |     o
  //  |                               |     | when function is lazily
  //  |                               |     | parsed inside a function
  //  |                               |     | which is eagerly parsed
  //  |                               |     | (Create)
  //  v                               |     v
  // +----------------------+         |    +---------+
  // | enclosing LazyScript |<--------+    | nullptr |
  // +----------------------+              +---------+
  //  |                                     |
  //  v                                     |
  //  +<------------------------------------+
  //  |
  //  | when the enclosing script     o
  //  | is successfully compiled      | when decoded from XDR,
  //  | (setEnclosingScope)           | and enclosing script is not lazy
  //  v                               | (CreateForXDR with enclosingScope)
  // +-----------------+              |
  // | enclosing Scope |<-------------+
  // +-----------------+
  GCPtr<TenuredCell*> enclosingLazyScriptOrScope_;

  // ScriptSourceObject. We leave this set to nullptr until we generate
  // bytecode for our immediate parent.
  GCPtr<ScriptSourceObject*> sourceObject_;

  // Heap allocated table with any free variables, inner functions, or class
  // fields. This will be nullptr if none exist.
  LazyScriptData* lazyData_;

  static const uint32_t NumClosedOverBindingsBits = 20;
  static const uint32_t NumInnerFunctionsBits = 20;

  // See: JSScript::ImmutableFlags / MutableFlags.
  // NOTE: Lazy script only defines and uses a subset of these flags.
  using ImmutableFlags = JSScript::ImmutableFlags;
  using MutableFlags = JSScript::MutableFlags;

  uint32_t immutableFlags_;
  uint32_t mutableFlags_;

  MOZ_MUST_USE bool hasFlag(MutableFlags flag) const {
    return mutableFlags_ & uint32_t(flag);
  }
  void setFlag(MutableFlags flag) { mutableFlags_ |= uint32_t(flag); }

  MOZ_MUST_USE bool hasFlag(ImmutableFlags flag) const {
    return immutableFlags_ & uint32_t(flag);
  }
  void setFlag(ImmutableFlags flag) { immutableFlags_ |= uint32_t(flag); }

  // Source location for the script.
  // See the comment in JSScript for the details
  uint32_t sourceStart_;
  uint32_t sourceEnd_;
  uint32_t toStringStart_;
  uint32_t toStringEnd_;
  // Line and column of |begin_| position, that is the position where we
  // start parsing.
  uint32_t lineno_;
  uint32_t column_;

  LazyScript(JSFunction* fun, ScriptSourceObject& sourceObject,
             LazyScriptData* data, uint32_t immutableFlags,
             uint32_t sourceStart, uint32_t sourceEnd, uint32_t toStringStart,
             uint32_t lineno, uint32_t column);

  // Create a LazyScript without initializing the closedOverBindings and the
  // innerFunctions. To be GC-safe, the caller must initialize both vectors
  // with valid atoms and functions.
  static LazyScript* CreateRaw(JSContext* cx, uint32_t numClosedOverBindings,
                               uint32_t numInnerFunctions, HandleFunction fun,
                               HandleScriptSourceObject sourceObject,
                               uint32_t immutableFlags, uint32_t sourceStart,
                               uint32_t sourceEnd, uint32_t toStringStart,
                               uint32_t lineno, uint32_t column);

 public:
  static const uint32_t NumClosedOverBindingsLimit =
      1 << NumClosedOverBindingsBits;
  static const uint32_t NumInnerFunctionsLimit = 1 << NumInnerFunctionsBits;

  // Create a LazyScript and initialize closedOverBindings and innerFunctions
  // with the provided vectors.
  static LazyScript* Create(JSContext* cx, HandleFunction fun,
                            HandleScriptSourceObject sourceObject,
                            const frontend::AtomVector& closedOverBindings,
                            Handle<GCVector<JSFunction*, 8>> innerFunctions,
                            uint32_t sourceStart, uint32_t sourceEnd,
                            uint32_t toStringStart, uint32_t lineno,
                            uint32_t column, frontend::ParseGoal parseGoal);

  // Create a LazyScript and initialize the closedOverBindings and the
  // innerFunctions with dummy values to be replaced in a later initialization
  // phase.
  //
  // The "script" argument to this function can be null.  If it's non-null,
  // then this LazyScript should be associated with the given JSScript.
  //
  // The sourceObject and enclosingScope arguments may be null if the
  // enclosing function is also lazy.
  static LazyScript* CreateForXDR(
      JSContext* cx, uint32_t numClosedOverBindings, uint32_t numInnerFunctions,
      HandleFunction fun, HandleScript script, HandleScope enclosingScope,
      HandleScriptSourceObject sourceObject, uint32_t immutableFlags,
      uint32_t sourceStart, uint32_t sourceEnd, uint32_t toStringStart,
      uint32_t toStringEnd, uint32_t lineno, uint32_t column);

  static inline JSFunction* functionDelazifying(JSContext* cx,
                                                Handle<LazyScript*>);
  JSFunction* functionNonDelazifying() const { return function_; }

  JS::Compartment* compartment() const;
  JS::Compartment* maybeCompartment() const { return compartment(); }
  Realm* realm() const;

  void initScript(JSScript* script);

  JSScript* maybeScript() { return script_; }
  const JSScript* maybeScriptUnbarriered() const {
    return script_.unbarrieredGet();
  }
  bool hasScript() const { return bool(script_); }

  bool hasEnclosingScope() const {
    return enclosingLazyScriptOrScope_ &&
           enclosingLazyScriptOrScope_->is<Scope>();
  }
  bool hasEnclosingLazyScript() const {
    return enclosingLazyScriptOrScope_ &&
           enclosingLazyScriptOrScope_->is<LazyScript>();
  }

  LazyScript* enclosingLazyScript() const {
    MOZ_ASSERT(hasEnclosingLazyScript());
    return enclosingLazyScriptOrScope_->as<LazyScript>();
  }
  void setEnclosingLazyScript(LazyScript* enclosingLazyScript);

  Scope* enclosingScope() const {
    MOZ_ASSERT(hasEnclosingScope());
    return enclosingLazyScriptOrScope_->as<Scope>();
  }
  void setEnclosingScope(Scope* enclosingScope);

  bool hasNonSyntacticScope() const {
    return enclosingScope()->hasOnChain(ScopeKind::NonSyntactic);
  }

  ScriptSourceObject& sourceObject() const;
  ScriptSource* scriptSource() const { return sourceObject().source(); }
  ScriptSource* maybeForwardedScriptSource() const;
  bool mutedErrors() const { return scriptSource()->mutedErrors(); }

  mozilla::Span<GCPtrAtom> closedOverBindings() {
    return lazyData_ ? lazyData_->closedOverBindings()
                     : mozilla::Span<GCPtrAtom>();
  }
  uint32_t numClosedOverBindings() const {
    return lazyData_ ? lazyData_->closedOverBindings().size() : 0;
  };

  mozilla::Span<GCPtrFunction> innerFunctions() {
    return lazyData_ ? lazyData_->innerFunctions()
                     : mozilla::Span<GCPtrFunction>();
  }
  uint32_t numInnerFunctions() const {
    return lazyData_ ? lazyData_->innerFunctions().size() : 0;
  }

  GeneratorKind generatorKind() const {
    return hasFlag(ImmutableFlags::IsGenerator) ? GeneratorKind::Generator
                                                : GeneratorKind::NotGenerator;
  }

  bool isGenerator() const {
    return generatorKind() == GeneratorKind::Generator;
  }

  void setGeneratorKind(GeneratorKind kind) {
    // A script only gets its generator kind set as part of initialization,
    // so it can only transition from NotGenerator.
    MOZ_ASSERT(!isGenerator());
    if (kind == GeneratorKind::Generator) {
      setFlag(ImmutableFlags::IsGenerator);
    }
  }

  bool isAsync() const { return hasFlag(ImmutableFlags::IsAsync); }
  FunctionAsyncKind asyncKind() const {
    return isAsync() ? FunctionAsyncKind::AsyncFunction
                     : FunctionAsyncKind::SyncFunction;
  }

  void setAsyncKind(FunctionAsyncKind kind) {
    if (kind == FunctionAsyncKind::AsyncFunction) {
      setFlag(ImmutableFlags::IsAsync);
    }
  }

  bool hasRest() const { return hasFlag(ImmutableFlags::HasRest); }
  void setHasRest() { setFlag(ImmutableFlags::HasRest); }

  frontend::ParseGoal parseGoal() const {
    if (hasFlag(ImmutableFlags::IsModule)) {
      return frontend::ParseGoal::Module;
    }
    return frontend::ParseGoal::Script;
  }

  bool isBinAST() const { return hasFlag(ImmutableFlags::IsBinAST); }
  void setIsBinAST() { setFlag(ImmutableFlags::IsBinAST); }

  bool strict() const { return hasFlag(ImmutableFlags::Strict); }
  void setStrict() { setFlag(ImmutableFlags::Strict); }

  bool bindingsAccessedDynamically() const {
    return hasFlag(ImmutableFlags::BindingsAccessedDynamically);
  }
  void setBindingsAccessedDynamically() {
    setFlag(ImmutableFlags::BindingsAccessedDynamically);
  }

  bool hasDebuggerStatement() const {
    return hasFlag(ImmutableFlags::HasDebuggerStatement);
  }
  void setHasDebuggerStatement() {
    setFlag(ImmutableFlags::HasDebuggerStatement);
  }

  bool hasDirectEval() const { return hasFlag(ImmutableFlags::HasDirectEval); }
  void setHasDirectEval() { setFlag(ImmutableFlags::HasDirectEval); }

  bool isLikelyConstructorWrapper() const {
    return hasFlag(ImmutableFlags::IsLikelyConstructorWrapper);
  }
  void setLikelyConstructorWrapper() {
    setFlag(ImmutableFlags::IsLikelyConstructorWrapper);
  }

  bool hasBeenCloned() const { return hasFlag(MutableFlags::HasBeenCloned); }
  void setHasBeenCloned() { setFlag(MutableFlags::HasBeenCloned); }

  bool treatAsRunOnce() const {
    return hasFlag(ImmutableFlags::TreatAsRunOnce);
  }
  void setTreatAsRunOnce() { setFlag(ImmutableFlags::TreatAsRunOnce); }

  bool isDerivedClassConstructor() const {
    return hasFlag(ImmutableFlags::IsDerivedClassConstructor);
  }
  void setIsDerivedClassConstructor() {
    setFlag(ImmutableFlags::IsDerivedClassConstructor);
  }

  bool needsHomeObject() const {
    return hasFlag(ImmutableFlags::NeedsHomeObject);
  }
  void setNeedsHomeObject() { setFlag(ImmutableFlags::NeedsHomeObject); }

  bool shouldDeclareArguments() const {
    return hasFlag(ImmutableFlags::ShouldDeclareArguments);
  }
  void setShouldDeclareArguments() {
    setFlag(ImmutableFlags::ShouldDeclareArguments);
  }

  bool hasThisBinding() const {
    return hasFlag(ImmutableFlags::FunctionHasThisBinding);
  }
  void setHasThisBinding() { setFlag(ImmutableFlags::FunctionHasThisBinding); }

  void setFieldInitializers(FieldInitializers fieldInitializers) {
    MOZ_ASSERT(lazyData_);
    lazyData_->fieldInitializers_ = fieldInitializers;
  }

  const FieldInitializers& getFieldInitializers() const {
    MOZ_ASSERT(lazyData_);
    return lazyData_->fieldInitializers_;
  }

  const char* filename() const { return scriptSource()->filename(); }
  uint32_t sourceStart() const { return sourceStart_; }
  uint32_t sourceEnd() const { return sourceEnd_; }
  uint32_t sourceLength() const { return sourceEnd_ - sourceStart_; }
  uint32_t toStringStart() const { return toStringStart_; }
  uint32_t toStringEnd() const { return toStringEnd_; }
  uint32_t lineno() const { return lineno_; }
  uint32_t column() const { return column_; }

  void setToStringEnd(uint32_t toStringEnd) {
    MOZ_ASSERT(toStringStart_ <= toStringEnd);
    MOZ_ASSERT(toStringEnd_ >= sourceEnd_);
    toStringEnd_ = toStringEnd;
  }

  // Returns true if the enclosing script has ever been compiled.
  // Once the enclosing script is compiled, the scope chain is created.
  // This LazyScript is delazify-able as long as it has the enclosing scope,
  // even if the enclosing JSScript is GCed.
  // The enclosing JSScript can be GCed later if the enclosing scope is not
  // FunctionScope or ModuleScope.
  bool enclosingScriptHasEverBeenCompiled() const {
    return hasEnclosingScope();
  }

  friend class GCMarker;
  void traceChildren(JSTracer* trc);
  void finalize(js::FreeOp* fop);

  static const JS::TraceKind TraceKind = JS::TraceKind::LazyScript;

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) {
    return mallocSizeOf(lazyData_);
  }

  uint32_t immutableFlags() const { return immutableFlags_; }
};

/* If this fails, add/remove padding within LazyScript. */
static_assert(sizeof(LazyScript) % js::gc::CellAlignBytes == 0,
              "Size of LazyScript must be an integral multiple of "
              "js::gc::CellAlignBytes");

struct ScriptAndCounts {
  /* This structure is stored and marked from the JSRuntime. */
  JSScript* script;
  ScriptCounts scriptCounts;

  inline explicit ScriptAndCounts(JSScript* script);
  inline ScriptAndCounts(ScriptAndCounts&& sac);

  const PCCounts* maybeGetPCCounts(jsbytecode* pc) const {
    return scriptCounts.maybeGetPCCounts(script->pcToOffset(pc));
  }
  const PCCounts* maybeGetThrowCounts(jsbytecode* pc) const {
    return scriptCounts.maybeGetThrowCounts(script->pcToOffset(pc));
  }

  jit::IonScriptCounts* getIonCounts() const { return scriptCounts.ionCounts_; }

  void trace(JSTracer* trc) {
    TraceRoot(trc, &script, "ScriptAndCounts::script");
  }
};

extern char* FormatIntroducedFilename(JSContext* cx, const char* filename,
                                      unsigned lineno, const char* introducer);

struct GSNCache;

jssrcnote* GetSrcNote(GSNCache& cache, JSScript* script, jsbytecode* pc);

extern jssrcnote* GetSrcNote(JSContext* cx, JSScript* script, jsbytecode* pc);

extern jsbytecode* LineNumberToPC(JSScript* script, unsigned lineno);

extern JS_FRIEND_API unsigned GetScriptLineExtent(JSScript* script);

} /* namespace js */

namespace js {

extern unsigned PCToLineNumber(JSScript* script, jsbytecode* pc,
                               unsigned* columnp = nullptr);

extern unsigned PCToLineNumber(unsigned startLine, jssrcnote* notes,
                               jsbytecode* code, jsbytecode* pc,
                               unsigned* columnp = nullptr);

/*
 * This function returns the file and line number of the script currently
 * executing on cx. If there is no current script executing on cx (e.g., a
 * native called directly through JSAPI (e.g., by setTimeout)), nullptr and 0
 * are returned as the file and line.
 */
extern void DescribeScriptedCallerForCompilation(
    JSContext* cx, MutableHandleScript maybeScript, const char** file,
    unsigned* linenop, uint32_t* pcOffset, bool* mutedErrors);

/*
 * Like DescribeScriptedCallerForCompilation, but this function avoids looking
 * up the script/pc and the full linear scan to compute line number.
 */
extern void DescribeScriptedCallerForDirectEval(
    JSContext* cx, HandleScript script, jsbytecode* pc, const char** file,
    unsigned* linenop, uint32_t* pcOffset, bool* mutedErrors);

JSScript* CloneScriptIntoFunction(JSContext* cx, HandleScope enclosingScope,
                                  HandleFunction fun, HandleScript src,
                                  Handle<ScriptSourceObject*> sourceObject);

JSScript* CloneGlobalScript(JSContext* cx, ScopeKind scopeKind,
                            HandleScript src);

} /* namespace js */

// JS::ubi::Nodes can point to js::LazyScripts; they're js::gc::Cell instances
// with no associated compartment.
namespace JS {
namespace ubi {
template <>
class Concrete<js::LazyScript> : TracerConcrete<js::LazyScript> {
 protected:
  explicit Concrete(js::LazyScript* ptr)
      : TracerConcrete<js::LazyScript>(ptr) {}

 public:
  static void construct(void* storage, js::LazyScript* ptr) {
    new (storage) Concrete(ptr);
  }

  CoarseType coarseType() const final { return CoarseType::Script; }
  Size size(mozilla::MallocSizeOf mallocSizeOf) const override;
  const char* scriptFilename() const final;

  const char16_t* typeName() const override { return concreteTypeName; }
  static const char16_t concreteTypeName[];
};
}  // namespace ubi
}  // namespace JS

#endif /* vm_JSScript_h */
