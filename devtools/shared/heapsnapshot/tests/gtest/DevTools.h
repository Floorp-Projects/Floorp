/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_gtest_DevTools__
#define mozilla_devtools_gtest_DevTools__

#include <utility>

#include "CoreDump.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "js/Principals.h"
#include "js/UbiNode.h"
#include "js/UniquePtr.h"
#include "jsapi.h"
#include "jspubtd.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/devtools/HeapSnapshot.h"
#include "mozilla/dom/ChromeUtils.h"
#include "nsCRTGlue.h"

using namespace mozilla;
using namespace mozilla::devtools;
using namespace mozilla::dom;
using namespace testing;

// GTest fixture class that all of our tests derive from.
struct DevTools : public ::testing::Test {
  bool _initialized;
  JSContext* cx;
  JS::Compartment* compartment;
  JS::Zone* zone;
  JS::PersistentRooted<JSObject*> global;

  DevTools() : _initialized(false), cx(nullptr) {}

  virtual void SetUp() {
    MOZ_ASSERT(!_initialized);

    cx = getContext();
    if (!cx) return;

    global.init(cx, createGlobal());
    if (!global) return;
    JS::EnterRealm(cx, global);

    compartment = js::GetContextCompartment(cx);
    zone = js::GetContextZone(cx);

    _initialized = true;
  }

  JSContext* getContext() { return CycleCollectedJSContext::Get()->Context(); }

  static void reportError(JSContext* cx, const char* message,
                          JSErrorReport* report) {
    fprintf(stderr, "%s:%u:%s\n",
            report->filename ? report->filename : "<no filename>",
            (unsigned int)report->lineno, message);
  }

  static const JSClass* getGlobalClass() {
    static const JSClass globalClass = {"global", JSCLASS_GLOBAL_FLAGS,
                                        &JS::DefaultGlobalClassOps};
    return &globalClass;
  }

  JSObject* createGlobal() {
    /* Create the global object. */
    JS::RealmOptions options;
    return JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                              JS::FireOnNewGlobalHook, options);
  }

  virtual void TearDown() {
    _initialized = false;

    if (global) {
      JS::LeaveRealm(cx, nullptr);
      global = nullptr;
    }
  }
};

// Helper to define a test and ensure that the fixture is initialized properly.
#define DEF_TEST(name, body)   \
  TEST_F(DevTools, name) {     \
    ASSERT_TRUE(_initialized); \
    body                       \
  }

// Fake JS::ubi::Node implementation
class MOZ_STACK_CLASS FakeNode {
 public:
  JS::ubi::EdgeVector edges;
  JS::Compartment* compartment;
  JS::Zone* zone;
  size_t size;

  explicit FakeNode() : edges(), compartment(nullptr), zone(nullptr), size(1) {}
};

namespace JS {
namespace ubi {

template <>
class Concrete<FakeNode> : public Base {
  const char16_t* typeName() const override { return concreteTypeName; }

  js::UniquePtr<EdgeRange> edges(JSContext*, bool) const override {
    return js::UniquePtr<EdgeRange>(js_new<PreComputedEdgeRange>(get().edges));
  }

  Size size(mozilla::MallocSizeOf) const override { return get().size; }

  JS::Zone* zone() const override { return get().zone; }

  JS::Compartment* compartment() const override { return get().compartment; }

 protected:
  explicit Concrete(FakeNode* ptr) : Base(ptr) {}
  FakeNode& get() const { return *static_cast<FakeNode*>(ptr); }

 public:
  static const char16_t concreteTypeName[];
  static void construct(void* storage, FakeNode* ptr) {
    new (storage) Concrete(ptr);
  }
};

const char16_t Concrete<FakeNode>::concreteTypeName[] = u"FakeNode";

}  // namespace ubi
}  // namespace JS

void AddEdge(FakeNode& node, FakeNode& referent,
             const char16_t* edgeName = nullptr) {
  char16_t* ownedEdgeName = nullptr;
  if (edgeName) {
    ownedEdgeName = NS_xstrdup(edgeName);
  }

  JS::ubi::Edge edge(ownedEdgeName, &referent);
  ASSERT_TRUE(node.edges.append(std::move(edge)));
}

// Custom GMock Matchers

// Use the testing namespace to avoid static analysis failures in the gmock
// matcher classes that get generated from MATCHER_P macros.
namespace testing {

// Ensure that given node has the expected number of edges.
MATCHER_P2(EdgesLength, cx, expectedLength, "") {
  auto edges = arg.edges(cx);
  if (!edges) return false;

  int actualLength = 0;
  for (; !edges->empty(); edges->popFront()) actualLength++;

  return Matcher<int>(Eq(expectedLength))
      .MatchAndExplain(actualLength, result_listener);
}

// Get the nth edge and match it with the given matcher.
MATCHER_P3(Edge, cx, n, matcher, "") {
  auto edges = arg.edges(cx);
  if (!edges) return false;

  int i = 0;
  for (; !edges->empty(); edges->popFront()) {
    if (i == n) {
      return Matcher<const JS::ubi::Edge&>(matcher).MatchAndExplain(
          edges->front(), result_listener);
    }

    i++;
  }

  return false;
}

// Ensures that two char16_t* strings are equal.
MATCHER_P(UTF16StrEq, str, "") { return NS_strcmp(arg, str) == 0; }

MATCHER_P(UniqueUTF16StrEq, str, "") { return NS_strcmp(arg.get(), str) == 0; }

MATCHER(UniqueIsNull, "") { return arg.get() == nullptr; }

// Matches an edge whose referent is the node with the given id.
MATCHER_P(EdgeTo, id, "") {
  return Matcher<const DeserializedEdge&>(
             Field(&DeserializedEdge::referent, id))
      .MatchAndExplain(arg, result_listener);
}

}  // namespace testing

// A mock `Writer` class to be used with testing `WriteHeapGraph`.
class MockWriter : public CoreDumpWriter {
 public:
  virtual ~MockWriter() override {}
  MOCK_METHOD2(writeNode,
               bool(const JS::ubi::Node&, CoreDumpWriter::EdgePolicy));
  MOCK_METHOD1(writeMetadata, bool(uint64_t));
};

void ExpectWriteNode(MockWriter& writer, FakeNode& node) {
  EXPECT_CALL(writer, writeNode(Eq(JS::ubi::Node(&node)), _))
      .Times(1)
      .WillOnce(Return(true));
}

#endif  // mozilla_devtools_gtest_DevTools__
