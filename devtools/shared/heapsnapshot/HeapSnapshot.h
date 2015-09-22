/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_HeapSnapshot__
#define mozilla_devtools_HeapSnapshot__

#include "js/HashTable.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/devtools/DeserializedNode.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

#include "CoreDump.pb.h"
#include "nsCOMPtr.h"
#include "nsCRTGlue.h"
#include "nsCycleCollectionParticipant.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "nsXPCOM.h"

namespace mozilla {
namespace devtools {

struct NSFreePolicy {
  void operator()(void* ptr) {
    NS_Free(ptr);
  }
};

using UniqueString = UniquePtr<char16_t[], NSFreePolicy>;

struct UniqueStringHashPolicy {
  struct Lookup {
    const char16_t* str;
    size_t          length;

    Lookup(const char16_t* str, size_t length)
      : str(str)
      , length(length)
    { }
  };

  static js::HashNumber hash(const Lookup& lookup) {
    MOZ_ASSERT(lookup.str);
    return HashString(lookup.str, lookup.length);
  }

  static bool match(const UniqueString& existing, const Lookup& lookup) {
    MOZ_ASSERT(lookup.str);
    if (NS_strlen(existing.get()) != lookup.length)
      return false;
    return memcmp(existing.get(), lookup.str, lookup.length * sizeof(char16_t)) == 0;
  }
};

class HeapSnapshot final : public nsISupports
                         , public nsWrapperCache
{
  friend struct DeserializedNode;
  friend struct DeserializedStackFrame;
  friend struct JS::ubi::Concrete<JS::ubi::DeserializedNode>;

  explicit HeapSnapshot(JSContext* cx, nsISupports* aParent)
    : timestamp(Nothing())
    , rootId(0)
    , nodes(cx)
    , frames(cx)
    , strings(cx)
    , mParent(aParent)
  {
    MOZ_ASSERT(aParent);
  };

  // Initialize this HeapSnapshot from the given buffer that contains a
  // serialized core dump. Do NOT take ownership of the buffer, only borrow it
  // for the duration of the call. Return false on failure.
  bool init(const uint8_t* buffer, uint32_t size);

  // Save the given `protobuf::Node` message in this `HeapSnapshot` as a
  // `DeserializedNode`.
  bool saveNode(const protobuf::Node& node);

  // Save the given `protobuf::StackFrame` message in this `HeapSnapshot` as a
  // `DeserializedStackFrame`. The saved stack frame's id is returned via the
  // out parameter.
  bool saveStackFrame(const protobuf::StackFrame& frame,
                      StackFrameId& outFrameId);

  // If present, a timestamp in the same units that `PR_Now` gives.
  Maybe<uint64_t> timestamp;

  // The id of the root node for this deserialized heap graph.
  NodeId rootId;

  // The set of nodes in this deserialized heap graph, keyed by id.
  using NodeSet = js::HashSet<DeserializedNode, DeserializedNode::HashPolicy>;
  NodeSet nodes;

  // The set of stack frames in this deserialized heap graph, keyed by id.
  using FrameSet = js::HashSet<DeserializedStackFrame,
                               DeserializedStackFrame::HashPolicy>;
  FrameSet frames;

  // Core dump files have many duplicate strings: type names are repeated for
  // each node, and although in theory edge names are highly customizable for
  // specific edges, in practice they are also highly duplicated. Rather than
  // make each Deserialized{Node,Edge} malloc their own copy of their edge and
  // type names, we de-duplicate the strings here and Deserialized{Node,Edge}
  // get borrowed pointers into this set.
  using UniqueStringSet = js::HashSet<UniqueString, UniqueStringHashPolicy>;
  UniqueStringSet strings;

protected:
  nsCOMPtr<nsISupports> mParent;

  virtual ~HeapSnapshot() { }

public:
  // Create a `HeapSnapshot` from the given buffer that contains a serialized
  // core dump. Do NOT take ownership of the buffer, only borrow it for the
  // duration of the call.
  static already_AddRefed<HeapSnapshot> Create(JSContext* cx,
                                               dom::GlobalObject& global,
                                               const uint8_t* buffer,
                                               uint32_t size,
                                               ErrorResult& rv);

  // Creates the `$TEMP_DIR/XXXXXX-XXX.fxsnapshot` core dump file that heap
  // snapshots are serialized into.
  static already_AddRefed<nsIFile> CreateUniqueCoreDumpFile(ErrorResult& rv,
                                                            const TimeStamp& now,
                                                            nsAString& outFilePath);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(HeapSnapshot)
  MOZ_DECLARE_REFCOUNTED_TYPENAME(HeapSnapshot)

  nsISupports* GetParentObject() const { return mParent; }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  const char16_t* borrowUniqueString(const char16_t* duplicateString,
                                     size_t length);

  // Get the root node of this heap snapshot's graph.
  JS::ubi::Node getRoot() {
    MOZ_ASSERT(nodes.initialized());
    auto p = nodes.lookup(rootId);
    MOZ_ASSERT(p);
    const DeserializedNode& node = *p;
    return JS::ubi::Node(const_cast<DeserializedNode*>(&node));
  }

  void TakeCensus(JSContext* cx, JS::HandleObject options,
                  JS::MutableHandleValue rval, ErrorResult& rv);
};

// A `CoreDumpWriter` is given the data we wish to save in a core dump and
// serializes it to disk, or memory, or a socket, etc.
class CoreDumpWriter
{
public:
  virtual ~CoreDumpWriter() { };

  // Write the given bits of metadata we would like to associate with this core
  // dump.
  virtual bool writeMetadata(uint64_t timestamp) = 0;

  enum EdgePolicy : bool {
    INCLUDE_EDGES = true,
    EXCLUDE_EDGES = false
  };

  // Write the given `JS::ubi::Node` to the core dump. The given `EdgePolicy`
  // dictates whether its outgoing edges should also be written to the core
  // dump, or excluded.
  virtual bool writeNode(const JS::ubi::Node& node,
                         EdgePolicy includeEdges) = 0;
};

// Serialize the heap graph as seen from `node` with the given `CoreDumpWriter`.
// If `wantNames` is true, capture edge names. If `zones` is non-null, only
// capture the sub-graph within the zone set, otherwise capture the whole heap
// graph. Returns false on failure.
bool
WriteHeapGraph(JSContext* cx,
               const JS::ubi::Node& node,
               CoreDumpWriter& writer,
               bool wantNames,
               JS::ZoneSet* zones,
               JS::AutoCheckCannotGC& noGC,
               uint32_t& outNodeCount,
               uint32_t& outEdgeCount);
inline bool
WriteHeapGraph(JSContext* cx,
               const JS::ubi::Node& node,
               CoreDumpWriter& writer,
               bool wantNames,
               JS::ZoneSet* zones,
               JS::AutoCheckCannotGC& noGC)
{
  uint32_t ignoreNodeCount;
  uint32_t ignoreEdgeCount;
  return WriteHeapGraph(cx, node, writer, wantNames, zones, noGC,
                        ignoreNodeCount, ignoreEdgeCount);
}

} // namespace devtools
} // namespace mozilla

#endif // mozilla_devtools_HeapSnapshot__
