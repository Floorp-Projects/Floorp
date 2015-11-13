/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeapSnapshot.h"

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/gzip_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "js/Debug.h"
#include "js/TypeDecls.h"
#include "js/UbiNodeCensus.h"
#include "js/UbiNodeBreadthFirst.h"
#include "mozilla/Attributes.h"
#include "mozilla/devtools/AutoMemMap.h"
#include "mozilla/devtools/CoreDump.pb.h"
#include "mozilla/devtools/DeserializedNode.h"
#include "mozilla/devtools/FileDescriptorOutputStream.h"
#include "mozilla/devtools/HeapSnapshotTempFileHelperChild.h"
#include "mozilla/devtools/ZeroCopyNSIOutputStream.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/HeapSnapshotBinding.h"
#include "mozilla/RangedPtr.h"
#include "mozilla/Telemetry.h"

#include "jsapi.h"
#include "nsCycleCollectionParticipant.h"
#include "nsCRTGlue.h"
#include "nsDirectoryServiceDefs.h"
#include "nsIFile.h"
#include "nsIOutputStream.h"
#include "nsISupportsImpl.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "prerror.h"
#include "prio.h"
#include "prtypes.h"

namespace mozilla {
namespace devtools {

using namespace JS;
using namespace dom;

using ::google::protobuf::io::ArrayInputStream;
using ::google::protobuf::io::CodedInputStream;
using ::google::protobuf::io::GzipInputStream;
using ::google::protobuf::io::ZeroCopyInputStream;

using JS::ubi::AtomOrTwoByteChars;

NS_IMPL_CYCLE_COLLECTION_CLASS(HeapSnapshot)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(HeapSnapshot)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(HeapSnapshot)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParent)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(HeapSnapshot)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(HeapSnapshot)
NS_IMPL_CYCLE_COLLECTING_RELEASE(HeapSnapshot)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(HeapSnapshot)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

/* virtual */ JSObject*
HeapSnapshot::WrapObject(JSContext* aCx, HandleObject aGivenProto)
{
  return HeapSnapshotBinding::Wrap(aCx, this, aGivenProto);
}

/*** Reading Heap Snapshots ***********************************************************************/

/* static */ already_AddRefed<HeapSnapshot>
HeapSnapshot::Create(JSContext* cx,
                     GlobalObject& global,
                     const uint8_t* buffer,
                     uint32_t size,
                     ErrorResult& rv)
{
  RefPtr<HeapSnapshot> snapshot = new HeapSnapshot(cx, global.GetAsSupports());
  if (!snapshot->init(cx, buffer, size)) {
    rv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }
  return snapshot.forget();
}

template<typename MessageType>
static bool
parseMessage(ZeroCopyInputStream& stream, MessageType& message)
{
  // We need to create a new `CodedInputStream` for each message so that the
  // 64MB limit is applied per-message rather than to the whole stream.
  CodedInputStream codedStream(&stream);

  // The protobuf message nesting that core dumps exhibit is dominated by
  // allocation stacks' frames. In the most deeply nested case, each frame has
  // two messages: a StackFrame message and a StackFrame::Data message. These
  // frames are on top of a small constant of other messages. There are a
  // MAX_STACK_DEPTH number of frames, so we multiply this by 3 to make room for
  // the two messages per frame plus some head room for the constant number of
  // non-dominating messages.
  codedStream.SetRecursionLimit(HeapSnapshot::MAX_STACK_DEPTH * 3);

  // Because protobuf messages aren't self-delimiting, we serialize each message
  // preceeded by its size in bytes. When deserializing, we read this size and
  // then limit reading from the stream to the given byte size. If we didn't,
  // then the first message would consume the entire stream.

  uint32_t size = 0;
  if (NS_WARN_IF(!codedStream.ReadVarint32(&size)))
    return false;

  auto limit = codedStream.PushLimit(size);
  if (NS_WARN_IF(!message.ParseFromCodedStream(&codedStream)) ||
      NS_WARN_IF(!codedStream.ConsumedEntireMessage()) ||
      NS_WARN_IF(codedStream.BytesUntilLimit() != 0))
  {
    return false;
  }

  codedStream.PopLimit(limit);
  return true;
}

template<typename CharT, typename InternedStringSet>
struct GetOrInternStringMatcher
{
  using ReturnType = const CharT*;

  InternedStringSet& internedStrings;

  explicit GetOrInternStringMatcher(InternedStringSet& strings) : internedStrings(strings) { }

  const CharT* match(const std::string* str) {
    MOZ_ASSERT(str);
    size_t length = str->length() / sizeof(CharT);
    auto tempString = reinterpret_cast<const CharT*>(str->data());

    UniquePtr<CharT[], NSFreePolicy> owned(NS_strndup(tempString, length));
    if (!owned || !internedStrings.append(Move(owned)))
      return nullptr;

    return internedStrings.back().get();
  }

  const CharT* match(uint64_t ref) {
    if (MOZ_LIKELY(ref < internedStrings.length())) {
      auto& string = internedStrings[ref];
      MOZ_ASSERT(string);
      return string.get();
    }

    return nullptr;
  }
};

template<
  // Either char or char16_t.
  typename CharT,
  // A reference to either `internedOneByteStrings` or `internedTwoByteStrings`
  // if CharT is char or char16_t respectively.
  typename InternedStringSet>
const CharT*
HeapSnapshot::getOrInternString(InternedStringSet& internedStrings,
                                Maybe<StringOrRef>& maybeStrOrRef)
{
  // Incomplete message: has neither a string nor a reference to an already
  // interned string.
  if (MOZ_UNLIKELY(maybeStrOrRef.isNothing()))
    return nullptr;

  GetOrInternStringMatcher<CharT, InternedStringSet> m(internedStrings);
  return maybeStrOrRef->match(m);
}

// Get a de-duplicated string as a Maybe<StringOrRef> from the given `msg`.
#define GET_STRING_OR_REF_WITH_PROP_NAMES(msg, strPropertyName, refPropertyName) \
  (msg.has_##refPropertyName()                                                   \
    ? Some(StringOrRef(msg.refPropertyName()))                                   \
    : msg.has_##strPropertyName()                                                \
      ? Some(StringOrRef(&msg.strPropertyName()))                                \
      : Nothing())

#define GET_STRING_OR_REF(msg, property)      \
  (msg.has_##property##ref()                  \
     ? Some(StringOrRef(msg.property##ref())) \
     : msg.has_##property()                   \
       ? Some(StringOrRef(&msg.property()))   \
       : Nothing())

bool
HeapSnapshot::saveNode(const protobuf::Node& node, NodeIdSet& edgeReferents)
{
  // NB: de-duplicated string properties must be read back and interned in the
  // same order here as they are written and serialized in
  // `CoreDumpWriter::writeNode` or else indices in references to already
  // serialized strings will be off.

  if (NS_WARN_IF(!node.has_id()))
    return false;
  NodeId id = node.id();

  // NodeIds are derived from pointers (at most 48 bits) and we rely on them
  // fitting into JS numbers (IEEE 754 doubles, can precisely store 53 bit
  // integers) despite storing them on disk as 64 bit integers.
  if (NS_WARN_IF(!JS::Value::isNumberRepresentable(id)))
    return false;

  // Should only deserialize each node once.
  if (NS_WARN_IF(nodes.has(id)))
    return false;

  if (NS_WARN_IF(!JS::ubi::Uint32IsValidCoarseType(node.coarsetype())))
    return false;
  auto coarseType = JS::ubi::Uint32ToCoarseType(node.coarsetype());

  Maybe<StringOrRef> typeNameOrRef = GET_STRING_OR_REF_WITH_PROP_NAMES(node, typename_, typenameref);
  auto typeName = getOrInternString<char16_t>(internedTwoByteStrings, typeNameOrRef);
  if (NS_WARN_IF(!typeName))
    return false;

  if (NS_WARN_IF(!node.has_size()))
    return false;
  uint64_t size = node.size();

  auto edgesLength = node.edges_size();
  DeserializedNode::EdgeVector edges;
  if (NS_WARN_IF(!edges.reserve(edgesLength)))
    return false;
  for (decltype(edgesLength) i = 0; i < edgesLength; i++) {
    auto& protoEdge = node.edges(i);

    if (NS_WARN_IF(!protoEdge.has_referent()))
      return false;
    NodeId referent = protoEdge.referent();

    if (NS_WARN_IF(!edgeReferents.put(referent)))
      return false;

    const char16_t* edgeName = nullptr;
    if (protoEdge.EdgeNameOrRef_case() != protobuf::Edge::EDGENAMEORREF_NOT_SET) {
      Maybe<StringOrRef> edgeNameOrRef = GET_STRING_OR_REF(protoEdge, name);
      edgeName = getOrInternString<char16_t>(internedTwoByteStrings, edgeNameOrRef);
      if (NS_WARN_IF(!edgeName))
        return false;
    }

    edges.infallibleAppend(DeserializedEdge(referent, edgeName));
  }

  Maybe<StackFrameId> allocationStack;
  if (node.has_allocationstack()) {
    StackFrameId id = 0;
    if (NS_WARN_IF(!saveStackFrame(node.allocationstack(), id)))
      return false;
    allocationStack.emplace(id);
  }
  MOZ_ASSERT(allocationStack.isSome() == node.has_allocationstack());

  const char* jsObjectClassName = nullptr;
  if (node.JSObjectClassNameOrRef_case() != protobuf::Node::JSOBJECTCLASSNAMEORREF_NOT_SET) {
    Maybe<StringOrRef> clsNameOrRef = GET_STRING_OR_REF(node, jsobjectclassname);
    jsObjectClassName = getOrInternString<char>(internedOneByteStrings, clsNameOrRef);
    if (NS_WARN_IF(!jsObjectClassName))
      return false;
  }

  const char* scriptFilename = nullptr;
  if (node.ScriptFilenameOrRef_case() != protobuf::Node::SCRIPTFILENAMEORREF_NOT_SET) {
    Maybe<StringOrRef> scriptFilenameOrRef = GET_STRING_OR_REF(node, scriptfilename);
    scriptFilename = getOrInternString<char>(internedOneByteStrings, scriptFilenameOrRef);
    if (NS_WARN_IF(!scriptFilename))
      return false;
  }

  if (NS_WARN_IF(!nodes.putNew(id, DeserializedNode(id, coarseType, typeName,
                                                    size, Move(edges),
                                                    allocationStack,
                                                    jsObjectClassName,
                                                    scriptFilename, *this))))
  {
    return false;
  };

  return true;
}

bool
HeapSnapshot::saveStackFrame(const protobuf::StackFrame& frame,
                             StackFrameId& outFrameId)
{
  // NB: de-duplicated string properties must be read in the same order here as
  // they are written in `CoreDumpWriter::getProtobufStackFrame` or else indices
  // in references to already serialized strings will be off.

  if (frame.has_ref()) {
    // We should only get a reference to the previous frame if we have already
    // seen the previous frame.
    if (!frames.has(frame.ref()))
      return false;

    outFrameId = frame.ref();
    return true;
  }

  // Incomplete message.
  if (!frame.has_data())
    return false;

  auto data = frame.data();

  if (!data.has_id())
    return false;
  StackFrameId id = data.id();

  // This should be the first and only time we see this frame.
  if (frames.has(id))
    return false;

  if (!data.has_line())
    return false;
  uint32_t line = data.line();

  if (!data.has_column())
    return false;
  uint32_t column = data.column();

  if (!data.has_issystem())
    return false;
  bool isSystem = data.issystem();

  if (!data.has_isselfhosted())
    return false;
  bool isSelfHosted = data.isselfhosted();

  Maybe<StringOrRef> sourceOrRef = GET_STRING_OR_REF(data, source);
  auto source = getOrInternString<char16_t>(internedTwoByteStrings, sourceOrRef);
  if (!source)
    return false;

  const char16_t* functionDisplayName = nullptr;
  if (data.FunctionDisplayNameOrRef_case() !=
      protobuf::StackFrame_Data::FUNCTIONDISPLAYNAMEORREF_NOT_SET)
  {
    Maybe<StringOrRef> nameOrRef = GET_STRING_OR_REF(data, functiondisplayname);
    functionDisplayName = getOrInternString<char16_t>(internedTwoByteStrings, nameOrRef);
    if (!functionDisplayName)
      return false;
  }

  Maybe<StackFrameId> parent;
  if (data.has_parent()) {
    StackFrameId parentId = 0;
    if (!saveStackFrame(data.parent(), parentId))
      return false;
    parent = Some(parentId);
  }

  if (!frames.putNew(id, DeserializedStackFrame(id, parent, line, column,
                                                source, functionDisplayName,
                                                isSystem, isSelfHosted, *this)))
  {
    return false;
  }

  outFrameId = id;
  return true;
}

static inline bool
StreamHasData(GzipInputStream& stream)
{
  // Test for the end of the stream. The protobuf library gives no way to tell
  // the difference between an underlying read error and the stream being
  // done. All we can do is attempt to read data and extrapolate guestimations
  // from the result of that operation.

  const void* buf;
  int size;
  bool more = stream.Next(&buf, &size);
  if (!more)
    // Could not read any more data. We are optimistic and assume the stream is
    // just exhausted and there is not an underlying IO error, since this
    // function is only called at message boundaries.
    return false;

  // There is more data still available in the stream. Return the data we read
  // to the stream and let the parser get at it.
  stream.BackUp(size);
  return true;
}

bool
HeapSnapshot::init(JSContext* cx, const uint8_t* buffer, uint32_t size)
{
  if (!nodes.init() || !frames.init())
    return false;

  ArrayInputStream stream(buffer, size);
  GzipInputStream gzipStream(&stream);

  // First is the metadata.

  protobuf::Metadata metadata;
  if (!parseMessage(gzipStream, metadata))
    return false;
  if (metadata.has_timestamp())
    timestamp.emplace(metadata.timestamp());

  // Next is the root node.

  protobuf::Node root;
  if (!parseMessage(gzipStream, root))
    return false;

  // Although the id is optional in the protobuf format for future proofing, we
  // can't currently do anything without it.
  if (NS_WARN_IF(!root.has_id()))
    return false;
  rootId = root.id();

  // The set of all node ids we've found edges pointing to.
  NodeIdSet edgeReferents(cx);
  if (NS_WARN_IF(!edgeReferents.init()))
    return false;

  if (NS_WARN_IF(!saveNode(root, edgeReferents)))
    return false;

  // Finally, the rest of the nodes in the core dump.

  while (StreamHasData(gzipStream)) {
    protobuf::Node node;
    if (!parseMessage(gzipStream, node))
      return false;
    if (NS_WARN_IF(!saveNode(node, edgeReferents)))
      return false;
  }

  // Check the set of node ids referred to by edges we found and ensure that we
  // have the node corresponding to each id. If we don't have all of them, it is
  // unsafe to perform analyses of this heap snapshot.
  for (auto range = edgeReferents.all(); !range.empty(); range.popFront()) {
    if (NS_WARN_IF(!nodes.has(range.front())))
      return false;
  }

  return true;
}


/*** Heap Snapshot Analyses ***********************************************************************/

void
HeapSnapshot::TakeCensus(JSContext* cx, JS::HandleObject options,
                         JS::MutableHandleValue rval, ErrorResult& rv)
{
  JS::ubi::Census census(cx);
  if (NS_WARN_IF(!census.init())) {
    rv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  JS::ubi::CountTypePtr rootType;
  if (NS_WARN_IF(!JS::ubi::ParseCensusOptions(cx,  census, options, rootType))) {
    rv.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  JS::ubi::RootedCount rootCount(cx, rootType->makeCount());
  if (NS_WARN_IF(!rootCount)) {
    rv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  JS::ubi::CensusHandler handler(census, rootCount);

  {
    JS::AutoCheckCannotGC nogc;

    JS::ubi::CensusTraversal traversal(JS_GetRuntime(cx), handler, nogc);
    if (NS_WARN_IF(!traversal.init())) {
      rv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    if (NS_WARN_IF(!traversal.addStart(getRoot()))) {
      rv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return;
    }

    if (NS_WARN_IF(!traversal.traverse())) {
      rv.Throw(NS_ERROR_UNEXPECTED);
      return;
    }
  }

  if (NS_WARN_IF(!handler.report(rval))) {
    rv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }
}

#undef GET_STRING_OR_REF_WITH_PROP_NAMES
#undef GET_STRING_OR_REF


/*** Saving Heap Snapshots ************************************************************************/

// If we are only taking a snapshot of the heap affected by the given set of
// globals, find the set of zones the globals are allocated within. Returns
// false on OOM failure.
static bool
PopulateZonesWithGlobals(ZoneSet& zones, AutoObjectVector& globals)
{
  if (!zones.init())
    return false;

  unsigned length = globals.length();
  for (unsigned i = 0; i < length; i++) {
    if (!zones.put(GetObjectZone(globals[i])))
      return false;
  }

  return true;
}

// Add the given set of globals as explicit roots in the given roots
// list. Returns false on OOM failure.
static bool
AddGlobalsAsRoots(AutoObjectVector& globals, ubi::RootList& roots)
{
  unsigned length = globals.length();
  for (unsigned i = 0; i < length; i++) {
    if (!roots.addRoot(ubi::Node(globals[i].get()),
                       MOZ_UTF16("heap snapshot global")))
    {
      return false;
    }
  }
  return true;
}

// Choose roots and limits for a traversal, given `boundaries`. Set `roots` to
// the set of nodes within the boundaries that are referred to by nodes
// outside. If `boundaries` does not include all JS zones, initialize `zones` to
// the set of included zones; otherwise, leave `zones` uninitialized. (You can
// use zones.initialized() to check.)
//
// If `boundaries` is incoherent, or we encounter an error while trying to
// handle it, or we run out of memory, set `rv` appropriately and return
// `false`.
static bool
EstablishBoundaries(JSContext* cx,
                    ErrorResult& rv,
                    const HeapSnapshotBoundaries& boundaries,
                    ubi::RootList& roots,
                    ZoneSet& zones)
{
  MOZ_ASSERT(!roots.initialized());
  MOZ_ASSERT(!zones.initialized());

  bool foundBoundaryProperty = false;

  if (boundaries.mRuntime.WasPassed()) {
    foundBoundaryProperty = true;

    if (!boundaries.mRuntime.Value()) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }

    if (!roots.init()) {
      rv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return false;
    }
  }

  if (boundaries.mDebugger.WasPassed()) {
    if (foundBoundaryProperty) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }
    foundBoundaryProperty = true;

    JSObject* dbgObj = boundaries.mDebugger.Value();
    if (!dbgObj || !dbg::IsDebugger(*dbgObj)) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }

    AutoObjectVector globals(cx);
    if (!dbg::GetDebuggeeGlobals(cx, *dbgObj, globals) ||
        !PopulateZonesWithGlobals(zones, globals) ||
        !roots.init(zones) ||
        !AddGlobalsAsRoots(globals, roots))
    {
      rv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return false;
    }
  }

  if (boundaries.mGlobals.WasPassed()) {
    if (foundBoundaryProperty) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }
    foundBoundaryProperty = true;

    uint32_t length = boundaries.mGlobals.Value().Length();
    if (length == 0) {
      rv.Throw(NS_ERROR_INVALID_ARG);
      return false;
    }

    AutoObjectVector globals(cx);
    for (uint32_t i = 0; i < length; i++) {
      JSObject* global = boundaries.mGlobals.Value().ElementAt(i);
      if (!JS_IsGlobalObject(global)) {
        rv.Throw(NS_ERROR_INVALID_ARG);
        return false;
      }
      if (!globals.append(global)) {
        rv.Throw(NS_ERROR_OUT_OF_MEMORY);
        return false;
      }
    }

    if (!PopulateZonesWithGlobals(zones, globals) ||
        !roots.init(zones) ||
        !AddGlobalsAsRoots(globals, roots))
    {
      rv.Throw(NS_ERROR_OUT_OF_MEMORY);
      return false;
    }
  }

  if (!foundBoundaryProperty) {
    rv.Throw(NS_ERROR_INVALID_ARG);
    return false;
  }

  MOZ_ASSERT(roots.initialized());
  MOZ_ASSERT_IF(boundaries.mDebugger.WasPassed(), zones.initialized());
  MOZ_ASSERT_IF(boundaries.mGlobals.WasPassed(), zones.initialized());
  return true;
}


// A variant covering all the various two-byte strings that we can get from the
// ubi::Node API.
class TwoByteString : public Variant<JSAtom*, const char16_t*, JS::ubi::EdgeName>
{
  using Base = Variant<JSAtom*, const char16_t*, JS::ubi::EdgeName>;

  struct AsTwoByteStringMatcher
  {
    using ReturnType = TwoByteString;

    TwoByteString match(JSAtom* atom) {
      return TwoByteString(atom);
    }

    TwoByteString match(const char16_t* chars) {
      return TwoByteString(chars);
    }
  };

  struct IsNonNullMatcher
  {
    using ReturnType = bool;

    template<typename T>
    bool match(const T& t) { return t != nullptr; }
  };

  struct LengthMatcher
  {
    using ReturnType = size_t;

    size_t match(JSAtom* atom) {
      MOZ_ASSERT(atom);
      JS::ubi::AtomOrTwoByteChars s(atom);
      return s.length();
    }

    size_t match(const char16_t* chars) {
      MOZ_ASSERT(chars);
      return NS_strlen(chars);
    }

    size_t match(const JS::ubi::EdgeName& ptr) {
      MOZ_ASSERT(ptr);
      return NS_strlen(ptr.get());
    }
  };

  struct CopyToBufferMatcher
  {
    using ReturnType = size_t;

    RangedPtr<char16_t> destination;
    size_t              maxLength;

    CopyToBufferMatcher(RangedPtr<char16_t> destination, size_t maxLength)
      : destination(destination)
      , maxLength(maxLength)
    { }

    size_t match(JS::ubi::EdgeName& ptr) {
      return ptr ? match(ptr.get()) : 0;
    }

    size_t match(JSAtom* atom) {
      MOZ_ASSERT(atom);
      JS::ubi::AtomOrTwoByteChars s(atom);
      return s.copyToBuffer(destination, maxLength);
    }

    size_t match(const char16_t* chars) {
      MOZ_ASSERT(chars);
      JS::ubi::AtomOrTwoByteChars s(chars);
      return s.copyToBuffer(destination, maxLength);
    }
  };

public:
  template<typename T>
  MOZ_IMPLICIT TwoByteString(T&& rhs) : Base(Forward<T>(rhs)) { }

  template<typename T>
  TwoByteString& operator=(T&& rhs) {
    MOZ_ASSERT(this != &rhs, "self-move disallowed");
    this->~TwoByteString();
    new (this) TwoByteString(Forward<T>(rhs));
    return *this;
  }

  TwoByteString(const TwoByteString&) = delete;
  TwoByteString& operator=(const TwoByteString&) = delete;

  // Rewrap the inner value of a JS::ubi::AtomOrTwoByteChars as a TwoByteString.
  static TwoByteString from(JS::ubi::AtomOrTwoByteChars&& s) {
    AsTwoByteStringMatcher m;
    return s.match(m);
  }

  // Returns true if the given TwoByteString is non-null, false otherwise.
  bool isNonNull() const {
    IsNonNullMatcher m;
    return match(m);
  }

  // Return the length of the string, 0 if it is null.
  size_t length() const {
    LengthMatcher m;
    return match(m);
  }

  // Copy the contents of a TwoByteString into the provided buffer. The buffer
  // is NOT null terminated. The number of characters written is returned.
  size_t copyToBuffer(RangedPtr<char16_t> destination, size_t maxLength) {
    CopyToBufferMatcher m(destination, maxLength);
    return match(m);
  }

  struct HashPolicy;
};

// A hashing policy for TwoByteString.
//
// Atoms are pointer hashed and use pointer equality, which means that we
// tolerate some duplication across atoms and the other two types of two-byte
// strings. In practice, we expect the amount of this duplication to be very low
// because each type is generally a different semantic thing in addition to
// having a slightly different representation. For example, the set of edge
// names and the set stack frames' source names naturally tend not to overlap
// very much if at all.
struct TwoByteString::HashPolicy {
  using Lookup = TwoByteString;

  struct HashingMatcher {
    using ReturnType  = js::HashNumber;

    js::HashNumber match(const JSAtom* atom) {
      return js::DefaultHasher<const JSAtom*>::hash(atom);
    }

    js::HashNumber match(const char16_t* chars) {
      MOZ_ASSERT(chars);
      auto length = NS_strlen(chars);
      return HashString(chars, length);
    }

    js::HashNumber match(const JS::ubi::EdgeName& ptr) {
      MOZ_ASSERT(ptr);
      return match(ptr.get());
    }
  };

  static js::HashNumber hash(const Lookup& l) {
    HashingMatcher hasher;
    return l.match(hasher);
  }

  struct EqualityMatcher {
    using ReturnType = bool;
    const TwoByteString& rhs;
    explicit EqualityMatcher(const TwoByteString& rhs) : rhs(rhs) { }

    bool match(const JSAtom* atom) {
      return rhs.is<JSAtom*>() && rhs.as<JSAtom*>() == atom;
    }

    bool match(const char16_t* chars) {
      MOZ_ASSERT(chars);

      const char16_t* rhsChars = nullptr;
      if (rhs.is<const char16_t*>())
        rhsChars = rhs.as<const char16_t*>();
      else if (rhs.is<JS::ubi::EdgeName>())
        rhsChars = rhs.as<JS::ubi::EdgeName>().get();
      else
        return false;
      MOZ_ASSERT(rhsChars);

      auto length = NS_strlen(chars);
      if (NS_strlen(rhsChars) != length)
        return false;

      return memcmp(chars, rhsChars, length * sizeof(char16_t)) == 0;
    }

    bool match(const JS::ubi::EdgeName& ptr) {
      MOZ_ASSERT(ptr);
      return match(ptr.get());
    }
  };

  static bool match(const TwoByteString& k, const Lookup& l) {
    EqualityMatcher eq(l);
    return k.match(eq);
  }

  static void rekey(TwoByteString& k, TwoByteString&& newKey) {
    k = Move(newKey);
  }
};

// A `CoreDumpWriter` that serializes nodes to protobufs and writes them to the
// given `ZeroCopyOutputStream`.
class MOZ_STACK_CLASS StreamWriter : public CoreDumpWriter
{
  using FrameSet         = js::HashSet<uint64_t>;
  using TwoByteStringMap = js::HashMap<TwoByteString, uint64_t, TwoByteString::HashPolicy>;
  using OneByteStringMap = js::HashMap<const char*, uint64_t>;

  JSContext*       cx;
  bool             wantNames;
  // The set of |JS::ubi::StackFrame::identifier()|s that have already been
  // serialized and written to the core dump.
  FrameSet         framesAlreadySerialized;
  // The set of two-byte strings that have already been serialized and written
  // to the core dump.
  TwoByteStringMap twoByteStringsAlreadySerialized;
  // The set of one-byte strings that have already been serialized and written
  // to the core dump.
  OneByteStringMap oneByteStringsAlreadySerialized;

  ::google::protobuf::io::ZeroCopyOutputStream& stream;

  bool writeMessage(const ::google::protobuf::MessageLite& message) {
    // We have to create a new CodedOutputStream when writing each message so
    // that the 64MB size limit used by Coded{Output,Input}Stream to prevent
    // integer overflow is enforced per message rather than on the whole stream.
    ::google::protobuf::io::CodedOutputStream codedStream(&stream);
    codedStream.WriteVarint32(message.ByteSize());
    message.SerializeWithCachedSizes(&codedStream);
    return !codedStream.HadError();
  }

  // Attach the full two-byte string or a reference to a two-byte string that
  // has already been serialized to a protobuf message.
  template <typename SetStringFunction,
            typename SetRefFunction>
  bool attachTwoByteString(TwoByteString& string, SetStringFunction setString,
                           SetRefFunction setRef) {
    auto ptr = twoByteStringsAlreadySerialized.lookupForAdd(string);
    if (ptr) {
      setRef(ptr->value());
      return true;
    }

    auto length = string.length();
    auto stringData = MakeUnique<std::string>(length * sizeof(char16_t), '\0');
    if (!stringData)
      return false;

    auto buf = const_cast<char16_t*>(reinterpret_cast<const char16_t*>(stringData->data()));
    string.copyToBuffer(RangedPtr<char16_t>(buf, length), length);

    uint64_t ref = twoByteStringsAlreadySerialized.count();
    if (!twoByteStringsAlreadySerialized.add(ptr, Move(string), ref))
      return false;

    setString(stringData.release());
    return true;
  }

  // Attach the full one-byte string or a reference to a one-byte string that
  // has already been serialized to a protobuf message.
  template <typename SetStringFunction,
            typename SetRefFunction>
  bool attachOneByteString(const char* string, SetStringFunction setString,
                           SetRefFunction setRef) {
    auto ptr = oneByteStringsAlreadySerialized.lookupForAdd(string);
    if (ptr) {
      setRef(ptr->value());
      return true;
    }

    auto length = strlen(string);
    auto stringData = MakeUnique<std::string>(string, length);
    if (!stringData)
      return false;

    uint64_t ref = oneByteStringsAlreadySerialized.count();
    if (!oneByteStringsAlreadySerialized.add(ptr, string, ref))
      return false;

    setString(stringData.release());
    return true;
  }

  protobuf::StackFrame* getProtobufStackFrame(JS::ubi::StackFrame& frame,
                                              size_t depth = 1) {
    // NB: de-duplicated string properties must be written in the same order
    // here as they are read in `HeapSnapshot::saveStackFrame` or else indices
    // in references to already serialized strings will be off.

    MOZ_ASSERT(frame,
               "null frames should be represented as the lack of a serialized "
               "stack frame");

    auto id = frame.identifier();
    auto protobufStackFrame = MakeUnique<protobuf::StackFrame>();
    if (!protobufStackFrame)
      return nullptr;

    if (framesAlreadySerialized.has(id)) {
      protobufStackFrame->set_ref(id);
      return protobufStackFrame.release();
    }

    auto data = MakeUnique<protobuf::StackFrame_Data>();
    if (!data)
      return nullptr;

    data->set_id(id);
    data->set_line(frame.line());
    data->set_column(frame.column());
    data->set_issystem(frame.isSystem());
    data->set_isselfhosted(frame.isSelfHosted());

    auto dupeSource = TwoByteString::from(frame.source());
    if (!attachTwoByteString(dupeSource,
                             [&] (std::string* source) { data->set_allocated_source(source); },
                             [&] (uint64_t ref) { data->set_sourceref(ref); }))
    {
      return nullptr;
    }

    auto dupeName = TwoByteString::from(frame.functionDisplayName());
    if (dupeName.isNonNull()) {
      if (!attachTwoByteString(dupeName,
                               [&] (std::string* name) { data->set_allocated_functiondisplayname(name); },
                               [&] (uint64_t ref) { data->set_functiondisplaynameref(ref); }))
      {
        return nullptr;
      }
    }

    auto parent = frame.parent();
    if (parent && depth < HeapSnapshot::MAX_STACK_DEPTH) {
      auto protobufParent = getProtobufStackFrame(parent, depth + 1);
      if (!protobufParent)
        return nullptr;
      data->set_allocated_parent(protobufParent);
    }

    protobufStackFrame->set_allocated_data(data.release());

    if (!framesAlreadySerialized.put(id))
      return nullptr;

    return protobufStackFrame.release();
  }

public:
  StreamWriter(JSContext* cx,
               ::google::protobuf::io::ZeroCopyOutputStream& stream,
               bool wantNames)
    : cx(cx)
    , wantNames(wantNames)
    , framesAlreadySerialized(cx)
    , twoByteStringsAlreadySerialized(cx)
    , oneByteStringsAlreadySerialized(cx)
    , stream(stream)
  { }

  bool init() {
    return framesAlreadySerialized.init() &&
           twoByteStringsAlreadySerialized.init() &&
           oneByteStringsAlreadySerialized.init();
  }

  ~StreamWriter() override { }

  virtual bool writeMetadata(uint64_t timestamp) final {
    protobuf::Metadata metadata;
    metadata.set_timestamp(timestamp);
    return writeMessage(metadata);
  }

  virtual bool writeNode(const JS::ubi::Node& ubiNode,
                         EdgePolicy includeEdges) final {
    // NB: de-duplicated string properties must be written in the same order
    // here as they are read in `HeapSnapshot::saveNode` or else indices in
    // references to already serialized strings will be off.

    protobuf::Node protobufNode;
    protobufNode.set_id(ubiNode.identifier());

    protobufNode.set_coarsetype(JS::ubi::CoarseTypeToUint32(ubiNode.coarseType()));

    auto typeName = TwoByteString(ubiNode.typeName());
    if (NS_WARN_IF(!attachTwoByteString(typeName,
                                        [&] (std::string* name) { protobufNode.set_allocated_typename_(name); },
                                        [&] (uint64_t ref) { protobufNode.set_typenameref(ref); })))
    {
      return false;
    }

    JSRuntime* rt = JS_GetRuntime(cx);
    mozilla::MallocSizeOf mallocSizeOf = dbg::GetDebuggerMallocSizeOf(rt);
    MOZ_ASSERT(mallocSizeOf);
    protobufNode.set_size(ubiNode.size(mallocSizeOf));

    if (includeEdges) {
      auto edges = ubiNode.edges(JS_GetRuntime(cx), wantNames);
      if (NS_WARN_IF(!edges))
        return false;

      for ( ; !edges->empty(); edges->popFront()) {
        ubi::Edge& ubiEdge = edges->front();

        protobuf::Edge* protobufEdge = protobufNode.add_edges();
        if (NS_WARN_IF(!protobufEdge)) {
          return false;
        }

        protobufEdge->set_referent(ubiEdge.referent.identifier());

        if (wantNames && ubiEdge.name) {
          TwoByteString edgeName(Move(ubiEdge.name));
          if (NS_WARN_IF(!attachTwoByteString(edgeName,
                                              [&] (std::string* name) { protobufEdge->set_allocated_name(name); },
                                              [&] (uint64_t ref) { protobufEdge->set_nameref(ref); })))
          {
            return false;
          }
        }
      }
    }

    if (ubiNode.hasAllocationStack()) {
      auto ubiStackFrame = ubiNode.allocationStack();
      auto protoStackFrame = getProtobufStackFrame(ubiStackFrame);
      if (NS_WARN_IF(!protoStackFrame))
        return false;
      protobufNode.set_allocated_allocationstack(protoStackFrame);
    }

    if (auto className = ubiNode.jsObjectClassName()) {
      if (NS_WARN_IF(!attachOneByteString(className,
                                          [&] (std::string* name) { protobufNode.set_allocated_jsobjectclassname(name); },
                                          [&] (uint64_t ref) { protobufNode.set_jsobjectclassnameref(ref); })))
      {
        return false;
      }
    }

    if (auto scriptFilename = ubiNode.scriptFilename()) {
      if (NS_WARN_IF(!attachOneByteString(scriptFilename,
                                          [&] (std::string* name) { protobufNode.set_allocated_scriptfilename(name); },
                                          [&] (uint64_t ref) { protobufNode.set_scriptfilenameref(ref); })))
      {
        return false;
      }
    }

    return writeMessage(protobufNode);
  }
};

// A JS::ubi::BreadthFirst handler that serializes a snapshot of the heap into a
// core dump.
class MOZ_STACK_CLASS HeapSnapshotHandler
{
  CoreDumpWriter& writer;
  JS::ZoneSet*    zones;

public:
  // For telemetry.
  uint32_t nodeCount;
  uint32_t edgeCount;

  HeapSnapshotHandler(CoreDumpWriter& writer,
                      JS::ZoneSet* zones)
    : writer(writer),
      zones(zones)
  { }

  // JS::ubi::BreadthFirst handler interface.

  class NodeData { };
  typedef JS::ubi::BreadthFirst<HeapSnapshotHandler> Traversal;
  bool operator() (Traversal& traversal,
                   JS::ubi::Node origin,
                   const JS::ubi::Edge& edge,
                   NodeData*,
                   bool first)
  {
    edgeCount++;

    // We're only interested in the first time we reach edge.referent, not in
    // every edge arriving at that node. "But, don't we want to serialize every
    // edge in the heap graph?" you ask. Don't worry! This edge is still
    // serialized into the core dump. Serializing a node also serializes each of
    // its edges, and if we are traversing a given edge, we must have already
    // visited and serialized the origin node and its edges.
    if (!first)
      return true;

    nodeCount++;

    const JS::ubi::Node& referent = edge.referent;

    if (!zones)
      // We aren't targeting a particular set of zones, so serialize all the
      // things!
      return writer.writeNode(referent, CoreDumpWriter::INCLUDE_EDGES);

    // We are targeting a particular set of zones. If this node is in our target
    // set, serialize it and all of its edges. If this node is _not_ in our
    // target set, we also serialize under the assumption that it is a shared
    // resource being used by something in our target zones since we reached it
    // by traversing the heap graph. However, we do not serialize its outgoing
    // edges and we abandon further traversal from this node.

    JS::Zone* zone = referent.zone();

    if (zones->has(zone))
      return writer.writeNode(referent, CoreDumpWriter::INCLUDE_EDGES);

    traversal.abandonReferent();
    return writer.writeNode(referent, CoreDumpWriter::EXCLUDE_EDGES);
  }
};


bool
WriteHeapGraph(JSContext* cx,
               const JS::ubi::Node& node,
               CoreDumpWriter& writer,
               bool wantNames,
               JS::ZoneSet* zones,
               JS::AutoCheckCannotGC& noGC,
               uint32_t& outNodeCount,
               uint32_t& outEdgeCount)
{
  // Serialize the starting node to the core dump.

  if (NS_WARN_IF(!writer.writeNode(node, CoreDumpWriter::INCLUDE_EDGES))) {
    return false;
  }

  // Walk the heap graph starting from the given node and serialize it into the
  // core dump.

  HeapSnapshotHandler handler(writer, zones);
  HeapSnapshotHandler::Traversal traversal(JS_GetRuntime(cx), handler, noGC);
  if (!traversal.init())
    return false;
  traversal.wantNames = wantNames;

  bool ok = traversal.addStartVisited(node) &&
            traversal.traverse();

  if (ok) {
    outNodeCount = handler.nodeCount;
    outEdgeCount = handler.edgeCount;
  }

  return ok;
}

static unsigned long
msSinceProcessCreation(const TimeStamp& now)
{
  bool ignored;
  auto duration = now - TimeStamp::ProcessCreation(ignored);
  return (unsigned long) duration.ToMilliseconds();
}

/* static */ already_AddRefed<nsIFile>
HeapSnapshot::CreateUniqueCoreDumpFile(ErrorResult& rv,
                                       const TimeStamp& now,
                                       nsAString& outFilePath)
{
  nsCOMPtr<nsIFile> file;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(file));
  if (NS_WARN_IF(rv.Failed()))
    return nullptr;

  auto ms = msSinceProcessCreation(now);
  rv = file->AppendNative(nsPrintfCString("%lu.fxsnapshot", ms));
  if (NS_WARN_IF(rv.Failed()))
    return nullptr;

  rv = file->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0666);
  if (NS_WARN_IF(rv.Failed()))
    return nullptr;

  rv = file->GetPath(outFilePath);
  if (NS_WARN_IF(rv.Failed()))
    return nullptr;

  return file.forget();
}

// Deletion policy for cleaning up PHeapSnapshotTempFileHelperChild pointers.
class DeleteHeapSnapshotTempFileHelperChild
{
public:
  MOZ_CONSTEXPR DeleteHeapSnapshotTempFileHelperChild() { }

  void operator()(PHeapSnapshotTempFileHelperChild* ptr) const {
    NS_WARN_IF(!HeapSnapshotTempFileHelperChild::Send__delete__(ptr));
  }
};

// A UniquePtr alias to automatically manage PHeapSnapshotTempFileHelperChild
// pointers.
using UniqueHeapSnapshotTempFileHelperChild = UniquePtr<PHeapSnapshotTempFileHelperChild,
                                                        DeleteHeapSnapshotTempFileHelperChild>;

// Get an nsIOutputStream that we can write the heap snapshot to. In non-e10s
// and in the e10s parent process, open a file directly and create an output
// stream for it. In e10s child processes, we are sandboxed without access to
// the filesystem. Use IPDL to request a file descriptor from the parent
// process.
static already_AddRefed<nsIOutputStream>
getCoreDumpOutputStream(ErrorResult& rv, TimeStamp& start, nsAString& outFilePath)
{
  if (XRE_IsParentProcess()) {
    // Create the file and open the output stream directly.

    nsCOMPtr<nsIFile> file = HeapSnapshot::CreateUniqueCoreDumpFile(rv,
                                                                    start,
                                                                    outFilePath);
    if (NS_WARN_IF(rv.Failed()))
      return nullptr;

    nsCOMPtr<nsIOutputStream> outputStream;
    rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), file,
                                     PR_WRONLY, -1, 0);
    if (NS_WARN_IF(rv.Failed()))
      return nullptr;

    return outputStream.forget();
  } else {
    // Request a file descriptor from the parent process over IPDL.

    auto cc = ContentChild::GetSingleton();
    if (!cc) {
      rv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    UniqueHeapSnapshotTempFileHelperChild helper(
      cc->SendPHeapSnapshotTempFileHelperConstructor());
    if (NS_WARN_IF(!helper)) {
      rv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    OpenHeapSnapshotTempFileResponse response;
    if (!helper->SendOpenHeapSnapshotTempFile(&response)) {
      rv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }
    if (response.type() == OpenHeapSnapshotTempFileResponse::Tnsresult) {
      rv.Throw(response.get_nsresult());
      return nullptr;
    }

    auto opened = response.get_OpenedFile();
    outFilePath = opened.path();
    nsCOMPtr<nsIOutputStream> outputStream =
      FileDescriptorOutputStream::Create(opened.descriptor());
    if (NS_WARN_IF(!outputStream)) {
      rv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    return outputStream.forget();
  }
}

} // namespace devtools

namespace dom {

using namespace JS;
using namespace devtools;

/* static */ void
ThreadSafeChromeUtils::SaveHeapSnapshot(GlobalObject& global,
                                        JSContext* cx,
                                        const HeapSnapshotBoundaries& boundaries,
                                        nsAString& outFilePath,
                                        ErrorResult& rv)
{
  auto start = TimeStamp::Now();

  bool wantNames = true;
  ZoneSet zones;
  uint32_t nodeCount = 0;
  uint32_t edgeCount = 0;

  nsCOMPtr<nsIOutputStream> outputStream = getCoreDumpOutputStream(rv, start, outFilePath);
  if (NS_WARN_IF(rv.Failed()))
    return;

  ZeroCopyNSIOutputStream zeroCopyStream(outputStream);
  ::google::protobuf::io::GzipOutputStream gzipStream(&zeroCopyStream);

  StreamWriter writer(cx, gzipStream, wantNames);
  if (NS_WARN_IF(!writer.init())) {
    rv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return;
  }

  {
    Maybe<AutoCheckCannotGC> maybeNoGC;
    ubi::RootList rootList(JS_GetRuntime(cx), maybeNoGC, wantNames);
    if (!EstablishBoundaries(cx, rv, boundaries, rootList, zones))
      return;

    MOZ_ASSERT(maybeNoGC.isSome());
    ubi::Node roots(&rootList);

    // Serialize the initial heap snapshot metadata to the core dump.
    if (!writer.writeMetadata(PR_Now()) ||
        // Serialize the heap graph to the core dump, starting from our list of
        // roots.
        !WriteHeapGraph(cx,
                        roots,
                        writer,
                        wantNames,
                        zones.initialized() ? &zones : nullptr,
                        maybeNoGC.ref(),
                        nodeCount,
                        edgeCount))
    {
      rv.Throw(zeroCopyStream.failed()
               ? zeroCopyStream.result()
               : NS_ERROR_UNEXPECTED);
      return;
    }
  }

  Telemetry::AccumulateTimeDelta(Telemetry::DEVTOOLS_SAVE_HEAP_SNAPSHOT_MS,
                                 start);
  Telemetry::Accumulate(Telemetry::DEVTOOLS_HEAP_SNAPSHOT_NODE_COUNT,
                        nodeCount);
  Telemetry::Accumulate(Telemetry::DEVTOOLS_HEAP_SNAPSHOT_EDGE_COUNT,
                        edgeCount);
}

/* static */ already_AddRefed<HeapSnapshot>
ThreadSafeChromeUtils::ReadHeapSnapshot(GlobalObject& global,
                                        JSContext* cx,
                                        const nsAString& filePath,
                                        ErrorResult& rv)
{
  auto start = TimeStamp::Now();

  UniquePtr<char[]> path(ToNewCString(filePath));
  if (!path) {
    rv.Throw(NS_ERROR_OUT_OF_MEMORY);
    return nullptr;
  }

  AutoMemMap mm;
  rv = mm.init(path.get());
  if (rv.Failed())
    return nullptr;

  RefPtr<HeapSnapshot> snapshot = HeapSnapshot::Create(
      cx, global, reinterpret_cast<const uint8_t*>(mm.address()), mm.size(), rv);

  if (!rv.Failed())
    Telemetry::AccumulateTimeDelta(Telemetry::DEVTOOLS_READ_HEAP_SNAPSHOT_MS,
                                   start);

  return snapshot.forget();
}

} // namespace dom
} // namespace mozilla
