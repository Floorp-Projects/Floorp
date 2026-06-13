import { assertEquals } from "@std/assert";
import { buildSsbKey, parseSsbKey } from "#libs/pwa/ssbKeyUtils.ts";

Deno.test("buildSsbKey uses start_url and userContextId", () => {
  assertEquals(
    buildSsbKey("https://example.com/", 3),
    "https://example.com/:3",
  );
  assertEquals(
    buildSsbKey("https://example.com/", 0),
    "https://example.com/:0",
  );
});

Deno.test("parseSsbKey round-trips composite keys", () => {
  const key = buildSsbKey("https://example.com/app", 5);
  assertEquals(parseSsbKey(key), {
    startUrl: "https://example.com/app",
    userContextId: 5,
  });
});

Deno.test("parseSsbKey round-trips default container key", () => {
  const key = buildSsbKey("https://example.com/", 0);
  assertEquals(parseSsbKey(key), {
    startUrl: "https://example.com/",
    userContextId: 0,
  });
});

Deno.test("different containers produce different keys for same URL", () => {
  const url = "https://example.com/";
  const keyA = buildSsbKey(url, 1);
  const keyB = buildSsbKey(url, 2);
  assertEquals(keyA !== keyB, true);
  assertEquals(parseSsbKey(keyA).userContextId, 1);
  assertEquals(parseSsbKey(keyB).userContextId, 2);
});
