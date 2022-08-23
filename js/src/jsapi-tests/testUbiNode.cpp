/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "builtin/TestingFunctions.h"
#include "js/CompilationAndEvaluation.h"  // JS::Compile
#include "js/GlobalObject.h"              // JS_NewGlobalObject
#include "js/SourceText.h"                // JS::Source{Ownership,Text}
#include "js/UbiNode.h"
#include "js/UbiNodeDominatorTree.h"
#include "js/UbiNodePostOrder.h"
#include "js/UbiNodeShortestPaths.h"
#include "jsapi-tests/tests.h"
#include "util/Text.h"
#include "vm/Compartment.h"
#include "vm/Realm.h"
#include "vm/SavedFrame.h"

#include "vm/JSObject-inl.h"

using JS::RootedObject;
using JS::RootedScript;
using JS::RootedString;
using namespace js;

// A helper JS::ubi::Node concrete implementation that can be used to make mock
// graphs for testing traversals with.
struct FakeNode {
  char name;
  JS::ubi::EdgeVector edges;

  explicit FakeNode(char name) : name(name), edges() {}

  bool addEdgeTo(FakeNode& referent, const char16_t* edgeName = nullptr) {
    JS::ubi::Node node(&referent);

    if (edgeName) {
      auto ownedName = js::DuplicateString(edgeName);
      MOZ_RELEASE_ASSERT(ownedName);
      return edges.emplaceBack(ownedName.release(), node);
    }

    return edges.emplaceBack(nullptr, node);
  }
};

namespace JS {
namespace ubi {

template <>
class Concrete<FakeNode> : public Base {
 protected:
  explicit Concrete(FakeNode* ptr) : Base(ptr) {}
  FakeNode& get() const { return *static_cast<FakeNode*>(ptr); }

 public:
  static void construct(void* storage, FakeNode* ptr) {
    new (storage) Concrete(ptr);
  }

  UniquePtr<EdgeRange> edges(JSContext* cx, bool wantNames) const override {
    return UniquePtr<EdgeRange>(js_new<PreComputedEdgeRange>(get().edges));
  }

  Node::Size size(mozilla::MallocSizeOf) const override { return 1; }

  static const char16_t concreteTypeName[];
  const char16_t* typeName() const override { return concreteTypeName; }
};

const char16_t Concrete<FakeNode>::concreteTypeName[] = u"FakeNode";

}  // namespace ubi
}  // namespace JS

// ubi::Node::zone works
BEGIN_TEST(test_ubiNodeZone) {
  RootedObject global1(cx, JS::CurrentGlobalOrNull(cx));
  CHECK(global1);
  CHECK(JS::ubi::Node(global1).zone() == cx->zone());

  JS::RealmOptions globalOptions;
  RootedObject global2(
      cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                             JS::FireOnNewGlobalHook, globalOptions));
  CHECK(global2);
  CHECK(global1->zone() != global2->zone());
  CHECK(JS::ubi::Node(global2).zone() == global2->zone());
  CHECK(JS::ubi::Node(global2).zone() != global1->zone());

  JS::CompileOptions options(cx);

  // Create a string and a script in the original zone...
  RootedString string1(
      cx, JS_NewStringCopyZ(cx, "Simpson's Individual Stringettes!"));
  CHECK(string1);

  JS::SourceText<mozilla::Utf8Unit> emptySrcBuf;
  CHECK(emptySrcBuf.init(cx, "", 0, JS::SourceOwnership::Borrowed));

  RootedScript script1(cx, JS::Compile(cx, options, emptySrcBuf));
  CHECK(script1);

  {
    // ... and then enter global2's zone and create a string and script
    // there, too.
    JSAutoRealm ar(cx, global2);

    RootedString string2(cx,
                         JS_NewStringCopyZ(cx, "A million household uses!"));
    CHECK(string2);
    RootedScript script2(cx, JS::Compile(cx, options, emptySrcBuf));
    CHECK(script2);

    CHECK(JS::ubi::Node(string1).zone() == global1->zone());
    CHECK(JS::ubi::Node(script1).zone() == global1->zone());

    CHECK(JS::ubi::Node(string2).zone() == global2->zone());
    CHECK(JS::ubi::Node(script2).zone() == global2->zone());
  }

  return true;
}
END_TEST(test_ubiNodeZone)

// ubi::Node::compartment works
BEGIN_TEST(test_ubiNodeCompartment) {
  RootedObject global1(cx, JS::CurrentGlobalOrNull(cx));
  CHECK(global1);
  CHECK(JS::ubi::Node(global1).compartment() == cx->compartment());
  CHECK(JS::ubi::Node(global1).realm() == cx->realm());

  JS::RealmOptions globalOptions;
  RootedObject global2(
      cx, JS_NewGlobalObject(cx, getGlobalClass(), nullptr,
                             JS::FireOnNewGlobalHook, globalOptions));
  CHECK(global2);
  CHECK(global1->compartment() != global2->compartment());
  CHECK(JS::ubi::Node(global2).compartment() == global2->compartment());
  CHECK(JS::ubi::Node(global2).compartment() != global1->compartment());
  CHECK(JS::ubi::Node(global2).realm() == global2->nonCCWRealm());
  CHECK(JS::ubi::Node(global2).realm() != global1->nonCCWRealm());

  JS::CompileOptions options(cx);

  JS::SourceText<mozilla::Utf8Unit> emptySrcBuf;
  CHECK(emptySrcBuf.init(cx, "", 0, JS::SourceOwnership::Borrowed));

  // Create a script in the original realm...
  RootedScript script1(cx, JS::Compile(cx, options, emptySrcBuf));
  CHECK(script1);

  {
    // ... and then enter global2's realm and create a script
    // there, too.
    JSAutoRealm ar(cx, global2);

    RootedScript script2(cx, JS::Compile(cx, options, emptySrcBuf));
    CHECK(script2);

    CHECK(JS::ubi::Node(script1).compartment() == global1->compartment());
    CHECK(JS::ubi::Node(script2).compartment() == global2->compartment());
    CHECK(JS::ubi::Node(script1).realm() == global1->nonCCWRealm());
    CHECK(JS::ubi::Node(script2).realm() == global2->nonCCWRealm());

    // Now create a wrapper for global1 in global2's compartment.
    RootedObject wrappedGlobal1(cx, global1);
    CHECK(cx->compartment()->wrap(cx, &wrappedGlobal1));

    // Cross-compartment wrappers have a compartment() but not a realm().
    CHECK(JS::ubi::Node(wrappedGlobal1).zone() == cx->zone());
    CHECK(JS::ubi::Node(wrappedGlobal1).compartment() == cx->compartment());
    CHECK(JS::ubi::Node(wrappedGlobal1).realm() == nullptr);
  }

  return true;
}
END_TEST(test_ubiNodeCompartment)

template <typename F, typename G>
static bool checkString(const char* expected, F fillBufferFunction,
                        G stringGetterFunction) {
  auto expectedLength = strlen(expected);
  char16_t buf[1024];
  if (fillBufferFunction(mozilla::RangedPtr<char16_t>(buf, 1024), 1024) !=
          expectedLength ||
      !EqualChars(buf, expected, expectedLength)) {
    return false;
  }

  auto string = stringGetterFunction();
  // Expecting a |JSAtom*| from a live |JS::ubi::StackFrame|.
  if (!string.template is<JSAtom*>() ||
      !StringEqualsAscii(string.template as<JSAtom*>(), expected)) {
    return false;
  }

  return true;
}

BEGIN_TEST(test_ubiStackFrame) {
  CHECK(js::DefineTestingFunctions(cx, global, false, false));

  JS::RootedValue val(cx);
  CHECK(
      evaluate("(function one() {                      \n"   // 1
               "  return (function two() {             \n"   // 2
               "    return (function three() {         \n"   // 3
               "      return saveStack();              \n"   // 4
               "    }());                              \n"   // 5
               "  }());                                \n"   // 6
               "}());                                  \n",  // 7
               "filename.js", 1, &val));

  CHECK(val.isObject());
  JS::RootedObject obj(cx, &val.toObject());

  CHECK(obj->is<SavedFrame>());
  JS::Rooted<SavedFrame*> savedFrame(cx, &obj->as<SavedFrame>());

  JS::ubi::StackFrame ubiFrame(savedFrame);

  // All frames should be from the "filename.js" source.
  while (ubiFrame) {
    CHECK(checkString(
        "filename.js",
        [&](mozilla::RangedPtr<char16_t> ptr, size_t length) {
          return ubiFrame.source(ptr, length);
        },
        [&] { return ubiFrame.source(); }));
    ubiFrame = ubiFrame.parent();
  }

  ubiFrame = savedFrame;

  auto bufferFunctionDisplayName = [&](mozilla::RangedPtr<char16_t> ptr,
                                       size_t length) {
    return ubiFrame.functionDisplayName(ptr, length);
  };
  auto getFunctionDisplayName = [&] { return ubiFrame.functionDisplayName(); };

  CHECK(
      checkString("three", bufferFunctionDisplayName, getFunctionDisplayName));
  CHECK(ubiFrame.line() == 4);

  ubiFrame = ubiFrame.parent();
  CHECK(checkString("two", bufferFunctionDisplayName, getFunctionDisplayName));
  CHECK(ubiFrame.line() == 5);

  ubiFrame = ubiFrame.parent();
  CHECK(checkString("one", bufferFunctionDisplayName, getFunctionDisplayName));
  CHECK(ubiFrame.line() == 6);

  ubiFrame = ubiFrame.parent();
  CHECK(ubiFrame.functionDisplayName().is<JSAtom*>());
  CHECK(ubiFrame.functionDisplayName().as<JSAtom*>() == nullptr);
  CHECK(ubiFrame.line() == 7);

  ubiFrame = ubiFrame.parent();
  CHECK(!ubiFrame);

  return true;
}
END_TEST(test_ubiStackFrame)

BEGIN_TEST(test_ubiCoarseType) {
  // Test that our explicit coarseType() overrides work as expected.

  JSObject* obj = nullptr;
  CHECK(JS::ubi::Node(obj).coarseType() == JS::ubi::CoarseType::Object);

  JSScript* script = nullptr;
  CHECK(JS::ubi::Node(script).coarseType() == JS::ubi::CoarseType::Script);

  js::BaseScript* baseScript = nullptr;
  CHECK(JS::ubi::Node(baseScript).coarseType() == JS::ubi::CoarseType::Script);

  js::jit::JitCode* jitCode = nullptr;
  CHECK(JS::ubi::Node(jitCode).coarseType() == JS::ubi::CoarseType::Script);

  JSString* str = nullptr;
  CHECK(JS::ubi::Node(str).coarseType() == JS::ubi::CoarseType::String);

  // Test that the default when coarseType() is not overridden is Other.

  JS::Symbol* sym = nullptr;
  CHECK(JS::ubi::Node(sym).coarseType() == JS::ubi::CoarseType::Other);

  return true;
}
END_TEST(test_ubiCoarseType)

struct ExpectedEdge {
  char from;
  char to;

  ExpectedEdge(FakeNode& fromNode, FakeNode& toNode)
      : from(fromNode.name), to(toNode.name) {}
};

namespace mozilla {

template <>
struct DefaultHasher<ExpectedEdge> {
  using Lookup = ExpectedEdge;

  static HashNumber hash(const Lookup& l) {
    return mozilla::AddToHash(l.from, l.to);
  }

  static bool match(const ExpectedEdge& k, const Lookup& l) {
    return k.from == l.from && k.to == l.to;
  }
};

}  // namespace mozilla

BEGIN_TEST(test_ubiPostOrder) {
  // Construct the following graph:
  //
  //                          .-----.
  //                          |     |
  //                  .-------|  r  |---------------.
  //                  |       |     |               |
  //                  |       '-----'               |
  //                  |                             |
  //               .--V--.                       .--V--.
  //               |     |                       |     |
  //        .------|  a  |------.           .----|  e  |----.
  //        |      |     |      |           |    |     |    |
  //        |      '--^--'      |           |    '-----'    |
  //        |         |         |           |               |
  //     .--V--.      |      .--V--.     .--V--.         .--V--.
  //     |     |      |      |     |     |     |         |     |
  //     |  b  |      '------|  c  |----->  f  |--------->  g  |
  //     |     |             |     |     |     |         |     |
  //     '-----'             '-----'     '-----'         '-----'
  //        |                   |
  //        |      .-----.      |
  //        |      |     |      |
  //        '------>  d  <------'
  //               |     |
  //               '-----'
  //

  FakeNode r('r');
  FakeNode a('a');
  FakeNode b('b');
  FakeNode c('c');
  FakeNode d('d');
  FakeNode e('e');
  FakeNode f('f');
  FakeNode g('g');

  js::HashSet<ExpectedEdge> expectedEdges(cx);

  auto declareEdge = [&](FakeNode& from, FakeNode& to) {
    return from.addEdgeTo(to) && expectedEdges.putNew(ExpectedEdge(from, to));
  };

  CHECK(declareEdge(r, a));
  CHECK(declareEdge(r, e));
  CHECK(declareEdge(a, b));
  CHECK(declareEdge(a, c));
  CHECK(declareEdge(b, d));
  CHECK(declareEdge(c, a));
  CHECK(declareEdge(c, d));
  CHECK(declareEdge(c, f));
  CHECK(declareEdge(e, f));
  CHECK(declareEdge(e, g));
  CHECK(declareEdge(f, g));

  js::Vector<char, 8, js::SystemAllocPolicy> visited;
  {
    // Do a PostOrder traversal, starting from r. Accumulate the names of
    // the nodes we visit in `visited`. Remove edges we traverse from
    // `expectedEdges` as we find them to ensure that we only find each edge
    // once.

    JS::AutoCheckCannotGC nogc(cx);
    JS::ubi::PostOrder traversal(cx, nogc);
    CHECK(traversal.addStart(&r));

    auto onNode = [&](const JS::ubi::Node& node) {
      return visited.append(node.as<FakeNode>()->name);
    };

    auto onEdge = [&](const JS::ubi::Node& origin, const JS::ubi::Edge& edge) {
      ExpectedEdge e(*origin.as<FakeNode>(), *edge.referent.as<FakeNode>());
      if (!expectedEdges.has(e)) {
        fprintf(stderr, "Error: Unexpected edge from %c to %c!\n",
                origin.as<FakeNode>()->name,
                edge.referent.as<FakeNode>()->name);
        return false;
      }

      expectedEdges.remove(e);
      return true;
    };

    CHECK(traversal.traverse(onNode, onEdge));
  }

  fprintf(stderr, "visited.length() = %lu\n", (unsigned long)visited.length());
  for (size_t i = 0; i < visited.length(); i++) {
    fprintf(stderr, "visited[%lu] = '%c'\n", (unsigned long)i, visited[i]);
  }

  CHECK(visited.length() == 8);
  CHECK(visited[0] == 'g');
  CHECK(visited[1] == 'f');
  CHECK(visited[2] == 'e');
  CHECK(visited[3] == 'd');
  CHECK(visited[4] == 'c');
  CHECK(visited[5] == 'b');
  CHECK(visited[6] == 'a');
  CHECK(visited[7] == 'r');

  // We found all the edges we expected.
  CHECK(expectedEdges.count() == 0);

  return true;
}
END_TEST(test_ubiPostOrder)

BEGIN_TEST(test_JS_ubi_DominatorTree) {
  // Construct the following graph:
  //
  //                                  .-----.
  //                                  |     <--------------------------------.
  //          .--------+--------------|  r  |--------------.                 |
  //          |        |              |     |              |                 |
  //          |        |              '-----'              |                 |
  //          |     .--V--.                             .--V--.              |
  //          |     |     |                             |     |              |
  //          |     |  b  |                             |  c  |--------.     |
  //          |     |     |                             |     |        |     |
  //          |     '-----'                             '-----'        |     |
  //       .--V--.     |                                   |        .--V--.  |
  //       |     |     |                                   |        |     |  |
  //       |  a  <-----+                                   |   .----|  g  |  |
  //       |     |     |                              .----'   |    |     |  |
  //       '-----'     |                              |        |    '-----'  |
  //          |        |                              |        |       |     |
  //       .--V--.     |    .-----.                .--V--.     |       |     |
  //       |     |     |    |     |                |     |     |       |     |
  //       |  d  <-----+---->  e  <----.           |  f  |     |       |     |
  //       |     |          |     |    |           |     |     |       |     |
  //       '-----'          '-----'    |           '-----'     |       |     |
  //          |     .-----.    |       |              |        |    .--V--.  |
  //          |     |     |    |       |              |      .-'    |     |  |
  //          '----->  l  |    |       |              |      |      |  j  |  |
  //                |     |    '--.    |              |      |      |     |  |
  //                '-----'       |    |              |      |      '-----'  |
  //                   |       .--V--. |              |   .--V--.      |     |
  //                   |       |     | |              |   |     |      |     |
  //                   '------->  h  |-'              '--->  i  <------'     |
  //                           |     |          .--------->     |            |
  //                           '-----'          |         '-----'            |
  //                              |          .-----.         |               |
  //                              |          |     |         |               |
  //                              '---------->  k  <---------'               |
  //                                         |     |                         |
  //                                         '-----'                         |
  //                                            |                            |
  //                                            '----------------------------'
  //
  // This graph has the following dominator tree:
  //
  //     r
  //     |-- a
  //     |-- b
  //     |-- c
  //     |   |-- f
  //     |   `-- g
  //     |       `-- j
  //     |-- d
  //     |   `-- l
  //     |-- e
  //     |-- i
  //     |-- k
  //     `-- h
  //
  // This graph and dominator tree are taken from figures 1 and 2 of "A Fast
  // Algorithm for Finding Dominators in a Flowgraph" by Lengauer et al:
  // http://www.cs.princeton.edu/courses/archive/spr03/cs423/download/dominators.pdf.

  FakeNode r('r');
  FakeNode a('a');
  FakeNode b('b');
  FakeNode c('c');
  FakeNode d('d');
  FakeNode e('e');
  FakeNode f('f');
  FakeNode g('g');
  FakeNode h('h');
  FakeNode i('i');
  FakeNode j('j');
  FakeNode k('k');
  FakeNode l('l');

  CHECK(r.addEdgeTo(a));
  CHECK(r.addEdgeTo(b));
  CHECK(r.addEdgeTo(c));
  CHECK(a.addEdgeTo(d));
  CHECK(b.addEdgeTo(a));
  CHECK(b.addEdgeTo(d));
  CHECK(b.addEdgeTo(e));
  CHECK(c.addEdgeTo(f));
  CHECK(c.addEdgeTo(g));
  CHECK(d.addEdgeTo(l));
  CHECK(e.addEdgeTo(h));
  CHECK(f.addEdgeTo(i));
  CHECK(g.addEdgeTo(i));
  CHECK(g.addEdgeTo(j));
  CHECK(h.addEdgeTo(e));
  CHECK(h.addEdgeTo(k));
  CHECK(i.addEdgeTo(k));
  CHECK(j.addEdgeTo(i));
  CHECK(k.addEdgeTo(r));
  CHECK(k.addEdgeTo(i));
  CHECK(l.addEdgeTo(h));

  mozilla::Maybe<JS::ubi::DominatorTree> maybeTree;
  {
    JS::AutoCheckCannotGC noGC(cx);
    maybeTree = JS::ubi::DominatorTree::Create(cx, noGC, &r);
  }

  CHECK(maybeTree.isSome());
  auto& tree = *maybeTree;

  // We return the null JS::ubi::Node for nodes that were not reachable in the
  // graph when computing the dominator tree.
  FakeNode m('m');
  CHECK(tree.getImmediateDominator(&m) == JS::ubi::Node());
  CHECK(tree.getDominatedSet(&m).isNothing());

  struct {
    FakeNode& dominated;
    FakeNode& dominator;
  } domination[] = {{r, r}, {a, r}, {b, r}, {c, r}, {d, r}, {e, r}, {f, c},
                    {g, c}, {h, r}, {i, r}, {j, g}, {k, r}, {l, d}};

  for (auto& relation : domination) {
    // Test immediate dominator.
    fprintf(
        stderr, "%c's immediate dominator is %c\n", relation.dominated.name,
        tree.getImmediateDominator(&relation.dominator).as<FakeNode>()->name);
    CHECK(tree.getImmediateDominator(&relation.dominated) ==
          JS::ubi::Node(&relation.dominator));

    // Test the dominated set. Build up the expected dominated set based on
    // the set of nodes immediately dominated by this one in `domination`,
    // then iterate over the actual dominated set and check against the
    // expected set.

    auto& node = relation.dominated;
    fprintf(stderr, "Checking %c's dominated set:\n", node.name);

    js::HashSet<char> expectedDominatedSet(cx);
    for (auto& rel : domination) {
      if (&rel.dominator == &node) {
        fprintf(stderr, "    Expecting %c\n", rel.dominated.name);
        CHECK(expectedDominatedSet.putNew(rel.dominated.name));
      }
    }

    auto maybeActualDominatedSet = tree.getDominatedSet(&node);
    CHECK(maybeActualDominatedSet.isSome());
    auto& actualDominatedSet = *maybeActualDominatedSet;

    for (const auto& dominated : actualDominatedSet) {
      fprintf(stderr, "    Found %c\n", dominated.as<FakeNode>()->name);
      CHECK(expectedDominatedSet.has(dominated.as<FakeNode>()->name));
      expectedDominatedSet.remove(dominated.as<FakeNode>()->name);
    }

    // Ensure we found them all and aren't still expecting nodes we never
    // got.
    CHECK(expectedDominatedSet.count() == 0);

    fprintf(stderr, "Done checking %c's dominated set.\n\n", node.name);
  }

  struct {
    FakeNode& node;
    JS::ubi::Node::Size retainedSize;
  } sizes[] = {
      {r, 13}, {a, 1}, {b, 1}, {c, 4}, {d, 2}, {e, 1}, {f, 1},
      {g, 2},  {h, 1}, {i, 1}, {j, 1}, {k, 1}, {l, 1},
  };

  for (auto& expected : sizes) {
    JS::ubi::Node::Size actual = 0;
    CHECK(tree.getRetainedSize(&expected.node, nullptr, actual));
    CHECK(actual == expected.retainedSize);
  }

  return true;
}
END_TEST(test_JS_ubi_DominatorTree)

BEGIN_TEST(test_JS_ubi_Node_scriptFilename) {
  JS::RootedValue val(cx);
  CHECK(
      evaluate("(function one() {                      \n"   // 1
               "  return (function two() {             \n"   // 2
               "    return (function three() {         \n"   // 3
               "      return function four() {};       \n"   // 4
               "    }());                              \n"   // 5
               "  }());                                \n"   // 6
               "}());                                  \n",  // 7
               "my-cool-filename.js", 1, &val));

  CHECK(val.isObject());
  JS::RootedObject obj(cx, &val.toObject());

  CHECK(obj->is<JSFunction>());
  JS::RootedFunction func(cx, &obj->as<JSFunction>());

  JS::RootedScript script(cx, JSFunction::getOrCreateScript(cx, func));
  CHECK(script);
  CHECK(script->filename());

  JS::ubi::Node node(script);
  const char* filename = node.scriptFilename();
  CHECK(filename);
  CHECK(strcmp(filename, script->filename()) == 0);
  CHECK(strcmp(filename, "my-cool-filename.js") == 0);

  return true;
}
END_TEST(test_JS_ubi_Node_scriptFilename)

#define LAMBDA_CHECK(cond)                                                    \
  do {                                                                        \
    if (!(cond)) {                                                            \
      fprintf(stderr, "%s:%d:CHECK failed: " #cond "\n", __FILE__, __LINE__); \
      return false;                                                           \
    }                                                                         \
  } while (false)

static void dumpPath(JS::ubi::Path& path) {
  for (size_t i = 0; i < path.length(); i++) {
    fprintf(stderr, "path[%llu]->predecessor() = '%c'\n", (long long unsigned)i,
            path[i]->predecessor().as<FakeNode>()->name);
  }
}

BEGIN_TEST(test_JS_ubi_ShortestPaths_no_path) {
  // Create the following graph:
  //
  //     .---.      .---.    .---.
  //     | a | <--> | c |    | b |
  //     '---'      '---'    '---'
  FakeNode a('a');
  FakeNode b('b');
  FakeNode c('c');
  CHECK(a.addEdgeTo(c));
  CHECK(c.addEdgeTo(a));

  mozilla::Maybe<JS::ubi::ShortestPaths> maybeShortestPaths;
  {
    JS::AutoCheckCannotGC noGC(cx);

    JS::ubi::NodeSet targets;
    CHECK(targets.put(&b));

    maybeShortestPaths =
        JS::ubi::ShortestPaths::Create(cx, noGC, 10, &a, std::move(targets));
  }

  CHECK(maybeShortestPaths);
  auto& paths = *maybeShortestPaths;

  size_t numPathsFound = 0;
  bool ok = paths.forEachPath(&b, [&](JS::ubi::Path& path) {
    numPathsFound++;
    dumpPath(path);
    return true;
  });
  CHECK(ok);
  CHECK(numPathsFound == 0);

  return true;
}
END_TEST(test_JS_ubi_ShortestPaths_no_path)

BEGIN_TEST(test_JS_ubi_ShortestPaths_one_path) {
  // Create the following graph:
  //
  //     .---.      .---.     .---.
  //     | a | <--> | c | --> | b |
  //     '---'      '---'     '---'
  FakeNode a('a');
  FakeNode b('b');
  FakeNode c('c');
  CHECK(a.addEdgeTo(c));
  CHECK(c.addEdgeTo(a));
  CHECK(c.addEdgeTo(b));

  mozilla::Maybe<JS::ubi::ShortestPaths> maybeShortestPaths;
  {
    JS::AutoCheckCannotGC noGC(cx);

    JS::ubi::NodeSet targets;
    CHECK(targets.put(&b));

    maybeShortestPaths =
        JS::ubi::ShortestPaths::Create(cx, noGC, 10, &a, std::move(targets));
  }

  CHECK(maybeShortestPaths);
  auto& paths = *maybeShortestPaths;

  size_t numPathsFound = 0;
  bool ok = paths.forEachPath(&b, [&](JS::ubi::Path& path) {
    numPathsFound++;

    dumpPath(path);
    LAMBDA_CHECK(path.length() == 2);
    LAMBDA_CHECK(path[0]->predecessor() == JS::ubi::Node(&a));
    LAMBDA_CHECK(path[1]->predecessor() == JS::ubi::Node(&c));

    return true;
  });

  CHECK(ok);
  CHECK(numPathsFound == 1);

  return true;
}
END_TEST(test_JS_ubi_ShortestPaths_one_path)

BEGIN_TEST(test_JS_ubi_ShortestPaths_multiple_paths) {
  // Create the following graph:
  //
  //                .---.
  //          .-----| a |-----.
  //          |     '---'     |
  //          V       |       V
  //        .---.     |     .---.
  //        | b |     |     | d |
  //        '---'     |     '---'
  //          |       |       |
  //          V       |       V
  //        .---.     |     .---.
  //        | c |     |     | e |
  //        '---'     V     '---'
  //          |     .---.     |
  //          '---->| f |<----'
  //                '---'
  FakeNode a('a');
  FakeNode b('b');
  FakeNode c('c');
  FakeNode d('d');
  FakeNode e('e');
  FakeNode f('f');
  CHECK(a.addEdgeTo(b));
  CHECK(a.addEdgeTo(f));
  CHECK(a.addEdgeTo(d));
  CHECK(b.addEdgeTo(c));
  CHECK(c.addEdgeTo(f));
  CHECK(d.addEdgeTo(e));
  CHECK(e.addEdgeTo(f));

  mozilla::Maybe<JS::ubi::ShortestPaths> maybeShortestPaths;
  {
    JS::AutoCheckCannotGC noGC(cx);

    JS::ubi::NodeSet targets;
    CHECK(targets.put(&f));

    maybeShortestPaths =
        JS::ubi::ShortestPaths::Create(cx, noGC, 10, &a, std::move(targets));
  }

  CHECK(maybeShortestPaths);
  auto& paths = *maybeShortestPaths;

  size_t numPathsFound = 0;
  bool ok = paths.forEachPath(&f, [&](JS::ubi::Path& path) {
    numPathsFound++;
    dumpPath(path);

    switch (path.back()->predecessor().as<FakeNode>()->name) {
      case 'a': {
        LAMBDA_CHECK(path.length() == 1);
        break;
      }

      case 'c': {
        LAMBDA_CHECK(path.length() == 3);
        LAMBDA_CHECK(path[0]->predecessor() == JS::ubi::Node(&a));
        LAMBDA_CHECK(path[1]->predecessor() == JS::ubi::Node(&b));
        LAMBDA_CHECK(path[2]->predecessor() == JS::ubi::Node(&c));
        break;
      }

      case 'e': {
        LAMBDA_CHECK(path.length() == 3);
        LAMBDA_CHECK(path[0]->predecessor() == JS::ubi::Node(&a));
        LAMBDA_CHECK(path[1]->predecessor() == JS::ubi::Node(&d));
        LAMBDA_CHECK(path[2]->predecessor() == JS::ubi::Node(&e));
        break;
      }

      default: {
        // Unexpected path!
        LAMBDA_CHECK(false);
      }
    }

    return true;
  });

  CHECK(ok);
  fprintf(stderr, "numPathsFound = %llu\n", (long long unsigned)numPathsFound);
  CHECK(numPathsFound == 3);

  return true;
}
END_TEST(test_JS_ubi_ShortestPaths_multiple_paths)

BEGIN_TEST(test_JS_ubi_ShortestPaths_more_paths_than_max) {
  // Create the following graph:
  //
  //                .---.
  //          .-----| a |-----.
  //          |     '---'     |
  //          V       |       V
  //        .---.     |     .---.
  //        | b |     |     | d |
  //        '---'     |     '---'
  //          |       |       |
  //          V       |       V
  //        .---.     |     .---.
  //        | c |     |     | e |
  //        '---'     V     '---'
  //          |     .---.     |
  //          '---->| f |<----'
  //                '---'
  FakeNode a('a');
  FakeNode b('b');
  FakeNode c('c');
  FakeNode d('d');
  FakeNode e('e');
  FakeNode f('f');
  CHECK(a.addEdgeTo(b));
  CHECK(a.addEdgeTo(f));
  CHECK(a.addEdgeTo(d));
  CHECK(b.addEdgeTo(c));
  CHECK(c.addEdgeTo(f));
  CHECK(d.addEdgeTo(e));
  CHECK(e.addEdgeTo(f));

  mozilla::Maybe<JS::ubi::ShortestPaths> maybeShortestPaths;
  {
    JS::AutoCheckCannotGC noGC(cx);

    JS::ubi::NodeSet targets;
    CHECK(targets.put(&f));

    maybeShortestPaths =
        JS::ubi::ShortestPaths::Create(cx, noGC, 1, &a, std::move(targets));
  }

  CHECK(maybeShortestPaths);
  auto& paths = *maybeShortestPaths;

  size_t numPathsFound = 0;
  bool ok = paths.forEachPath(&f, [&](JS::ubi::Path& path) {
    numPathsFound++;
    dumpPath(path);
    return true;
  });

  CHECK(ok);
  fprintf(stderr, "numPathsFound = %llu\n", (long long unsigned)numPathsFound);
  CHECK(numPathsFound == 1);

  return true;
}
END_TEST(test_JS_ubi_ShortestPaths_more_paths_than_max)

BEGIN_TEST(test_JS_ubi_ShortestPaths_multiple_edges_to_target) {
  // Create the following graph:
  //
  //                .---.
  //          .-----| a |-----.
  //          |     '---'     |
  //          |       |       |
  //          |x      |y      |z
  //          |       |       |
  //          |       V       |
  //          |     .---.     |
  //          '---->| b |<----'
  //                '---'
  FakeNode a('a');
  FakeNode b('b');
  CHECK(a.addEdgeTo(b, u"x"));
  CHECK(a.addEdgeTo(b, u"y"));
  CHECK(a.addEdgeTo(b, u"z"));

  mozilla::Maybe<JS::ubi::ShortestPaths> maybeShortestPaths;
  {
    JS::AutoCheckCannotGC noGC(cx);

    JS::ubi::NodeSet targets;
    CHECK(targets.put(&b));

    maybeShortestPaths =
        JS::ubi::ShortestPaths::Create(cx, noGC, 10, &a, std::move(targets));
  }

  CHECK(maybeShortestPaths);
  auto& paths = *maybeShortestPaths;

  size_t numPathsFound = 0;
  bool foundX = false;
  bool foundY = false;
  bool foundZ = false;

  bool ok = paths.forEachPath(&b, [&](JS::ubi::Path& path) {
    numPathsFound++;
    dumpPath(path);

    LAMBDA_CHECK(path.length() == 1);
    LAMBDA_CHECK(path.back()->name());
    LAMBDA_CHECK(js_strlen(path.back()->name().get()) == 1);

    auto c = uint8_t(path.back()->name().get()[0]);
    fprintf(stderr, "Edge name = '%c'\n", c);

    switch (c) {
      case 'x': {
        foundX = true;
        break;
      }
      case 'y': {
        foundY = true;
        break;
      }
      case 'z': {
        foundZ = true;
        break;
      }
      default: {
        // Unexpected edge!
        LAMBDA_CHECK(false);
      }
    }

    return true;
  });

  CHECK(ok);
  fprintf(stderr, "numPathsFound = %llu\n", (long long unsigned)numPathsFound);
  CHECK(numPathsFound == 3);
  CHECK(foundX);
  CHECK(foundY);
  CHECK(foundZ);

  return true;
}
END_TEST(test_JS_ubi_ShortestPaths_multiple_edges_to_target)

#undef LAMBDA_CHECK
