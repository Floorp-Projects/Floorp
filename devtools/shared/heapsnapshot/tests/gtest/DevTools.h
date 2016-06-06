/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_devtools_gtest_DevTools__
#define mozilla_devtools_gtest_DevTools__

#include "CoreDump.pb.h"
#include "jsapi.h"
#include "jspubtd.h"
#include "nsCRTGlue.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "mozilla/devtools/HeapSnapshot.h"
#include "mozilla/dom/ChromeUtils.h"
#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/Move.h"
#include "js/Principals.h"
#include "js/UbiNode.h"
#include "js/UniquePtr.h"

using namespace mozilla;
using namespace mozilla::devtools;
using namespace mozilla::dom;
using namespace testing;

// GTest fixture class that all of our tests derive from.
struct DevTools : public ::testing::Test {
  bool                       _initialized;
  JSRuntime*                 rt;
  JSContext*                 cx;
  JSCompartment*             compartment;
  JS::Zone*                  zone;
  JS::PersistentRootedObject global;

  DevTools()
    : _initialized(false),
      rt(nullptr),
      cx(nullptr)
  { }

  virtual void SetUp() {
    MOZ_ASSERT(!_initialized);

    rt = getRuntime();
    if (!rt)
      return;

    MOZ_RELEASE_ASSERT(!cx);
    MOZ_RELEASE_ASSERT(JS_ContextIterator(rt, &cx));

    JS_BeginRequest(cx);

    global.init(rt, createGlobal());
    if (!global)
      return;
    JS_EnterCompartment(cx, global);

    compartment = js::GetContextCompartment(cx);
    zone = js::GetContextZone(cx);

    _initialized = true;
  }

  JSRuntime* getRuntime() {
    return CycleCollectedJSRuntime::Get()->Runtime();
  }

  static void setNativeStackQuota(JSRuntime* rt)
  {
    const size_t MAX_STACK_SIZE =
      /* Assume we can't use more than 5e5 bytes of C stack by default. */
#if (defined(DEBUG) && defined(__SUNPRO_CC))  || defined(JS_CPU_SPARC)
      /*
       * Sun compiler uses a larger stack space for js::Interpret() with
       * debug.  Use a bigger gMaxStackSize to make "make check" happy.
       */
      5000000
#else
      500000
#endif
      ;

    JS_SetNativeStackQuota(rt, MAX_STACK_SIZE);
  }

  static void reportError(JSContext* cx, const char* message, JSErrorReport* report) {
    fprintf(stderr, "%s:%u:%s\n",
            report->filename ? report->filename : "<no filename>",
            (unsigned int) report->lineno,
            message);
  }

  static const JSClass* getGlobalClass() {
    static const JSClassOps globalClassOps = {
      nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr, nullptr,
      nullptr, nullptr, nullptr,
      JS_GlobalObjectTraceHook
    };
    static const JSClass globalClass = {
      "global", JSCLASS_GLOBAL_FLAGS,
      &globalClassOps
    };
    return &globalClass;
  }

  JSObject* createGlobal()
  {
    /* Create the global object. */
    JS::RootedObject newGlobal(cx);
    JS::CompartmentOptions options;
    options.behaviors().setVersion(JSVERSION_LATEST);
    newGlobal = JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                                   JS::FireOnNewGlobalHook, options);
    if (!newGlobal)
      return nullptr;

    JSAutoCompartment ac(cx, newGlobal);

    /* Populate the global object with the standard globals, like Object and
       Array. */
    if (!JS_InitStandardClasses(cx, newGlobal))
      return nullptr;

    return newGlobal;
  }

  virtual void TearDown() {
    _initialized = false;

    if (global) {
      JS_LeaveCompartment(cx, nullptr);
      global = nullptr;
    }
    if (cx)
      JS_EndRequest(cx);
  }
};


// Helper to define a test and ensure that the fixture is initialized properly.
#define DEF_TEST(name, body)                    \
  TEST_F(DevTools, name) {                      \
    ASSERT_TRUE(_initialized);                  \
    body                                        \
  }


// Fake JS::ubi::Node implementation
class MOZ_STACK_CLASS FakeNode
{
public:
  JS::ubi::EdgeVector edges;
  JSCompartment*      compartment;
  JS::Zone*           zone;
  size_t              size;

  explicit FakeNode()
    : edges(),
    compartment(nullptr),
    zone(nullptr),
    size(1)
  { }
};

namespace JS {
namespace ubi {

template<>
class Concrete<FakeNode> : public Base
{
  const char16_t* typeName() const override {
    return concreteTypeName;
  }

  js::UniquePtr<EdgeRange> edges(JSRuntime*, bool) const override {
    return js::UniquePtr<EdgeRange>(js_new<PreComputedEdgeRange>(get().edges));
  }

  Size size(mozilla::MallocSizeOf) const override {
    return get().size;
  }

  JS::Zone* zone() const override {
    return get().zone;
  }

  JSCompartment* compartment() const override {
    return get().compartment;
  }

protected:
  explicit Concrete(FakeNode* ptr) : Base(ptr) { }
  FakeNode& get() const { return *static_cast<FakeNode*>(ptr); }

public:
  static const char16_t concreteTypeName[];
  static void construct(void* storage, FakeNode* ptr) {
    new (storage) Concrete(ptr);
  }
};

const char16_t Concrete<FakeNode>::concreteTypeName[] = MOZ_UTF16("FakeNode");

} // namespace ubi
} // namespace JS

void AddEdge(FakeNode& node, FakeNode& referent, const char16_t* edgeName = nullptr) {
  char16_t* ownedEdgeName = nullptr;
  if (edgeName) {
    ownedEdgeName = NS_strdup(edgeName);
    ASSERT_NE(ownedEdgeName, nullptr);
  }

  JS::ubi::Edge edge(ownedEdgeName, &referent);
  ASSERT_TRUE(node.edges.append(mozilla::Move(edge)));
}


// Custom GMock Matchers

// Use the testing namespace to avoid static analysis failures in the gmock
// matcher classes that get generated from MATCHER_P macros.
namespace testing {

// Ensure that given node has the expected number of edges.
MATCHER_P2(EdgesLength, rt, expectedLength, "") {
  auto edges = arg.edges(rt);
  if (!edges)
    return false;

  int actualLength = 0;
  for ( ; !edges->empty(); edges->popFront())
    actualLength++;

  return Matcher<int>(Eq(expectedLength))
    .MatchAndExplain(actualLength, result_listener);
}

// Get the nth edge and match it with the given matcher.
MATCHER_P3(Edge, rt, n, matcher, "") {
  auto edges = arg.edges(rt);
  if (!edges)
    return false;

  int i = 0;
  for ( ; !edges->empty(); edges->popFront()) {
    if (i == n) {
      return Matcher<const JS::ubi::Edge&>(matcher)
        .MatchAndExplain(edges->front(), result_listener);
    }

    i++;
  }

  return false;
}

// Ensures that two char16_t* strings are equal.
MATCHER_P(UTF16StrEq, str, "") {
  return NS_strcmp(arg, str) == 0;
}

MATCHER_P(UniqueUTF16StrEq, str, "") {
  return NS_strcmp(arg.get(), str) == 0;
}

MATCHER(UniqueIsNull, "") {
  return arg.get() == nullptr;
}

// Matches an edge whose referent is the node with the given id.
MATCHER_P(EdgeTo, id, "") {
  return Matcher<const DeserializedEdge&>(Field(&DeserializedEdge::referent, id))
    .MatchAndExplain(arg, result_listener);
}

} // namespace testing


// A mock `Writer` class to be used with testing `WriteHeapGraph`.
class MockWriter : public CoreDumpWriter
{
public:
  virtual ~MockWriter() override { }
  MOCK_METHOD2(writeNode, bool(const JS::ubi::Node&, CoreDumpWriter::EdgePolicy));
  MOCK_METHOD1(writeMetadata, bool(uint64_t));
};

void ExpectWriteNode(MockWriter& writer, FakeNode& node) {
  EXPECT_CALL(writer, writeNode(Eq(JS::ubi::Node(&node)), _))
    .Times(1)
    .WillOnce(Return(true));
}

#endif // mozilla_devtools_gtest_DevTools__
