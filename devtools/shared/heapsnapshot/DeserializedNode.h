/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_DeserializedNode__
#define mozilla_devtools_DeserializedNode__

#include <utility>

#include "js/UbiNode.h"
#include "js/UniquePtr.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Maybe.h"
#include "mozilla/Vector.h"
#include "mozilla/devtools/CoreDump.pb.h"

// `Deserialized{Node,Edge}` translate protobuf messages from our core dump
// format into structures we can rely upon for implementing `JS::ubi::Node`
// specializations on top of. All of the properties of the protobuf messages are
// optional for future compatibility, and this is the layer where we validate
// that the properties that do actually exist in any given message fulfill our
// semantic requirements.
//
// Both `DeserializedNode` and `DeserializedEdge` are always owned by a
// `HeapSnapshot` instance, and their lifetimes must not extend after that of
// their owning `HeapSnapshot`.

namespace mozilla {
namespace devtools {

class HeapSnapshot;

using NodeId = uint64_t;
using StackFrameId = uint64_t;

// A `DeserializedEdge` represents an edge in the heap graph pointing to the
// node with id equal to `DeserializedEdge::referent` that we deserialized from
// a core dump.
struct DeserializedEdge {
  NodeId referent;
  // A borrowed reference to a string owned by this node's owning HeapSnapshot.
  const char16_t* name;

  explicit DeserializedEdge(NodeId referent, const char16_t* edgeName = nullptr)
      : referent(referent), name(edgeName) {}
  DeserializedEdge(DeserializedEdge&& rhs);
  DeserializedEdge& operator=(DeserializedEdge&& rhs);

 private:
  DeserializedEdge(const DeserializedEdge&) = delete;
  DeserializedEdge& operator=(const DeserializedEdge&) = delete;
};

// A `DeserializedNode` is a node in the heap graph that we deserialized from a
// core dump.
struct DeserializedNode {
  using EdgeVector = Vector<DeserializedEdge>;
  using UniqueStringPtr = UniquePtr<char16_t[]>;

  NodeId id;
  JS::ubi::CoarseType coarseType;
  // A borrowed reference to a string owned by this node's owning HeapSnapshot.
  const char16_t* typeName;
  uint64_t size;
  EdgeVector edges;
  Maybe<StackFrameId> allocationStack;
  // A borrowed reference to a string owned by this node's owning HeapSnapshot.
  const char* jsObjectClassName;
  // A borrowed reference to a string owned by this node's owning HeapSnapshot.
  const char* scriptFilename;
  // A borrowed reference to a string owned by this node's owning HeapSnapshot.
  const char16_t* descriptiveTypeName;
  // A weak pointer to this node's owning `HeapSnapshot`. Safe without
  // AddRef'ing because this node's lifetime is equal to that of its owner.
  HeapSnapshot* owner;

  DeserializedNode(NodeId id, JS::ubi::CoarseType coarseType,
                   const char16_t* typeName, uint64_t size, EdgeVector&& edges,
                   const Maybe<StackFrameId>& allocationStack,
                   const char* className, const char* filename,
                   const char16_t* descriptiveName, HeapSnapshot& owner)
      : id(id),
        coarseType(coarseType),
        typeName(typeName),
        size(size),
        edges(std::move(edges)),
        allocationStack(allocationStack),
        jsObjectClassName(className),
        scriptFilename(filename),
        descriptiveTypeName(descriptiveName),
        owner(&owner) {}
  virtual ~DeserializedNode() {}

  DeserializedNode(DeserializedNode&& rhs)
      : id(rhs.id),
        coarseType(rhs.coarseType),
        typeName(rhs.typeName),
        size(rhs.size),
        edges(std::move(rhs.edges)),
        allocationStack(rhs.allocationStack),
        jsObjectClassName(rhs.jsObjectClassName),
        scriptFilename(rhs.scriptFilename),
        descriptiveTypeName(rhs.descriptiveTypeName),
        owner(rhs.owner) {}

  DeserializedNode& operator=(DeserializedNode&& rhs) {
    MOZ_ASSERT(&rhs != this);
    this->~DeserializedNode();
    new (this) DeserializedNode(std::move(rhs));
    return *this;
  }

  // Get a borrowed reference to the given edge's referent. This method is
  // virtual to provide a hook for gmock and gtest.
  virtual JS::ubi::Node getEdgeReferent(const DeserializedEdge& edge);

  struct HashPolicy;

 protected:
  // This is only for use with `MockDeserializedNode` in testing.
  DeserializedNode(NodeId id, const char16_t* typeName, uint64_t size)
      : id(id),
        coarseType(JS::ubi::CoarseType::Other),
        typeName(typeName),
        size(size),
        edges(),
        allocationStack(Nothing()),
        jsObjectClassName(nullptr),
        scriptFilename(nullptr),
        descriptiveTypeName(nullptr),
        owner(nullptr) {}

 private:
  DeserializedNode(const DeserializedNode&) = delete;
  DeserializedNode& operator=(const DeserializedNode&) = delete;
};

static inline js::HashNumber hashIdDerivedFromPtr(uint64_t id) {
  return mozilla::HashGeneric(id);
}

struct DeserializedNode::HashPolicy {
  using Lookup = NodeId;

  static js::HashNumber hash(const Lookup& lookup) {
    return hashIdDerivedFromPtr(lookup);
  }

  static bool match(const DeserializedNode& existing, const Lookup& lookup) {
    return existing.id == lookup;
  }
};

// A `DeserializedStackFrame` is a stack frame referred to by a thing in the
// heap graph that we deserialized from a core dump.
struct DeserializedStackFrame {
  StackFrameId id;
  Maybe<StackFrameId> parent;
  uint32_t line;
  uint32_t column;
  // Borrowed references to strings owned by this DeserializedStackFrame's
  // owning HeapSnapshot.
  const char16_t* source;
  const char16_t* functionDisplayName;
  bool isSystem;
  bool isSelfHosted;
  // A weak pointer to this frame's owning `HeapSnapshot`. Safe without
  // AddRef'ing because this frame's lifetime is equal to that of its owner.
  HeapSnapshot* owner;

  explicit DeserializedStackFrame(StackFrameId id,
                                  const Maybe<StackFrameId>& parent,
                                  uint32_t line, uint32_t column,
                                  const char16_t* source,
                                  const char16_t* functionDisplayName,
                                  bool isSystem, bool isSelfHosted,
                                  HeapSnapshot& owner)
      : id(id),
        parent(parent),
        line(line),
        column(column),
        source(source),
        functionDisplayName(functionDisplayName),
        isSystem(isSystem),
        isSelfHosted(isSelfHosted),
        owner(&owner) {
    MOZ_ASSERT(source);
  }

  JS::ubi::StackFrame getParentStackFrame() const;

  struct HashPolicy;

 protected:
  // This is exposed only for MockDeserializedStackFrame in the gtests.
  explicit DeserializedStackFrame()
      : id(0),
        parent(Nothing()),
        line(0),
        column(0),
        source(nullptr),
        functionDisplayName(nullptr),
        isSystem(false),
        isSelfHosted(false),
        owner(nullptr){};
};

struct DeserializedStackFrame::HashPolicy {
  using Lookup = StackFrameId;

  static js::HashNumber hash(const Lookup& lookup) {
    return hashIdDerivedFromPtr(lookup);
  }

  static bool match(const DeserializedStackFrame& existing,
                    const Lookup& lookup) {
    return existing.id == lookup;
  }
};

}  // namespace devtools
}  // namespace mozilla

namespace JS {
namespace ubi {

using mozilla::devtools::DeserializedNode;
using mozilla::devtools::DeserializedStackFrame;

template <>
class Concrete<DeserializedNode> : public Base {
 protected:
  explicit Concrete(DeserializedNode* ptr) : Base(ptr) {}
  DeserializedNode& get() const { return *static_cast<DeserializedNode*>(ptr); }

 public:
  static void construct(void* storage, DeserializedNode* ptr) {
    new (storage) Concrete(ptr);
  }

  CoarseType coarseType() const final { return get().coarseType; }
  Id identifier() const override { return get().id; }
  bool isLive() const override { return false; }
  const char16_t* typeName() const override;
  Node::Size size(mozilla::MallocSizeOf mallocSizeof) const override;
  const char* jsObjectClassName() const override {
    return get().jsObjectClassName;
  }
  const char* scriptFilename() const final { return get().scriptFilename; }
  const char16_t* descriptiveTypeName() const override {
    return get().descriptiveTypeName;
  }

  bool hasAllocationStack() const override {
    return get().allocationStack.isSome();
  }
  StackFrame allocationStack() const override;

  // We ignore the `bool wantNames` parameter because we can't control whether
  // the core dump was serialized with edge names or not.
  js::UniquePtr<EdgeRange> edges(JSContext* cx, bool) const override;

  static const char16_t concreteTypeName[];
};

template <>
class ConcreteStackFrame<DeserializedStackFrame> : public BaseStackFrame {
 protected:
  explicit ConcreteStackFrame(DeserializedStackFrame* ptr)
      : BaseStackFrame(ptr) {}

  DeserializedStackFrame& get() const {
    return *static_cast<DeserializedStackFrame*>(ptr);
  }

 public:
  static void construct(void* storage, DeserializedStackFrame* ptr) {
    new (storage) ConcreteStackFrame(ptr);
  }

  uint64_t identifier() const override { return get().id; }
  uint32_t line() const override { return get().line; }
  uint32_t column() const override { return get().column; }
  bool isSystem() const override { return get().isSystem; }
  bool isSelfHosted(JSContext* cx) const override { return get().isSelfHosted; }
  void trace(JSTracer* trc) override {}
  AtomOrTwoByteChars source() const override {
    return AtomOrTwoByteChars(get().source);
  }
  uint32_t sourceId() const override {
    // Source IDs are local to their host process and are not serialized.
    return 0;
  }
  AtomOrTwoByteChars functionDisplayName() const override {
    return AtomOrTwoByteChars(get().functionDisplayName);
  }

  StackFrame parent() const override;
  bool constructSavedFrameStack(
      JSContext* cx, MutableHandleObject outSavedFrameStack) const override;
};

}  // namespace ubi
}  // namespace JS

#endif  // mozilla_devtools_DeserializedNode__
