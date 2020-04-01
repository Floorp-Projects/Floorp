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
#include "mozilla/Tuple.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Utf8.h"
#include "mozilla/Variant.h"

#include <type_traits>  // std::is_same
#include <utility>      // std::move

#include "jstypes.h"

#include "frontend/BinASTRuntimeSupport.h"
#include "frontend/NameAnalysisTypes.h"
#include "frontend/SourceNotes.h"  // SrcNote
#include "gc/Barrier.h"
#include "gc/Rooting.h"
#include "jit/IonCode.h"
#include "js/CompileOptions.h"
#include "js/UbiNode.h"
#include "js/UniquePtr.h"
#include "js/Utility.h"
#include "util/StructuredSpewer.h"
#include "vm/BigIntType.h"
#include "vm/BytecodeIterator.h"
#include "vm/BytecodeLocation.h"
#include "vm/BytecodeUtil.h"
#include "vm/JSAtom.h"
#include "vm/NativeObject.h"
#include "vm/Scope.h"
#include "vm/Shape.h"
#include "vm/SharedImmutableStringsCache.h"
#include "vm/SharedStencil.h"
#include "vm/Time.h"

namespace JS {
struct ScriptSourceInfo;
template <typename UnitT>
class SourceText;
}  // namespace JS

namespace js {

namespace coverage {
class LCovSource;
}  // namespace coverage

namespace jit {
class AutoKeepJitScripts;
struct BaselineScript;
struct IonScriptCounts;
class JitScript;
}  // namespace jit

class AutoSweepJitScript;
class ModuleObject;
class RegExpObject;
class ScriptSourceHolder;
class SourceCompressionTask;
class Shape;
class DebugAPI;
class DebugScript;

namespace frontend {
struct CompilationInfo;
class FunctionIndex;
class FunctionBox;
class ModuleSharedContext;
class ScriptStencil;
}  // namespace frontend

namespace detail {

// Do not call this directly! It is exposed for the friend declarations in
// this file.
JSScript* CopyScript(JSContext* cx, HandleScript src,
                     HandleObject functionOrGlobal,
                     HandleScriptSourceObject sourceObject,
                     MutableHandle<GCVector<Scope*>> scopes);

}  // namespace detail

}  // namespace js

namespace js {

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

// The key of these side-table hash maps are intentionally not traced GC
// references to JSScript. Instead, we use bare pointers and manually fix up
// when objects could have moved (see Zone::fixupScriptMapsAfterMovingGC) and
// remove when the realm is destroyed (see Zone::clearScriptCounts and
// Zone::clearScriptNames). They essentially behave as weak references, except
// that the references are not cleared early by the GC. They must be non-strong
// references because the tables are kept at the Zone level and otherwise the
// table keys would keep scripts alive, thus keeping Realms alive, beyond their
// expected lifetimes. However, We do not use actual weak references (e.g. as
// used by WeakMap tables provided in gc/WeakMap.h) because they would be
// collected before the calls to the JSScript::finalize function which are used
// to aggregate code coverage results on the realm.
//
// Note carefully, however, that there is an exceptional case for which we *do*
// want the JSScripts to be strong references (and thus traced): when the
// --dump-bytecode command line option or the PCCount JSFriend API is used,
// then the scripts for all counts must remain alive. See
// Zone::traceScriptTableRoots() for more details.
//
// TODO: Clean this up by either aggregating coverage results in some other
// way, or by tweaking sweep ordering.
using UniqueScriptCounts = js::UniquePtr<ScriptCounts>;
using ScriptCountsMap = HashMap<BaseScript*, UniqueScriptCounts,
                                DefaultHasher<BaseScript*>, SystemAllocPolicy>;

// The 'const char*' for the function name is a pointer within the LCovSource's
// LifoAlloc and will be discarded at the same time.
using ScriptLCovEntry = mozilla::Tuple<coverage::LCovSource*, const char*>;
using ScriptLCovMap = HashMap<BaseScript*, ScriptLCovEntry,
                              DefaultHasher<BaseScript*>, SystemAllocPolicy>;

#ifdef MOZ_VTUNE
using ScriptVTuneIdMap = HashMap<BaseScript*, uint32_t,
                                 DefaultHasher<BaseScript*>, SystemAllocPolicy>;
#endif

using UniqueDebugScript = js::UniquePtr<DebugScript, JS::FreePolicy>;
using DebugScriptMap = HashMap<BaseScript*, UniqueDebugScript,
                               DefaultHasher<BaseScript*>, SystemAllocPolicy>;

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

// Synchronously compress the source of |script|, for testing purposes.
extern MOZ_MUST_USE bool SynchronouslyCompressSource(
    JSContext* cx, JS::Handle<JSScript*> script);

// Retrievable source can be retrieved using the source hook (and therefore
// need not be XDR'd, can be discarded if desired because it can always be
// reconstituted later, etc.).
enum class SourceRetrievable { Yes, No };

// [SMDOC] ScriptSource
//
// This class abstracts over the source we used to compile from. The current
// representation may transition to different modes in order to save memory.
// Abstractly the source may be one of UTF-8, UTF-16, or BinAST. The data
// itself may be unavailable, retrieveable-using-source-hook, compressed, or
// uncompressed. If source is retrieved or decompressed for use, we may update
// the ScriptSource to hold the result.
class ScriptSource {
  // NOTE: While ScriptSources may be compressed off thread, they are only
  // modified by the main thread, and all members are always safe to access
  // on the main thread.

  friend class SourceCompressionTask;
  friend bool SynchronouslyCompressSource(JSContext* cx,
                                          JS::Handle<JSScript*> script);

 private:
  // Common base class of the templated variants of PinnedUnits<T>.
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
  // Missing source text that isn't retrievable using the source hook.  (All
  // ScriptSources initially begin in this state.  Users that are compiling
  // source text will overwrite |data| to store a different state.)
  struct Missing {};

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

  // Uncompressed source text. Templates distinguish if we are interconvertable
  // to |Retrievable| or not.
  template <typename Unit>
  class UncompressedData {
    typename SourceTypeTraits<Unit>::SharedImmutableString string_;

   public:
    explicit UncompressedData(
        typename SourceTypeTraits<Unit>::SharedImmutableString str)
        : string_(std::move(str)) {}

    const Unit* units() const { return SourceTypeTraits<Unit>::units(string_); }

    size_t length() const { return string_.length(); }
  };

  template <typename Unit, SourceRetrievable CanRetrieve>
  class Uncompressed : public UncompressedData<Unit> {
    using Base = UncompressedData<Unit>;

   public:
    using Base::Base;
  };

  // Compressed source text. Templates distinguish if we are interconvertable
  // to |Retrievable| or not.
  template <typename Unit>
  struct CompressedData {
    // Single-byte compressed text, regardless whether the original text
    // was single-byte or two-byte.
    SharedImmutableString raw;
    size_t uncompressedLength;

    CompressedData(SharedImmutableString raw, size_t uncompressedLength)
        : raw(std::move(raw)), uncompressedLength(uncompressedLength) {}
  };

  template <typename Unit, SourceRetrievable CanRetrieve>
  struct Compressed : public CompressedData<Unit> {
    using Base = CompressedData<Unit>;

   public:
    using Base::Base;
  };

  // BinAST source.
  struct BinAST {
    SharedImmutableString string;
    UniquePtr<frontend::BinASTSourceMetadata> metadata;

    BinAST(SharedImmutableString&& str,
           UniquePtr<frontend::BinASTSourceMetadata> metadata)
        : string(std::move(str)), metadata(std::move(metadata)) {}
  };

  // The set of currently allowed encoding modes.
  using SourceType =
      mozilla::Variant<Compressed<mozilla::Utf8Unit, SourceRetrievable::Yes>,
                       Uncompressed<mozilla::Utf8Unit, SourceRetrievable::Yes>,
                       Compressed<mozilla::Utf8Unit, SourceRetrievable::No>,
                       Uncompressed<mozilla::Utf8Unit, SourceRetrievable::No>,
                       Compressed<char16_t, SourceRetrievable::Yes>,
                       Uncompressed<char16_t, SourceRetrievable::Yes>,
                       Compressed<char16_t, SourceRetrievable::No>,
                       Uncompressed<char16_t, SourceRetrievable::No>,
                       Retrievable<mozilla::Utf8Unit>, Retrievable<char16_t>,
                       Missing, BinAST>;

  //
  // Start of fields.
  //

  mozilla::Atomic<uint32_t, mozilla::ReleaseAcquire> refs = {};

  // An id for this source that is unique across the process. This can be used
  // to refer to this source from places that don't want to hold a strong
  // reference on the source itself.
  //
  // This is a 32 bit ID and could overflow, in which case the ID will not be
  // unique anymore.
  uint32_t id_ = 0;

  // Source data (as a mozilla::Variant).
  SourceType data = SourceType(Missing());

  // If the GC calls triggerConvertToCompressedSource with PinnedUnits present,
  // the first PinnedUnits (that is, bottom of the stack) will install the
  // compressed chars upon destruction.
  //
  // Retrievability isn't part of the type here because uncompressed->compressed
  // transitions must preserve existing retrievability.
  PinnedUnitsBase* pinnedUnitsStack_ = nullptr;
  mozilla::MaybeOneOf<CompressedData<mozilla::Utf8Unit>,
                      CompressedData<char16_t>>
      pendingCompressed_;

  // The filename of this script.
  mozilla::Maybe<SharedImmutableString> filename_;

  // If this ScriptSource was generated by a code-introduction mechanism such
  // as |eval| or |new Function|, the debugger needs access to the "raw"
  // filename of the top-level script that contains the eval-ing code.  To
  // keep track of this, we must preserve the original outermost filename (of
  // the original introducer script), so that instead of a filename of
  // "foo.js line 30 > eval line 10 > Function", we can obtain the original
  // raw filename of "foo.js".
  //
  // In the case described above, this field will be set to to the original raw
  // filename from above, otherwise it will be mozilla::Nothing.
  mozilla::Maybe<SharedImmutableString> introducerFilename_;

  mozilla::Maybe<SharedImmutableTwoByteString> displayURL_;
  mozilla::Maybe<SharedImmutableTwoByteString> sourceMapURL_;

  // The bytecode cache encoder is used to encode only the content of function
  // which are delazified.  If this value is not nullptr, then each delazified
  // function should be recorded before their first execution.
  // This value is logically owned by the canonical ScriptSourceObject, and
  // will be released in the canonical SSO's finalizer.
  UniquePtr<XDRIncrementalEncoder> xdrEncoder_ = nullptr;

  // Instant at which the first parse of this source ended, or null
  // if the source hasn't been parsed yet.
  //
  // Used for statistics purposes, to determine how much time code spends
  // syntax parsed before being full parsed, to help determine whether
  // our syntax parse vs. full parse heuristics are correct.
  mozilla::TimeStamp parseEnded_;

  // A string indicating how this source code was introduced into the system.
  // This is a constant, statically allocated C string, so does not need memory
  // management.
  const char* introductionType_ = nullptr;

  // Bytecode offset in caller script that generated this code.  This is
  // present for eval-ed code, as well as "new Function(...)"-introduced
  // scripts.
  mozilla::Maybe<uint32_t> introductionOffset_;

  // If this source is for Function constructor, the position of ")" after
  // parameter list in the source.  This is used to get function body.
  // 0 for other cases.
  uint32_t parameterListEnd_ = 0;

  // Line number within the file where this source starts.
  uint32_t startLine_ = 0;

  // See: CompileOptions::mutedErrors.
  bool mutedErrors_ = false;

  // Set to true if parser saw  asmjs directives.
  bool containsAsmJS_ = false;

  //
  // End of fields.
  //

  // How many ids have been handed out to sources.
  static mozilla::Atomic<uint32_t, mozilla::SequentiallyConsistent> idCount_;

  template <typename Unit>
  const Unit* chunkUnits(JSContext* cx,
                         UncompressedSourceCache::AutoHoldEntry& holder,
                         size_t chunk);

  // Return a string containing the chars starting at |begin| and ending at
  // |begin + len|.
  //
  // Warning: this is *not* GC-safe! Any chars to be handed out must use
  // PinnedUnits. See comment below.
  template <typename Unit>
  const Unit* units(JSContext* cx, UncompressedSourceCache::AutoHoldEntry& asp,
                    size_t begin, size_t len);

 public:
  // When creating a JSString* from TwoByte source characters, we don't try to
  // to deflate to Latin1 for longer strings, because this can be slow.
  static const size_t SourceDeflateLimit = 100;

  explicit ScriptSource() : id_(++idCount_) {}

  void finalizeGCData();
  ~ScriptSource();

  void incref() { refs++; }
  void decref() {
    MOZ_ASSERT(refs != 0);
    if (--refs == 0) {
      js_delete(this);
    }
  }
  MOZ_MUST_USE bool initFromOptions(JSContext* cx,
                                    const JS::ReadOnlyCompileOptions& options);

  /**
   * The minimum script length (in code units) necessary for a script to be
   * eligible to be compressed.
   */
  static constexpr size_t MinimumCompressibleLength = 256;

  mozilla::Maybe<SharedImmutableString> getOrCreateStringZ(JSContext* cx,
                                                           UniqueChars&& str);
  mozilla::Maybe<SharedImmutableTwoByteString> getOrCreateStringZ(
      JSContext* cx, UniqueTwoByteChars&& str);

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

  bool hasSourceText() const {
    return hasUncompressedSource() || hasCompressedSource();
  }
  bool hasBinASTSource() const { return data.is<BinAST>(); }

  void setBinASTSourceMetadata(frontend::BinASTSourceMetadata* metadata) {
    MOZ_ASSERT(hasBinASTSource());
    data.as<BinAST>().metadata.reset(metadata);
  }
  frontend::BinASTSourceMetadata* binASTSourceMetadata() const {
    MOZ_ASSERT(hasBinASTSource());
    return data.as<BinAST>().metadata.get();
  }

 private:
  template <typename Unit>
  struct UncompressedDataMatcher {
    template <SourceRetrievable CanRetrieve>
    const UncompressedData<Unit>* operator()(
        const Uncompressed<Unit, CanRetrieve>& u) {
      return &u;
    }

    template <typename T>
    const UncompressedData<Unit>* operator()(const T&) {
      MOZ_CRASH(
          "attempting to access uncompressed data in a ScriptSource not "
          "containing it");
      return nullptr;
    }
  };

 public:
  template <typename Unit>
  const UncompressedData<Unit>* uncompressedData() {
    return data.match(UncompressedDataMatcher<Unit>());
  }

 private:
  template <typename Unit>
  struct CompressedDataMatcher {
    template <SourceRetrievable CanRetrieve>
    const CompressedData<Unit>* operator()(
        const Compressed<Unit, CanRetrieve>& c) {
      return &c;
    }

    template <typename T>
    const CompressedData<Unit>* operator()(const T&) {
      MOZ_CRASH(
          "attempting to access compressed data in a ScriptSource not "
          "containing it");
      return nullptr;
    }
  };

 public:
  template <typename Unit>
  const CompressedData<Unit>* compressedData() {
    return data.match(CompressedDataMatcher<Unit>());
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
    template <typename Unit, SourceRetrievable CanRetrieve>
    bool operator()(const Uncompressed<Unit, CanRetrieve>&) {
      return true;
    }

    template <typename Unit, SourceRetrievable CanRetrieve>
    bool operator()(const Compressed<Unit, CanRetrieve>&) {
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

 private:
  template <typename Unit>
  struct IsUncompressed {
    template <SourceRetrievable CanRetrieve>
    bool operator()(const Uncompressed<Unit, CanRetrieve>&) {
      return true;
    }

    template <typename T>
    bool operator()(const T&) {
      return false;
    }
  };

 public:
  template <typename Unit>
  bool isUncompressed() const {
    return data.match(IsUncompressed<Unit>());
  }

 private:
  struct HasCompressedSource {
    template <typename Unit, SourceRetrievable CanRetrieve>
    bool operator()(const Compressed<Unit, CanRetrieve>&) {
      return true;
    }

    template <typename T>
    bool operator()(const T&) {
      return false;
    }
  };

 public:
  bool hasCompressedSource() const { return data.match(HasCompressedSource()); }

 private:
  template <typename Unit>
  struct IsCompressed {
    template <SourceRetrievable CanRetrieve>
    bool operator()(const Compressed<Unit, CanRetrieve>&) {
      return true;
    }

    template <typename T>
    bool operator()(const T&) {
      return false;
    }
  };

 public:
  template <typename Unit>
  bool isCompressed() const {
    return data.match(IsCompressed<Unit>());
  }

 private:
  template <typename Unit>
  struct SourceTypeMatcher {
    template <template <typename C, SourceRetrievable R> class Data,
              SourceRetrievable CanRetrieve>
    bool operator()(const Data<Unit, CanRetrieve>&) {
      return true;
    }

    template <template <typename C, SourceRetrievable R> class Data,
              typename NotUnit, SourceRetrievable CanRetrieve>
    bool operator()(const Data<NotUnit, CanRetrieve>&) {
      return false;
    }

    bool operator()(const Retrievable<Unit>&) {
      MOZ_CRASH("source type only applies where actual text is available");
      return false;
    }

    template <typename NotUnit>
    bool operator()(const Retrievable<NotUnit>&) {
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
    template <typename Unit, SourceRetrievable CanRetrieve>
    size_t operator()(const Uncompressed<Unit, CanRetrieve>& u) {
      return u.length();
    }

    template <typename Unit, SourceRetrievable CanRetrieve>
    size_t operator()(const Compressed<Unit, CanRetrieve>& u) {
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

  JSLinearString* substring(JSContext* cx, size_t start, size_t stop);
  JSLinearString* substringDontDeflate(JSContext* cx, size_t start,
                                       size_t stop);

  MOZ_MUST_USE bool appendSubstring(JSContext* cx, js::StringBuffer& buf,
                                    size_t start, size_t stop);

  void setParameterListEnd(uint32_t parameterListEnd) {
    parameterListEnd_ = parameterListEnd;
  }

  bool isFunctionBody() { return parameterListEnd_ != 0; }
  JSLinearString* functionBodyString(JSContext* cx);

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
                                                size_t length,
                                                SourceRetrievable retrievable);

 public:
  // Initialize a fresh |ScriptSource| with unretrievable, uncompressed source.
  template <typename Unit>
  MOZ_MUST_USE bool initializeUnretrievableUncompressedSource(
      JSContext* cx, EntryUnits<Unit>&& source, size_t length);

  // Set the retrieved source for a |ScriptSource| whose source was recorded as
  // missing but retrievable.
  template <typename Unit>
  MOZ_MUST_USE bool setRetrievedSource(JSContext* cx, EntryUnits<Unit>&& source,
                                       size_t length);

  MOZ_MUST_USE bool tryCompressOffThread(JSContext* cx);

  // *Trigger* the conversion of this ScriptSource from containing uncompressed
  // |Unit|-encoded source to containing compressed source.  Conversion may not
  // be complete when this function returns: it'll be delayed if there's ongoing
  // use of the uncompressed source via |PinnedUnits|, in which case conversion
  // won't occur until the outermost |PinnedUnits| is destroyed.
  //
  // Compressed source is in bytes, no matter that |Unit| might be |char16_t|.
  // |sourceLength| is the length in code units (not bytes) of the uncompressed
  // source.
  template <typename Unit>
  void triggerConvertToCompressedSource(SharedImmutableString compressed,
                                        size_t sourceLength);

  // Initialize a fresh ScriptSource as containing unretrievable compressed
  // source of the indicated original encoding.
  template <typename Unit>
  MOZ_MUST_USE bool initializeWithUnretrievableCompressedSource(
      JSContext* cx, UniqueChars&& raw, size_t rawLength, size_t sourceLength);

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

  struct TriggerConvertToCompressedSourceFromTask {
    ScriptSource* const source_;
    SharedImmutableString& compressed_;

    TriggerConvertToCompressedSourceFromTask(ScriptSource* source,
                                             SharedImmutableString& compressed)
        : source_(source), compressed_(compressed) {}

    template <typename Unit, SourceRetrievable CanRetrieve>
    void operator()(const Uncompressed<Unit, CanRetrieve>&) {
      source_->triggerConvertToCompressedSource<Unit>(std::move(compressed_),
                                                      source_->length());
    }

    template <typename Unit, SourceRetrievable CanRetrieve>
    void operator()(const Compressed<Unit, CanRetrieve>&) {
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

  template <typename Unit>
  void convertToCompressedSource(SharedImmutableString compressed,
                                 size_t uncompressedLength);

  template <typename Unit>
  void performDelayedConvertToCompressedSource();

  void triggerConvertToCompressedSourceFromTask(
      SharedImmutableString compressed);

 private:
  // It'd be better to make this function take <XDRMode, Unit>, as both
  // specializations of this function contain nested Unit-parametrized
  // helper classes that do everything the function needs to do.  But then
  // we'd need template function partial specialization to hold XDRMode
  // constant while varying Unit, so that idea's no dice.
  template <XDRMode mode>
  MOZ_MUST_USE XDRResult xdrUnretrievableUncompressedSource(
      XDRState<mode>* xdr, uint8_t sourceCharSize, uint32_t uncompressedLength);

 public:
  const char* filename() const {
    return filename_ ? filename_.ref().chars() : nullptr;
  }
  MOZ_MUST_USE bool setFilename(JSContext* cx, const char* filename);
  MOZ_MUST_USE bool setFilename(JSContext* cx, UniqueChars&& filename);

  const char* introducerFilename() const {
    return introducerFilename_ ? introducerFilename_.ref().chars() : filename();
  }
  MOZ_MUST_USE bool setIntroducerFilename(JSContext* cx, const char* filename);
  MOZ_MUST_USE bool setIntroducerFilename(JSContext* cx,
                                          UniqueChars&& filename);

  bool hasIntroductionType() const { return introductionType_; }
  const char* introductionType() const {
    MOZ_ASSERT(hasIntroductionType());
    return introductionType_;
  }

  uint32_t id() const { return id_; }

  // Display URLs
  MOZ_MUST_USE bool setDisplayURL(JSContext* cx, const char16_t* url);
  MOZ_MUST_USE bool setDisplayURL(JSContext* cx, UniqueTwoByteChars&& url);
  bool hasDisplayURL() const { return displayURL_.isSome(); }
  const char16_t* displayURL() { return displayURL_.ref().chars(); }

  // Source maps
  MOZ_MUST_USE bool setSourceMapURL(JSContext* cx, const char16_t* url);
  MOZ_MUST_USE bool setSourceMapURL(JSContext* cx, UniqueTwoByteChars&& url);
  bool hasSourceMapURL() const { return sourceMapURL_.isSome(); }
  const char16_t* sourceMapURL() { return sourceMapURL_.ref().chars(); }

  bool mutedErrors() const { return mutedErrors_; }

  uint32_t startLine() const { return startLine_; }

  bool hasIntroductionOffset() const { return introductionOffset_.isSome(); }
  uint32_t introductionOffset() const { return introductionOffset_.value(); }
  void setIntroductionOffset(uint32_t offset) {
    MOZ_ASSERT(!hasIntroductionOffset());
    MOZ_ASSERT(offset <= (uint32_t)INT32_MAX);
    introductionOffset_.emplace(offset);
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
  template <typename Unit,
            template <typename U, SourceRetrievable CanRetrieve> class Data,
            XDRMode mode>
  static void codeRetrievable(ScriptSource* ss);

  template <typename Unit, XDRMode mode>
  static MOZ_MUST_USE XDRResult codeUncompressedData(XDRState<mode>* const xdr,
                                                     ScriptSource* const ss);

  template <typename Unit, XDRMode mode>
  static MOZ_MUST_USE XDRResult codeCompressedData(XDRState<mode>* const xdr,
                                                   ScriptSource* const ss);

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
  static const JSClassOps classOps_;

  static ScriptSourceObject* createInternal(JSContext* cx, ScriptSource* source,
                                            HandleObject canonical);

  bool isCanonical() const {
    return &getReservedSlot(CANONICAL_SLOT).toObject() == this;
  }
  ScriptSourceObject* unwrappedCanonical() const;

 public:
  static const JSClass class_;

  static void trace(JSTracer* trc, JSObject* obj);
  static void finalize(JSFreeOp* fop, JSObject* obj);

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
  BaseScript* unwrappedIntroductionScript() const {
    Value value =
        unwrappedCanonical()->getReservedSlot(INTRODUCTION_SCRIPT_SLOT);
    if (value.isUndefined()) {
      return nullptr;
    }
    return value.toGCThing()->as<BaseScript>();
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

// ScriptWarmUpData represents a pointer-sized field in BaseScript that stores
// one of the following using low-bit tags:
//
// * The enclosing BaseScript. This is only used while this script is lazy and
//   its containing script is also lazy. This outer script must be compiled
//   before the current script can in order to correctly build the scope chain.
//
// * The enclosing Scope. This is only used while this script is lazy and its
//   containing script is compiled. This is the outer scope chain that will be
//   used to compile this scipt.
//
// * The script's warm-up count. This is only used until the script has a
//   JitScript. The Baseline Interpreter and JITs use the warm-up count stored
//   in JitScript.
//
// * A pointer to the JitScript, when the script is warm enough for the Baseline
//   Interpreter.
//
class ScriptWarmUpData {
  uintptr_t data_ = ResetState();

 private:
  static constexpr uintptr_t NumTagBits = 2;
  static constexpr uint32_t MaxWarmUpCount = UINT32_MAX >> NumTagBits;

 public:
  // Public only for the JITs.
  static constexpr uintptr_t TagMask = (1 << NumTagBits) - 1;
  static constexpr uintptr_t JitScriptTag = 0;
  static constexpr uintptr_t EnclosingScriptTag = 1;
  static constexpr uintptr_t EnclosingScopeTag = 2;
  static constexpr uintptr_t WarmUpCountTag = 3;

 private:
  // A gc-safe value to clear to.
  constexpr uintptr_t ResetState() { return 0 | WarmUpCountTag; }

  template <uintptr_t Tag>
  inline void setTaggedPtr(void* ptr) {
    static_assert(Tag <= TagMask, "Tag must fit in TagMask");
    MOZ_ASSERT((uintptr_t(ptr) & TagMask) == 0);
    data_ = uintptr_t(ptr) | Tag;
  }

  template <typename T, uintptr_t Tag>
  inline T getTaggedPtr() const {
    static_assert(Tag <= TagMask, "Tag must fit in TagMask");
    MOZ_ASSERT((data_ & TagMask) == Tag);
    return reinterpret_cast<T>(data_ & ~TagMask);
  }

  void setWarmUpCount(uint32_t count) {
    if (count > MaxWarmUpCount) {
      count = MaxWarmUpCount;
    }
    data_ = (uintptr_t(count) << NumTagBits) | WarmUpCountTag;
  }

 public:
  void trace(JSTracer* trc);

  bool isEnclosingScript() const {
    return (data_ & TagMask) == EnclosingScriptTag;
  }
  bool isEnclosingScope() const {
    return (data_ & TagMask) == EnclosingScopeTag;
  }
  bool isWarmUpCount() const { return (data_ & TagMask) == WarmUpCountTag; }
  bool isJitScript() const { return (data_ & TagMask) == JitScriptTag; }

  // NOTE: To change type safely, 'clear' the old tagged value and then 'init'
  //       the new one. This will notify the GC appropriately.

  BaseScript* toEnclosingScript() const {
    return getTaggedPtr<BaseScript*, EnclosingScriptTag>();
  }
  inline void initEnclosingScript(BaseScript* enclosingScript);
  inline void clearEnclosingScript();

  Scope* toEnclosingScope() const {
    return getTaggedPtr<Scope*, EnclosingScopeTag>();
  }
  inline void initEnclosingScope(Scope* enclosingScope);
  inline void clearEnclosingScope();

  uint32_t toWarmUpCount() const {
    MOZ_ASSERT(isWarmUpCount());
    return data_ >> NumTagBits;
  }
  void resetWarmUpCount(uint32_t count) {
    MOZ_ASSERT(isWarmUpCount());
    setWarmUpCount(count);
  }
  void incWarmUpCount(uint32_t amount) {
    MOZ_ASSERT(isWarmUpCount());
    data_ += uintptr_t(amount) << NumTagBits;
  }

  jit::JitScript* toJitScript() const {
    return getTaggedPtr<jit::JitScript*, JitScriptTag>();
  }
  void initJitScript(jit::JitScript* jitScript) {
    MOZ_ASSERT(isWarmUpCount());
    setTaggedPtr<JitScriptTag>(jitScript);
  }
  void clearJitScript() {
    MOZ_ASSERT(isJitScript());
    data_ = ResetState();
  }
} JS_HAZ_GC_POINTER;

static_assert(sizeof(ScriptWarmUpData) == sizeof(uintptr_t),
              "JIT code depends on ScriptWarmUpData being pointer-sized");

struct FieldInitializers {
  static constexpr uint32_t MaxInitializers = INT32_MAX;

#ifdef DEBUG
  bool valid = false;
#endif

  // This struct will eventually have a vector of constant values for optimizing
  // field initializers.
  uint32_t numFieldInitializers = 0;

  explicit FieldInitializers(uint32_t numFieldInitializers)
      :
#ifdef DEBUG
        valid(true),
#endif
        numFieldInitializers(numFieldInitializers) {
  }

  static FieldInitializers Invalid() { return FieldInitializers(); }

 private:
  FieldInitializers() = default;
};

// [SMDOC] - JSScript data layout (unshared)
//
// PrivateScriptData stores variable-length data associated with a script.
// Abstractly a PrivateScriptData consists of all these arrays:
//
//   * A non-empty array of GCCellPtr in gcthings()
//
// Accessing this array just requires calling the appropriate public
// Span-computing function.
class alignas(uintptr_t) PrivateScriptData final {
  uint32_t ngcthings = 0;

  js::FieldInitializers fieldInitializers_ = js::FieldInitializers::Invalid();

  // Translate an offset into a concrete pointer.
  template <typename T>
  T* offsetToPointer(size_t offset) {
    uintptr_t base = reinterpret_cast<uintptr_t>(this);
    uintptr_t elem = base + offset;
    return reinterpret_cast<T*>(elem);
  }

  // Helpers for creating initializing trailing data
  template <typename T>
  void initElements(size_t offset, size_t length);

  // Size to allocate
  static size_t AllocationSize(uint32_t ngcthings);

  // Initialize header and PackedSpans
  explicit PrivateScriptData(uint32_t ngcthings);

 public:
  static constexpr size_t offsetOfGCThings() {
    return sizeof(PrivateScriptData);
  }

  // Accessors for typed array spans.
  mozilla::Span<JS::GCCellPtr> gcthings() {
    size_t offset = offsetOfGCThings();
    return mozilla::MakeSpan(offsetToPointer<JS::GCCellPtr>(offset), ngcthings);
  }

  void setFieldInitializers(FieldInitializers fieldInitializers) {
    fieldInitializers_ = fieldInitializers;
  }
  const FieldInitializers& getFieldInitializers() { return fieldInitializers_; }

  // Allocate a new PrivateScriptData. Headers and GCPtrs are initialized.
  static PrivateScriptData* new_(JSContext* cx, uint32_t ngcthings);

  template <XDRMode mode>
  static MOZ_MUST_USE XDRResult XDR(js::XDRState<mode>* xdr,
                                    js::HandleScript script,
                                    js::HandleScriptSourceObject sourceObject,
                                    js::HandleScope scriptEnclosingScope,
                                    js::HandleObject funOrMod);

  // Clone src script data into dst script.
  static bool Clone(JSContext* cx, js::HandleScript src, js::HandleScript dst,
                    js::MutableHandle<JS::GCVector<js::Scope*>> scopes);

  static bool InitFromStencil(JSContext* cx, js::HandleScript script,
                              const js::frontend::ScriptStencil& stencil);

  void trace(JSTracer* trc);

  size_t allocationSize() const;

  // PrivateScriptData has trailing data so isn't copyable or movable.
  PrivateScriptData(const PrivateScriptData&) = delete;
  PrivateScriptData& operator=(const PrivateScriptData&) = delete;
};

// Script data that is shareable across a JSRuntime.
class RuntimeScriptData final {
  // This class is reference counted as follows: each pointer from a JSScript
  // counts as one reference plus there may be one reference from the shared
  // script data table.
  mozilla::Atomic<uint32_t, mozilla::SequentiallyConsistent> refCount_ = {};

  uint32_t natoms_ = 0;

  js::UniquePtr<ImmutableScriptData> isd_ = nullptr;

  // NOTE: The raw bytes of this structure are used for hashing so use explicit
  // padding values as needed for predicatable results across compilers.

  friend class ::JSScript;

 private:
  // Layout of trailing arrays.
  size_t atomOffset() const { return offsetOfAtoms(); }

  // Size to allocate.
  static size_t AllocationSize(uint32_t natoms);

  template <typename T>
  void initElements(size_t offset, size_t length);

  // Initialize to GC-safe state.
  explicit RuntimeScriptData(uint32_t natoms);

 public:
  // Hash over the contents of RuntimeScriptData and its ImmutableScriptData.
  struct Hasher;

  static RuntimeScriptData* new_(JSContext* cx, uint32_t natoms);

  uint32_t refCount() const { return refCount_; }
  void AddRef() { refCount_++; }
  void Release() {
    MOZ_ASSERT(refCount_ != 0);
    uint32_t remain = --refCount_;
    if (remain == 0) {
      isd_ = nullptr;
      js_free(this);
    }
  }

  uint32_t natoms() const { return natoms_; }
  GCPtrAtom* atoms() {
    uintptr_t base = reinterpret_cast<uintptr_t>(this);
    return reinterpret_cast<GCPtrAtom*>(base + atomOffset());
  }

  mozilla::Span<const GCPtrAtom> atomsSpan() const {
    uintptr_t base = reinterpret_cast<uintptr_t>(this);
    const GCPtrAtom* p =
        reinterpret_cast<const GCPtrAtom*>(base + atomOffset());
    return mozilla::MakeSpan(p, natoms_);
  }

  static constexpr size_t offsetOfAtoms() { return sizeof(RuntimeScriptData); }

  static constexpr size_t offsetOfISD() {
    return offsetof(RuntimeScriptData, isd_);
  }

  void traceChildren(JSTracer* trc);

  template <XDRMode mode>
  static MOZ_MUST_USE XDRResult XDR(js::XDRState<mode>* xdr,
                                    js::HandleScript script);

  // Mark this RuntimeScriptData for use in a new zone.
  void markForCrossZone(JSContext* cx);

  static bool InitFromStencil(JSContext* cx, js::HandleScript script,
                              js::frontend::ScriptStencil& stencil);

  size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) {
    return mallocSizeOf(this) + mallocSizeOf(isd_.get());
  }

  // RuntimeScriptData has trailing data so isn't copyable or movable.
  RuntimeScriptData(const RuntimeScriptData&) = delete;
  RuntimeScriptData& operator=(const RuntimeScriptData&) = delete;
};

// Matches RuntimeScriptData objects that have the same atoms as well as
// contain the same bytes in their ImmutableScriptData.
struct RuntimeScriptData::Hasher {
  using Lookup = RefPtr<RuntimeScriptData>;

  static HashNumber hash(const Lookup& l) {
    mozilla::Span<const uint8_t> immutableData = l->isd_->immutableData();

    HashNumber h =
        mozilla::HashBytes(immutableData.data(), immutableData.size());
    return mozilla::AddToHash(
        h, mozilla::HashBytes(l->atoms(), l->natoms() * sizeof(GCPtrAtom)));
  }

  static bool match(RuntimeScriptData* entry, const Lookup& lookup) {
    return (entry->atomsSpan() == lookup->atomsSpan()) &&
           (entry->isd_->immutableData() == lookup->isd_->immutableData());
  }
};

using RuntimeScriptDataTable =
    HashSet<RuntimeScriptData*, RuntimeScriptData::Hasher, SystemAllocPolicy>;

// Range of characters in scriptSource which contains a script's source,
// that is, the range used by the Parser to produce a script.
//
// For most functions the fields point to the following locations.
//
//   function * f(a, b) { return a + b; }
//   ^          ^                        ^
//   |          |                        |
//   |          sourceStart              sourceEnd
//   |                                   |
//   toStringStart                       toStringEnd
//
// For the special case of class constructors, the spec requires us to use an
// alternate definition of toStringStart / toStringEnd.
//
//   class C { constructor() { this.field = 42; } }
//   ^         ^                                 ^ ^
//   |         |                                 | `---------`
//   |         sourceStart                       sourceEnd   |
//   |                                                       |
//   toStringStart                                           toStringEnd
//
// NOTE: These are counted in Code Units from the start of the script source.
struct SourceExtent {
  SourceExtent() = default;

  SourceExtent(uint32_t sourceStart, uint32_t sourceEnd, uint32_t toStringStart,
               uint32_t toStringEnd, uint32_t lineno, uint32_t column)
      : sourceStart(sourceStart),
        sourceEnd(sourceEnd),
        toStringStart(toStringStart),
        toStringEnd(toStringEnd),
        lineno(lineno),
        column(column) {}

  uint32_t sourceStart = 0;
  uint32_t sourceEnd = 0;
  uint32_t toStringStart = 0;
  uint32_t toStringEnd = 0;

  // Line and column of |sourceStart_| position.
  uint32_t lineno = 0;
  uint32_t column = 0;  // Count of Code Points
};

// This class contains fields and accessors that are common to both lazy and
// non-lazy interpreted scripts. This must be located at offset +0 of any
// derived classes in order for the 'jitCodeRaw' mechanism to work with the
// JITs.
class BaseScript : public gc::TenuredCell {
 protected:
  // Pointer to baseline->method()->raw(), ion->method()->raw(), a wasm jit
  // entry, the JIT's EnterInterpreter stub, or the lazy link stub. Must be
  // non-null (except on no-jit builds).
  using HeaderWithCodePtr = gc::CellHeaderWithNonGCPointer<uint8_t>;
  HeaderWithCodePtr headerAndJitCodeRaw_;

  // Object that determines what Realm this script is compiled for. In general
  // this refers to the realm's GlobalObject, but for a lazy-script we instead
  // refer to the associated function.
  const GCPtrObject functionOrGlobal_ = {};

  // The ScriptSourceObject for this script.
  const GCPtr<ScriptSourceObject*> sourceObject_ = {};

  // Unshared variable-length data. This may be nullptr for lazy scripts of leaf
  // functions. Note that meaning of this data is different if the script is
  // lazy vs non-lazy. In both cases, the JSFunction pointers will represent the
  // inner-functions, but other kinds of entries have different interpretations.
  PrivateScriptData* data_ = nullptr;

  // Shareable script data. This includes runtime-wide atom pointers, bytecode,
  // and various script note structures. If the script is currently lazy, this
  // will not point to anything.
  RefPtr<js::RuntimeScriptData> sharedData_ = {};

  SourceExtent extent_;

 public:
  // Alias the enum into JSScript to provide easy translation for various
  // consumers
  using ImmutableFlags = ImmutableScriptFlagsEnum;
  using MutableFlags = MutableScriptFlagsEnum;

 protected:
  // Immutable flags should not be modified after this script has been
  // initialized. These flags should likely be preserved when serializing
  // (XDR) or copying (CopyScript) this script. This is only public for the
  // JITs.
  //
  // Specific accessors for flag values are defined with
  // IMMUTABLE_FLAG_* macros below.
  ImmutableScriptFlags immutableFlags_;
  // Mutable flags typically store information about runtime or deoptimization
  // behavior of this script. This is only public for the JITs.
  //
  // Specific accessors for flag values are defined with
  // MUTABLE_FLAG_* macros below.
  MutableScriptFlags mutableFlags_;

  ScriptWarmUpData warmUpData_ = {};

  BaseScript(uint8_t* stubEntry, JSObject* functionOrGlobal,
             ScriptSourceObject* sourceObject, SourceExtent extent,
             uint32_t immutableFlags)
      : headerAndJitCodeRaw_(stubEntry),
        functionOrGlobal_(functionOrGlobal),
        sourceObject_(sourceObject),
        extent_(extent),
        immutableFlags_(immutableFlags) {
    MOZ_ASSERT(functionOrGlobal->compartment() == sourceObject->compartment());
    MOZ_ASSERT(extent_.toStringStart <= extent_.sourceStart);
    MOZ_ASSERT(extent_.sourceStart <= extent_.sourceEnd);
    MOZ_ASSERT(extent_.sourceEnd <= extent_.toStringEnd);
  }

  void setJitCodeRaw(uint8_t* code) { headerAndJitCodeRaw_.setPtr(code); }

 public:
  // Create a lazy BaseScript without initializing any gc-things.
  static BaseScript* CreateRawLazy(JSContext* cx, uint32_t ngcthings,
                                   HandleFunction fun,
                                   HandleScriptSourceObject sourceObject,
                                   const SourceExtent& extent,
                                   uint32_t immutableFlags);

  // Create a lazy BaseScript and initialize gc-things with provided
  // closedOverBindings and innerFunctions.
  static BaseScript* CreateLazy(
      JSContext* cx, const frontend::CompilationInfo& compilationInfo,
      HandleFunction fun, HandleScriptSourceObject sourceObject,
      const frontend::AtomVector& closedOverBindings,
      const Vector<frontend::FunctionIndex>& innerFunctionIndexes,
      const SourceExtent& extent, uint32_t immutableFlags);

  uint8_t* jitCodeRaw() const { return headerAndJitCodeRaw_.ptr(); }
  bool isUsingInterpreterTrampoline(JSRuntime* rt) const;

  // Canonical function for the script, if it has a function. For global and
  // eval scripts this is nullptr.
  JSFunction* function() const {
    if (functionOrGlobal_->is<JSFunction>()) {
      return &functionOrGlobal_->as<JSFunction>();
    }
    return nullptr;
  }

  JS::Realm* realm() const { return functionOrGlobal_->nonCCWRealm(); }
  JS::Compartment* compartment() const {
    return functionOrGlobal_->compartment();
  }
  JS::Compartment* maybeCompartment() const { return compartment(); }
  inline JSPrincipals* principals() const;

  ScriptSourceObject* sourceObject() const { return sourceObject_; }
  ScriptSource* scriptSource() const { return sourceObject()->source(); }
  ScriptSource* maybeForwardedScriptSource() const;

  bool mutedErrors() const { return scriptSource()->mutedErrors(); }

  const char* filename() const { return scriptSource()->filename(); }
  const char* maybeForwardedFilename() const {
    return maybeForwardedScriptSource()->filename();
  }

  bool isBinAST() const { return scriptSource()->hasBinASTSource(); }

  uint32_t sourceStart() const { return extent_.sourceStart; }
  uint32_t sourceEnd() const { return extent_.sourceEnd; }
  uint32_t sourceLength() const {
    return extent_.sourceEnd - extent_.sourceStart;
  }
  uint32_t toStringStart() const { return extent_.toStringStart; }
  uint32_t toStringEnd() const { return extent_.toStringEnd; }
  SourceExtent extent() const { return extent_; }

  MOZ_MUST_USE bool appendSourceDataForToString(JSContext* cx,
                                                js::StringBuffer& buf);

#if defined(JS_BUILD_BINAST)
  // Set the position of the function in the source code.
  //
  // BinAST file format can put lazy functions after the entire tree, and in
  // that case BaseScript::CreateLazy will be called with dummy values for those
  // positions, and then once it reaches to the lazy function part, this
  // function is called to set those positions to correct value.
  void setPositions(uint32_t sourceStart, uint32_t sourceEnd,
                    uint32_t toStringStart, uint32_t toStringEnd) {
    MOZ_ASSERT(toStringStart <= sourceStart);
    MOZ_ASSERT(sourceStart <= sourceEnd);
    MOZ_ASSERT(sourceEnd <= toStringEnd);

    extent_.sourceStart = sourceStart;
    extent_.sourceEnd = sourceEnd;
    extent_.toStringStart = toStringStart;
    extent_.toStringEnd = toStringEnd;
  }

  void setColumn(uint32_t column) { extent_.column = column; }
#endif

  void setToStringEnd(uint32_t toStringEnd) {
    MOZ_ASSERT(extent_.toStringStart <= toStringEnd);
    MOZ_ASSERT(extent_.toStringEnd >= extent_.sourceEnd);
    extent_.toStringEnd = toStringEnd;
  }

  uint32_t lineno() const { return extent_.lineno; }
  uint32_t column() const { return extent_.column; }

 public:
  ImmutableScriptFlags immutableFlags() const { return immutableFlags_; }

  void addToImmutableFlags(const ImmutableScriptFlags& flags) {
    immutableFlags_ |= flags;
  }

  // ImmutableFlags accessors.
  MOZ_MUST_USE bool hasFlag(ImmutableFlags flag) const {
    return immutableFlags_.hasFlag(flag);
  }
  void setFlag(ImmutableFlags flag, bool b = true) {
    immutableFlags_.setFlag(flag, b);
  }
  void clearFlag(ImmutableFlags flag) { immutableFlags_.clearFlag(flag); }

  // MutableFlags accessors.
  MOZ_MUST_USE bool hasFlag(MutableFlags flag) const {
    return mutableFlags_.hasFlag(flag);
  }
  void setFlag(MutableFlags flag, bool b = true) {
    mutableFlags_.setFlag(flag, b);
  }
  void clearFlag(MutableFlags flag) { mutableFlags_.clearFlag(flag); }
  // Specific flag accessors

#define FLAG_GETTER(enumName, enumEntry, lowerName) \
 public:                                            \
  bool lowerName() const { return hasFlag(enumName::enumEntry); }

#define FLAG_GETTER_SETTER(enumName, enumEntry, setterLevel, lowerName, name) \
setterLevel:                                                                  \
  void set##name() { setFlag(enumName::enumEntry); }                          \
  void set##name(bool b) { setFlag(enumName::enumEntry, b); }                 \
  void clear##name() { clearFlag(enumName::enumEntry); }                      \
                                                                              \
 public:                                                                      \
  bool lowerName() const { return hasFlag(enumName::enumEntry); }

#define IMMUTABLE_FLAG_GETTER(lowerName, name) \
  FLAG_GETTER(ImmutableFlags, name, lowerName)
#define IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(lowerName, name) \
  FLAG_GETTER_SETTER(ImmutableFlags, name, public, lowerName, name)
#define MUTABLE_FLAG_GETTER(lowerName, name) \
  FLAG_GETTER(MutableFlags, name, lowerName)
#define MUTABLE_FLAG_GETTER_SETTER(lowerName, name) \
  FLAG_GETTER_SETTER(MutableFlags, name, public, lowerName, name)

  IMMUTABLE_FLAG_GETTER(noScriptRval, NoScriptRval)
  IMMUTABLE_FLAG_GETTER(selfHosted, SelfHosted)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(treatAsRunOnce, TreatAsRunOnce)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(forceStrict, ForceStrict)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(strict, Strict)
  IMMUTABLE_FLAG_GETTER(hasNonSyntacticScope, HasNonSyntacticScope)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(bindingsAccessedDynamically,
                                      BindingsAccessedDynamically)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(funHasExtensibleScope,
                                      FunHasExtensibleScope)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(hasCallSiteObj, HasCallSiteObj)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(hasModuleGoal, HasModuleGoal)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(functionHasThisBinding,
                                      FunctionHasThisBinding)
  // FunctionHasExtraBodyVarScope: custom logic below.
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(hasMappedArgsObj, HasMappedArgsObj)
  IMMUTABLE_FLAG_GETTER(hasInnerFunctions, HasInnerFunctions)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(needsHomeObject, NeedsHomeObject)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(isDerivedClassConstructor,
                                      IsDerivedClassConstructor)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(isLikelyConstructorWrapper,
                                      IsLikelyConstructorWrapper)
  IMMUTABLE_FLAG_GETTER(isGenerator, IsGenerator)
  IMMUTABLE_FLAG_GETTER(isAsync, IsAsync)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(hasRest, HasRest)
  // See FunctionBox::argumentsHasLocalBinding_ comment.
  // N.B.: no setter -- custom logic in JSScript.
  IMMUTABLE_FLAG_GETTER(argumentsHasVarBinding, ArgumentsHasVarBinding)
  IMMUTABLE_FLAG_GETTER(isForEval, IsForEval)
  IMMUTABLE_FLAG_GETTER(isModule, IsModule)
  IMMUTABLE_FLAG_GETTER(needsFunctionEnvironmentObjects,
                        NeedsFunctionEnvironmentObjects)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(shouldDeclareArguments,
                                      ShouldDeclareArguments)
  IMMUTABLE_FLAG_GETTER(isFunction, IsFunction)
  IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC(hasDirectEval, HasDirectEval)

  MUTABLE_FLAG_GETTER_SETTER(hasRunOnce, HasRunOnce)
  MUTABLE_FLAG_GETTER_SETTER(hasBeenCloned, HasBeenCloned)
  MUTABLE_FLAG_GETTER_SETTER(hasScriptCounts, HasScriptCounts)
  // Access the flag for whether this script has a DebugScript in its realm's
  // map. This should only be used by the DebugScript class.
  MUTABLE_FLAG_GETTER_SETTER(hasDebugScript, HasDebugScript)
  MUTABLE_FLAG_GETTER_SETTER(allowRelazify, AllowRelazify)
  MUTABLE_FLAG_GETTER_SETTER(failedBoundsCheck, FailedBoundsCheck)
  MUTABLE_FLAG_GETTER_SETTER(failedShapeGuard, FailedShapeGuard)
  MUTABLE_FLAG_GETTER_SETTER(hadFrequentBailouts, HadFrequentBailouts)
  MUTABLE_FLAG_GETTER_SETTER(hadOverflowBailout, HadOverflowBailout)
  MUTABLE_FLAG_GETTER_SETTER(uninlineable, Uninlineable)
  MUTABLE_FLAG_GETTER_SETTER(invalidatedIdempotentCache,
                             InvalidatedIdempotentCache)
  MUTABLE_FLAG_GETTER_SETTER(failedLexicalCheck, FailedLexicalCheck)
  MUTABLE_FLAG_GETTER_SETTER(needsArgsAnalysis, NeedsArgsAnalysis)
  // NeedsArgsObj: custom logic below.
  MUTABLE_FLAG_GETTER_SETTER(spewEnabled, SpewEnabled)

#undef IMMUTABLE_FLAG_GETTER
#undef IMMUTABLE_FLAG_GETTER_SETTER_PUBLIC
#undef MUTABLE_FLAG_GETTER
#undef MUTABLE_FLAG_GETTER_SETTER
#undef FLAG_GETTER
#undef FLAG_GETTER_SETTER

  GeneratorKind generatorKind() const {
    return isGenerator() ? GeneratorKind::Generator
                         : GeneratorKind::NotGenerator;
  }

  void setGeneratorKind(GeneratorKind kind) {
    // A script only gets its generator kind set as part of initialization,
    // so it can only transition from NotGenerator.
    MOZ_ASSERT(!isGenerator());
    if (kind == GeneratorKind::Generator) {
      setFlag(ImmutableFlags::IsGenerator);
    }
  }

  FunctionAsyncKind asyncKind() const {
    return isAsync() ? FunctionAsyncKind::AsyncFunction
                     : FunctionAsyncKind::SyncFunction;
  }

  void setAsyncKind(FunctionAsyncKind kind) {
    if (kind == FunctionAsyncKind::AsyncFunction) {
      setFlag(ImmutableFlags::IsAsync);
    }
  }

  frontend::ParseGoal parseGoal() const {
    return hasModuleGoal() ? frontend::ParseGoal::Module
                           : frontend::ParseGoal::Script;
  }

  void setArgumentsHasVarBinding();

  bool hasEnclosingScript() const { return warmUpData_.isEnclosingScript(); }
  BaseScript* enclosingScript() const {
    return warmUpData_.toEnclosingScript();
  }
  void setEnclosingScript(BaseScript* enclosingScript);

  // Returns true is the script has an enclosing scope but no bytecode. It is
  // ready for delazification.
  // NOTE: The enclosing script must have been successfully compiled at some
  // point for the enclosing scope to exist. That script may have since been
  // GC'd, but we kept the scope live so we can still compile ourselves.
  bool isReadyForDelazification() const {
    return warmUpData_.isEnclosingScope();
  }

  Scope* enclosingScope() const {
    MOZ_ASSERT(!warmUpData_.isEnclosingScript(),
               "Enclosing scope is not computed yet");

    if (warmUpData_.isEnclosingScope()) {
      return warmUpData_.toEnclosingScope();
    }

    MOZ_ASSERT(data_, "Script doesn't seem to be compiled");

    size_t outermostScopeIndex = 0;
    return gcthings()[outermostScopeIndex].as<Scope>().enclosing();
  }
  void setEnclosingScope(Scope* enclosingScope);
  Scope* releaseEnclosingScope();

  bool hasJitScript() const { return warmUpData_.isJitScript(); }
  jit::JitScript* jitScript() const {
    MOZ_ASSERT(hasJitScript());
    return warmUpData_.toJitScript();
  }
  jit::JitScript* maybeJitScript() const {
    return hasJitScript() ? jitScript() : nullptr;
  }

  inline bool hasBaselineScript() const;
  inline bool hasIonScript() const;

  bool hasPrivateScriptData() const { return data_ != nullptr; }

  // Update data_ pointer while also informing GC MemoryUse tracking.
  void swapData(UniquePtr<PrivateScriptData>& other);

  mozilla::Span<const JS::GCCellPtr> gcthings() const {
    return data_ ? data_->gcthings() : mozilla::Span<JS::GCCellPtr>();
  }

  void setFieldInitializers(FieldInitializers fieldInitializers) {
    MOZ_ASSERT(data_);
    data_->setFieldInitializers(fieldInitializers);
  }
  const FieldInitializers& getFieldInitializers() const {
    MOZ_ASSERT(data_);
    return data_->getFieldInitializers();
  }

  RuntimeScriptData* sharedData() const { return sharedData_; }
  void freeSharedData() { sharedData_ = nullptr; }

  // NOTE: Script only has bytecode if JSScript::fullyInitFromStencil completes
  // successfully.
  bool hasBytecode() const {
    if (sharedData_) {
      MOZ_ASSERT(data_);
      MOZ_ASSERT(warmUpData_.isWarmUpCount() || warmUpData_.isJitScript());
      return true;
    }
    return false;
  }

 public:
  static const JS::TraceKind TraceKind = JS::TraceKind::Script;
  const gc::CellHeader& cellHeader() const { return headerAndJitCodeRaw_; }

  void traceChildren(JSTracer* trc);
  void finalize(JSFreeOp* fop);

  size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) {
    return mallocSizeOf(data_);
  }

  inline JSScript* asJSScript();

  template <XDRMode mode>
  static XDRResult XDRLazyScriptData(XDRState<mode>* xdr,
                                     HandleScriptSourceObject sourceObject,
                                     Handle<BaseScript*> lazy,
                                     bool hasFieldInitializer);

  // JIT accessors
  static constexpr size_t offsetOfJitCodeRaw() {
    return offsetof(BaseScript, headerAndJitCodeRaw_) +
           HeaderWithCodePtr::offsetOfPtr();
  }
  static constexpr size_t offsetOfPrivateData() {
    return offsetof(BaseScript, data_);
  }
  static constexpr size_t offsetOfSharedData() {
    return offsetof(BaseScript, sharedData_);
  }
  static size_t offsetOfImmutableFlags() {
    static_assert(offsetof(ImmutableScriptFlags, flags_) == 0,
                  "Required for JIT flag access");
    return offsetof(BaseScript, immutableFlags_);
  }
  static constexpr size_t offsetOfMutableFlags() {
    return offsetof(BaseScript, mutableFlags_);
  }
  static constexpr size_t offsetOfWarmUpData() {
    return offsetof(BaseScript, warmUpData_);
  }
};

/*
 * NB: after a successful XDR_DECODE, XDRScript callers must do any required
 * subsequent set-up of owning function or script object and then call
 * CallNewScriptHook.
 */
template <XDRMode mode>
XDRResult XDRScript(XDRState<mode>* xdr, HandleScope enclosingScope,
                    HandleScriptSourceObject sourceObject,
                    HandleObject funOrMod, MutableHandleScript scriptp);

template <XDRMode mode>
XDRResult XDRLazyScript(XDRState<mode>* xdr, HandleScope enclosingScope,
                        HandleScriptSourceObject sourceObject,
                        HandleFunction fun, MutableHandle<BaseScript*> lazy);

/*
 * Code any constant value.
 */
template <XDRMode mode>
XDRResult XDRScriptConst(XDRState<mode>* xdr, MutableHandleValue vp);

extern void SweepScriptData(JSRuntime* rt);

} /* namespace js */

namespace JS {

// Define a GCManagedDeletePolicy to allow deleting type outside of normal
// sweeping.
template <>
struct DeletePolicy<js::PrivateScriptData>
    : public js::GCManagedDeletePolicy<js::PrivateScriptData> {};

} /* namespace JS */

class JSScript : public js::BaseScript {
 private:
  template <js::XDRMode mode>
  friend js::XDRResult js::XDRScript(js::XDRState<mode>* xdr,
                                     js::HandleScope enclosingScope,
                                     js::HandleScriptSourceObject sourceObject,
                                     js::HandleObject funOrMod,
                                     js::MutableHandleScript scriptp);

  template <js::XDRMode mode>
  friend js::XDRResult js::RuntimeScriptData::XDR(js::XDRState<mode>* xdr,
                                                  js::HandleScript script);

  friend bool js::RuntimeScriptData::InitFromStencil(
      JSContext* cx, js::HandleScript script,
      js::frontend::ScriptStencil& stencil);

  template <js::XDRMode mode>
  friend js::XDRResult js::PrivateScriptData::XDR(
      js::XDRState<mode>* xdr, js::HandleScript script,
      js::HandleScriptSourceObject sourceObject,
      js::HandleScope scriptEnclosingScope, js::HandleObject funOrMod);

  friend bool js::PrivateScriptData::Clone(
      JSContext* cx, js::HandleScript src, js::HandleScript dst,
      js::MutableHandle<JS::GCVector<js::Scope*>> scopes);

  friend bool js::PrivateScriptData::InitFromStencil(
      JSContext* cx, js::HandleScript script,
      const js::frontend::ScriptStencil& stencil);

  friend JSScript* js::detail::CopyScript(
      JSContext* cx, js::HandleScript src, js::HandleObject functionOrGlobal,
      js::HandleScriptSourceObject sourceObject,
      js::MutableHandle<JS::GCVector<js::Scope*>> scopes);

 private:
  using js::BaseScript::BaseScript;

  static JSScript* New(JSContext* cx, js::HandleObject functionOrGlobal,
                       js::HandleScriptSourceObject sourceObject,
                       const js::SourceExtent& extent, uint32_t immutableFlags);

 public:
  static JSScript* Create(JSContext* cx, js::HandleObject functionOrGlobal,
                          const JS::ReadOnlyCompileOptions& options,
                          js::HandleScriptSourceObject sourceObject,
                          const js::SourceExtent& extent);

  static JSScript* Create(JSContext* cx, js::HandleObject functionOrGlobal,
                          js::HandleScriptSourceObject sourceObject,
                          js::ImmutableScriptFlags flags,
                          js::SourceExtent extent);

  // NOTE: This should only be used while delazifying.
  static JSScript* CastFromLazy(js::BaseScript* lazy) {
    return static_cast<JSScript*>(lazy);
  }

  // NOTE: If you use createPrivateScriptData directly instead of via
  // fullyInitFromStencil, you are responsible for notifying the debugger
  // after successfully creating the script.
  static bool createPrivateScriptData(JSContext* cx,
                                      JS::Handle<JSScript*> script,
                                      uint32_t ngcthings);

 public:
  static bool fullyInitFromStencil(
      JSContext* cx, js::frontend::CompilationInfo& compilationInfo,
      js::HandleScript script, js::frontend::ScriptStencil& stencil);

#ifdef DEBUG
 private:
  // Assert that jump targets are within the code array of the script.
  void assertValidJumpTargets() const;
#endif

 public:
  js::ImmutableScriptData* immutableScriptData() const {
    return sharedData_->isd_.get();
  }

  // Script bytecode is immutable after creation.
  jsbytecode* code() const {
    if (!sharedData_) {
      return nullptr;
    }
    return immutableScriptData()->code();
  }

  bool hasForceInterpreterOp() const {
    // JSOp::ForceInterpreter, if present, must be the first op.
    MOZ_ASSERT(length() >= 1);
    return JSOp(*code()) == JSOp::ForceInterpreter;
  }

  js::AllBytecodesIterable allLocations() {
    return js::AllBytecodesIterable(this);
  }

  js::BytecodeLocation location() { return js::BytecodeLocation(this, code()); }

  size_t length() const {
    MOZ_ASSERT(sharedData_);
    return immutableScriptData()->codeLength();
  }

  jsbytecode* codeEnd() const { return code() + length(); }

  jsbytecode* lastPC() const {
    jsbytecode* pc = codeEnd() - js::JSOpLength_RetRval;
    MOZ_ASSERT(JSOp(*pc) == JSOp::RetRval);
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

  size_t mainOffset() const { return immutableScriptData()->mainOffset; }

  // The fixed part of a stack frame is comprised of vars (in function and
  // module code) and block-scoped locals (in all kinds of code).
  size_t nfixed() const { return immutableScriptData()->nfixed; }

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

  size_t nslots() const { return immutableScriptData()->nslots; }

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

  bool functionAllowsParameterRedeclaration() const {
    // Parameter redeclaration is only allowed for non-strict functions with
    // simple parameter lists, which are neither arrow nor method functions. We
    // don't have a flag at hand to test the function kind, but we can still
    // test if the function is non-strict and has a simple parameter list by
    // checking |hasMappedArgsObj()|. (Mapped arguments objects are only
    // created for non-strict functions with simple parameter lists.)
    return hasMappedArgsObj();
  }

  // If there are more than MaxBytecodeTypeSets JOF_TYPESET ops in the script,
  // the first MaxBytecodeTypeSets - 1 JOF_TYPESET ops have their own TypeSet
  // and all other JOF_TYPESET ops share the last TypeSet.
  static constexpr size_t MaxBytecodeTypeSets = UINT16_MAX;
  static_assert(sizeof(js::ImmutableScriptData::numBytecodeTypeSets) == 2,
                "MaxBytecodeTypeSets must match sizeof(numBytecodeTypeSets)");

  size_t numBytecodeTypeSets() const {
    return immutableScriptData()->numBytecodeTypeSets;
  }

  size_t numICEntries() const { return immutableScriptData()->numICEntries; }

  size_t funLength() const { return immutableScriptData()->funLength; }

  void cacheForEval() {
    MOZ_ASSERT(isForEval());
    // IsEvalCacheCandidate will make sure that there's nothing in this
    // script that would prevent reexecution even if isRunOnce is
    // true.  So just pretend like we never ran this script.
    clearFlag(MutableFlags::HasRunOnce);
  }

  bool argumentsAliasesFormals() const {
    return argumentsHasVarBinding() && hasMappedArgsObj();
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

  // Update the the arguments analysis flags based on the frontend
  // derived information.
  void resetArgsUsageAnalysis();

  /*
   * Arguments access (via JSOp::*Arg* opcodes) must access the canonical
   * location for the argument. If an arguments object exists AND it's mapped
   * ('arguments' aliases formals), then all access must go through the
   * arguments object. Otherwise, the local slot is the canonical location for
   * the arguments. Note: if a formal is aliased through the scope chain, then
   * script->formalIsAliased and JSOp::*Arg* opcodes won't be emitted at all.
   */
  bool argsObjAliasesFormals() const {
    return needsArgsObj() && hasMappedArgsObj();
  }

  void updateJitCodeRaw(JSRuntime* rt);

  bool isRelazifiable() const {
    // A script may not be relazifiable if parts of it can be entrained in
    // interesting ways:
    //  - Scripts with inner-functions or direct-eval (which can add
    //    inner-functions) should not be relazified as their Scopes may be part
    //    of another scope-chain.
    //  - Generators and async functions may be re-entered in complex ways so
    //    don't discard bytecode. The JIT resume code assumes this.
    //  - Functions with template literals must always return the same object
    //    instance so must not discard it by relazifying.
    return !hasInnerFunctions() && !hasDirectEval() && !isGenerator() &&
           !isAsync() && !hasCallSiteObj();
  }

  js::ModuleObject* module() const {
    if (bodyScope()->is<js::ModuleScope>()) {
      return bodyScope()->as<js::ModuleScope>().module();
    }
    return nullptr;
  }

  bool isGlobalCode() const { return bodyScope()->is<js::GlobalScope>(); }

  // Returns true if the script may read formal arguments on the stack
  // directly, via lazy arguments or a rest parameter.
  bool mayReadFrameArgsDirectly();

  static JSLinearString* sourceData(JSContext* cx, JS::HandleScript script);

  void setDefaultClassConstructorSpan(uint32_t start, uint32_t end,
                                      unsigned line, unsigned column);

#ifdef MOZ_VTUNE
  // Unique Method ID passed to the VTune profiler. Allows attribution of
  // different jitcode to the same source script.
  uint32_t vtuneMethodID();
#endif

 public:
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
  bool isTopLevel() { return code() && !isFunction(); }

  /* Ensure the script has a JitScript. */
  inline bool ensureHasJitScript(JSContext* cx, js::jit::AutoKeepJitScripts&);

  void maybeReleaseJitScript(JSFreeOp* fop);
  void releaseJitScript(JSFreeOp* fop);
  void releaseJitScriptOnFinalize(JSFreeOp* fop);

  inline js::jit::BaselineScript* baselineScript() const;
  inline js::jit::IonScript* ionScript() const;

  inline bool isIonCompilingOffThread() const;
  inline bool canIonCompile() const;
  inline void disableIon();

  inline bool canBaselineCompile() const;
  inline void disableBaselineCompile();

  inline js::GlobalObject& global() const;
  inline bool hasGlobal(const js::GlobalObject* global) const;
  js::GlobalObject& uninlinedGlobal() const;

  uint32_t bodyScopeIndex() const {
    return immutableScriptData()->bodyScopeIndex;
  }

  js::Scope* bodyScope() const { return getScope(bodyScopeIndex()); }

  js::Scope* outermostScope() const {
    // The body scope may not be the outermost scope in the script when
    // the decl env scope is present.
    size_t index = 0;
    return getScope(index);
  }

  bool functionHasExtraBodyVarScope() const {
    bool res = hasFlag(ImmutableFlags::FunctionHasExtraBodyVarScope);
    MOZ_ASSERT_IF(res, functionHasParameterExprs());
    return res;
  }

  js::VarScope* functionExtraBodyVarScope() const {
    MOZ_ASSERT(functionHasExtraBodyVarScope());
    for (JS::GCCellPtr gcThing : gcthings()) {
      if (!gcThing.is<js::Scope>()) {
        continue;
      }
      js::Scope* scope = &gcThing.as<js::Scope>();
      if (scope->kind() == js::ScopeKind::FunctionBodyVar) {
        return &scope->as<js::VarScope>();
      }
    }
    MOZ_CRASH("Function extra body var scope not found");
  }

  bool needsBodyEnvironment() const {
    for (JS::GCCellPtr gcThing : gcthings()) {
      if (!gcThing.is<js::Scope>()) {
        continue;
      }
      js::Scope* scope = &gcThing.as<js::Scope>();
      if (ScopeKindIsInBody(scope->kind()) && scope->hasEnvironment()) {
        return true;
      }
    }
    return false;
  }

  inline js::LexicalScope* maybeNamedLambdaScope() const;

  // Drop script data and reset warmUpData to reference enclosing scope.
  void relazify(JSRuntime* rt);

 private:
  bool createJitScript(JSContext* cx);

  bool createScriptData(JSContext* cx, uint32_t natoms);
  void initImmutableScriptData(js::UniquePtr<js::ImmutableScriptData>&& data) {
    MOZ_ASSERT(!sharedData_->isd_);
    sharedData_->isd_ = std::move(data);
  }
  bool shareScriptData(JSContext* cx);

 public:
  inline uint32_t getWarmUpCount() const;
  inline void incWarmUpCounter(uint32_t amount = 1);
  inline void resetWarmUpCounterForGC();

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
  js::ScriptCounts& getScriptCounts();
  js::PCCounts* maybeGetPCCounts(jsbytecode* pc);
  const js::PCCounts* maybeGetThrowCounts(jsbytecode* pc);
  js::PCCounts* getThrowCounts(jsbytecode* pc);
  uint64_t getHitCount(jsbytecode* pc);
  void incHitCount(jsbytecode* pc);  // Used when we bailout out of Ion.
  void addIonCounts(js::jit::IonScriptCounts* ionCounts);
  js::jit::IonScriptCounts* getIonCounts();
  void releaseScriptCounts(js::ScriptCounts* counts);
  void destroyScriptCounts();
  void resetScriptCounts();

  jsbytecode* main() const { return code() + mainOffset(); }

  js::BytecodeLocation mainLocation() const {
    return js::BytecodeLocation(this, main());
  }

  js::BytecodeLocation endLocation() const {
    return js::BytecodeLocation(this, codeEnd());
  }

  js::BytecodeLocation offsetToLocation(uint32_t offset) const {
    return js::BytecodeLocation(this, offsetToPC(offset));
  }

  void addSizeOfJitScript(mozilla::MallocSizeOf mallocSizeOf,
                          size_t* sizeOfJitScript,
                          size_t* sizeOfBaselineFallbackStubs) const;

  mozilla::Span<const js::TryNote> trynotes() const {
    return immutableScriptData()->tryNotes();
  }

  mozilla::Span<const js::ScopeNote> scopeNotes() const {
    return immutableScriptData()->scopeNotes();
  }

  mozilla::Span<const uint32_t> resumeOffsets() const {
    return immutableScriptData()->resumeOffsets();
  }

  uint32_t tableSwitchCaseOffset(jsbytecode* pc, uint32_t caseIndex) const {
    MOZ_ASSERT(containsPC(pc));
    MOZ_ASSERT(JSOp(*pc) == JSOp::TableSwitch);
    uint32_t firstResumeIndex = GET_RESUMEINDEX(pc + 3 * JUMP_OFFSET_LEN);
    return resumeOffsets()[firstResumeIndex + caseIndex];
  }
  jsbytecode* tableSwitchCasePC(jsbytecode* pc, uint32_t caseIndex) const {
    return offsetToPC(tableSwitchCaseOffset(pc, caseIndex));
  }

  bool hasLoops();

  uint32_t numNotes() const {
    MOZ_ASSERT(sharedData_);
    return immutableScriptData()->noteLength();
  }
  js::SrcNote* notes() const {
    MOZ_ASSERT(sharedData_);
    return immutableScriptData()->notes();
  }

  size_t natoms() const {
    MOZ_ASSERT(sharedData_);
    return sharedData_->natoms();
  }
  js::GCPtrAtom* atoms() const {
    MOZ_ASSERT(sharedData_);
    return sharedData_->atoms();
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

  JSObject* getObject(size_t index) const {
    MOZ_ASSERT(gcthings()[index].asCell()->isTenured());
    return &gcthings()[index].as<JSObject>();
  }

  JSObject* getObject(jsbytecode* pc) const {
    MOZ_ASSERT(containsPC(pc) && containsPC(pc + sizeof(uint32_t)));
    return getObject(GET_UINT32_INDEX(pc));
  }

  js::Scope* getScope(size_t index) const {
    return &gcthings()[index].as<js::Scope>();
  }

  js::Scope* getScope(jsbytecode* pc) const {
    // This method is used to get a scope directly using a JSOp with an
    // index. To search through ScopeNotes to look for a Scope using pc,
    // use lookupScope.
    MOZ_ASSERT(containsPC(pc) && containsPC(pc + sizeof(uint32_t)));
    MOZ_ASSERT(js::JOF_OPTYPE(JSOp(*pc)) == JOF_SCOPE,
               "Did you mean to use lookupScope(pc)?");
    return getScope(GET_UINT32_INDEX(pc));
  }

  inline JSFunction* getFunction(size_t index) const;
  inline JSFunction* getFunction(jsbytecode* pc) const;

  inline js::RegExpObject* getRegExp(size_t index) const;
  inline js::RegExpObject* getRegExp(jsbytecode* pc) const;

  js::BigInt* getBigInt(size_t index) const {
    MOZ_ASSERT(gcthings()[index].asCell()->isTenured());
    return &gcthings()[index].as<js::BigInt>();
  }

  js::BigInt* getBigInt(jsbytecode* pc) const {
    MOZ_ASSERT(containsPC(pc));
    MOZ_ASSERT(js::JOF_OPTYPE(JSOp(*pc)) == JOF_BIGINT);
    return getBigInt(GET_UINT32_INDEX(pc));
  }

  // The following 3 functions find the static scope just before the
  // execution of the instruction pointed to by pc.

  js::Scope* lookupScope(jsbytecode* pc) const;

  js::Scope* innermostScope(jsbytecode* pc) const;
  js::Scope* innermostScope() const { return innermostScope(main()); }

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
    if (noScriptRval() && JSOp(*pc) == JSOp::False) {
      ++pc;
    }
    return JSOp(*pc) == JSOp::RetRval;
  }

  bool formalIsAliased(unsigned argSlot);
  bool formalLivesInArgumentsObject(unsigned argSlot);

  // See comment above 'debugMode' in Realm.h for explanation of
  // invariants of debuggee compartments, scripts, and frames.
  inline bool isDebuggee() const;

  // A helper class to prevent relazification of the given function's script
  // while it's holding on to it.  This class automatically roots the script.
  class AutoDelazify;
  friend class AutoDelazify;

  class AutoDelazify {
    JS::RootedScript script_;
    JSContext* cx_;
    bool oldAllowRelazify_ = false;

   public:
    explicit AutoDelazify(JSContext* cx, JS::HandleFunction fun = nullptr)
        : script_(cx), cx_(cx) {
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
};

namespace js {

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

extern JS::UniqueChars FormatIntroducedFilename(JSContext* cx,
                                                const char* filename,
                                                unsigned lineno,
                                                const char* introducer);

struct GSNCache;

const js::SrcNote* GetSrcNote(GSNCache& cache, JSScript* script,
                              jsbytecode* pc);

extern const js::SrcNote* GetSrcNote(JSContext* cx, JSScript* script,
                                     jsbytecode* pc);

extern jsbytecode* LineNumberToPC(JSScript* script, unsigned lineno);

extern JS_FRIEND_API unsigned GetScriptLineExtent(JSScript* script);

} /* namespace js */

namespace js {

extern unsigned PCToLineNumber(JSScript* script, jsbytecode* pc,
                               unsigned* columnp = nullptr);

extern unsigned PCToLineNumber(unsigned startLine, SrcNote* notes,
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

namespace JS {
namespace ubi {

template <>
class Concrete<JSScript> : public Concrete<js::BaseScript> {};

}  // namespace ubi
}  // namespace JS

#endif /* vm_JSScript_h */
