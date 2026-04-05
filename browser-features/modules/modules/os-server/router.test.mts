// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { Router, NamespaceBuilder, createApi } from "./router.sys.mts";
import type { Context, Handler, HttpResult } from "./router.sys.mts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";

function makeHandler(label: string): Handler<unknown, unknown> {
  return (_ctx: Context) => ({ status: 200, body: label });
}

const tests: TestCase[] = [
  // --- Router.register & Router.match ---
  {
    name: "Router#match returns null when no routes registered",
    fn() {
      const router = new Router();
      const result = router.match("GET", "/anything");
      assertEquals(result, null, "should return null for empty router");
    },
  },
  {
    name: "Router#match finds a simple GET route",
    fn() {
      const router = new Router();
      const handler = makeHandler("hello");
      router.register("GET", "/hello", handler);

      const result = router.match("GET", "/hello");
      assert(result !== null, "should match /hello");
      assertEquals(
        result.handler,
        handler,
        "handler should be the registered one",
      );
      assertEquals(
        Object.keys(result.params).length,
        0,
        "no params expected for static route",
      );
    },
  },
  {
    name: "Router#match does not match wrong method",
    fn() {
      const router = new Router();
      router.register("POST", "/data", makeHandler("post"));

      const result = router.match("GET", "/data");
      assertEquals(result, null, "GET should not match POST route");
    },
  },
  {
    name: "Router#match extracts path parameters",
    fn() {
      const router = new Router();
      router.register("GET", "/items/:id", makeHandler("item"));

      const result = router.match("GET", "/items/42");
      assert(result !== null, "should match /items/42");
      assertEquals(result.params.id, "42", "should extract id param");
    },
  },
  {
    name: "Router#match extracts multiple path parameters",
    fn() {
      const router = new Router();
      router.register(
        "GET",
        "/users/:userId/posts/:postId",
        makeHandler("post"),
      );

      const result = router.match("GET", "/users/abc/posts/xyz");
      assert(result !== null, "should match nested params");
      assertEquals(result.params.userId, "abc", "should extract userId");
      assertEquals(result.params.postId, "xyz", "should extract postId");
    },
  },
  {
    name: "Router#match returns null for non-matching path",
    fn() {
      const router = new Router();
      router.register("GET", "/foo", makeHandler("foo"));

      assertEquals(
        router.match("GET", "/bar"),
        null,
        "should not match different path",
      );
    },
  },
  {
    name: "Router#match returns first matching route",
    fn() {
      const router = new Router();
      const first = makeHandler("first");
      const second = makeHandler("second");
      router.register("GET", "/dup", first);
      router.register("GET", "/dup", second);

      const result = router.match("GET", "/dup");
      assert(result !== null, "should match");
      assertEquals(
        result.handler,
        first,
        "should return first registered handler",
      );
    },
  },
  {
    name: "Router#match does not match partial paths",
    fn() {
      const router = new Router();
      router.register("GET", "/api/v1", makeHandler("v1"));

      assertEquals(
        router.match("GET", "/api/v1/extra"),
        null,
        "should not match longer path",
      );
      assertEquals(
        router.match("GET", "/api"),
        null,
        "should not match shorter path",
      );
    },
  },
  {
    name: "Router supports DELETE method",
    fn() {
      const router = new Router();
      const handler = makeHandler("del");
      router.register("DELETE", "/items/:id", handler);

      const result = router.match("DELETE", "/items/99");
      assert(result !== null, "should match DELETE route");
      assertEquals(result.params.id, "99", "should extract id");
    },
  },

  // --- NamespaceBuilder / createApi ---
  {
    name: "createApi + get registers a GET route",
    fn() {
      const router = new Router();
      const api = createApi(router);
      api.get("/ping", makeHandler("pong"));

      const result = router.match("GET", "/ping");
      assert(result !== null, "should match GET /ping");
    },
  },
  {
    name: "createApi + post registers a POST route",
    fn() {
      const router = new Router();
      const api = createApi(router);
      api.post("/data", makeHandler("data"));

      const result = router.match("POST", "/data");
      assert(result !== null, "should match POST /data");
    },
  },
  {
    name: "createApi + delete registers a DELETE route",
    fn() {
      const router = new Router();
      const api = createApi(router);
      api.delete("/items/:id", makeHandler("del"));

      const result = router.match("DELETE", "/items/5");
      assert(result !== null, "should match DELETE /items/5");
      assertEquals(result!.params.id, "5", "should extract id");
    },
  },
  {
    name: "namespace nests route paths",
    fn() {
      const router = new Router();
      const api = createApi(router);
      api.namespace("api", (ns) => {
        ns.namespace("v1", (v1) => {
          v1.get("/users", makeHandler("users"));
        });
      });

      const result = router.match("GET", "/api/v1/users");
      assert(
        result !== null,
        "should match nested namespace route /api/v1/users",
      );
    },
  },
  {
    name: "namespace with params works correctly",
    fn() {
      const router = new Router();
      const api = createApi(router);
      api.namespace("api", (ns) => {
        ns.get("/items/:id", makeHandler("item"));
      });

      const result = router.match("GET", "/api/items/hello");
      assert(result !== null, "should match namespaced param route");
      assertEquals(
        result.params.id,
        "hello",
        "should extract id from namespaced route",
      );
    },
  },
  {
    name: "NamespaceBuilder methods are chainable",
    fn() {
      const router = new Router();
      const api = createApi(router);
      const returned = api
        .get("/a", makeHandler("a"))
        .post("/b", makeHandler("b"))
        .delete("/c", makeHandler("c"));

      assert(
        returned instanceof NamespaceBuilder,
        "methods should return NamespaceBuilder",
      );
      assert(router.match("GET", "/a") !== null, "/a should be registered");
      assert(router.match("POST", "/b") !== null, "/b should be registered");
      assert(router.match("DELETE", "/c") !== null, "/c should be registered");
    },
  },

  // --- Edge cases ---
  {
    name: "Route with special regex characters in static segment",
    fn() {
      const router = new Router();
      router.register("GET", "/api/v1.0/status", makeHandler("status"));

      const result = router.match("GET", "/api/v1.0/status");
      assert(result !== null, "should match path with dot in segment");
    },
  },
  {
    name: "Route pattern escapes regex special chars properly",
    fn() {
      const router = new Router();
      router.register("GET", "/path+value", makeHandler("plus"));

      // Should match the literal "+" not regex "one or more"
      assertEquals(
        router.match("GET", "/pathvalue"),
        null,
        "should not match without literal +",
      );
      const result = router.match("GET", "/path+value");
      assert(result !== null, "should match literal +");
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("router.test.mts", tests);
}
