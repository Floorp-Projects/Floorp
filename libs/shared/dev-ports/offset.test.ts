// SPDX-License-Identifier: MPL-2.0

/// <reference lib="deno.ns" />

import { assertEquals, assertThrows } from "@std/assert";

import {
  DEV_PORT_MAX,
  DEV_PORT_MIN,
  DEV_PORT_NON_OVERLAP_SPAN,
  DEV_PORT_OFFSET_MAX,
  DEV_PORT_OFFSET_PREF,
  DEV_PORTS,
  devHttpOrigin,
  devPort,
  type DevPortName,
  devPortOffsetError,
  devPortOffsetsAreDisjoint,
  devPortString,
  HUB_DEV_PAGES,
  isTrustedDevPort,
  marionettePortForOffset,
  moduleUrlServedByDevHttpServer,
  moduleUrlServedFromDevPort,
  normalizeDevPortOffset,
  requireDevPortOffset,
} from "./defines.ts";
import { createLaunchDevPorts, readDevPortOffset } from "./launch_offset.ts";

const STEP = DEV_PORT_NON_OVERLAP_SPAN;

// ── parsing / validation ────────────────────────────────────────

Deno.test("offset accepts 0 and offsets at or above the non-overlap span", () => {
  assertEquals(normalizeDevPortOffset(0), 0);
  assertEquals(normalizeDevPortOffset("0"), 0);
  assertEquals(normalizeDevPortOffset(STEP), STEP);
  assertEquals(normalizeDevPortOffset(100), 100);
  assertEquals(
    normalizeDevPortOffset(DEV_PORT_OFFSET_MAX),
    DEV_PORT_OFFSET_MAX,
  );
});

Deno.test("offset rejects negative, fractional and overflowing values", () => {
  for (
    const bad of [-1, "-1", 0.5, "1.5", "auto", "n100", "", " ", "+1", "0x10"]
  ) {
    assertEquals(
      normalizeDevPortOffset(bad as unknown as number),
      null,
      `${String(bad)} must not parse as an offset`,
    );
    assertThrows(
      () => requireDevPortOffset(bad as unknown as number),
      Error,
    );
  }
  assertEquals(
    normalizeDevPortOffset(Number.NaN as unknown as number),
    null,
  );
  // Offsets inside the non-overlap span are rejected: a launch shifted by
  // 1..span-1 could share ports (Marionette included) with an offset-0 launch.
  assertEquals(
    normalizeDevPortOffset(1),
    null,
  );
  assertEquals(
    devPortOffsetError(1),
    `port offset 1 is below the non-overlap span ${STEP}; concurrent dev/test launch port sets would overlap (use 0 or >= ${STEP})`,
  );
  assertEquals(normalizeDevPortOffset(DEV_PORT_OFFSET_MAX + 1), null);
});

Deno.test("offset keeps the whole shifted table inside the port range", () => {
  assertEquals(DEV_PORT_OFFSET_MAX, 65_535 - DEV_PORT_MAX);
  assertEquals(
    requireDevPortOffset(DEV_PORT_OFFSET_MAX) + DEV_PORT_MAX,
    65_535,
  );
});

// ── port arithmetic ────────────────────────────────────────────

Deno.test("offset shifts every table accessor by exactly n", () => {
  for (const name of Object.keys(DEV_PORTS) as DevPortName[]) {
    assertEquals(devPort(name, 0), devPort(name));
    assertEquals(devPort(name, 100), DEV_PORTS[name] + 100);
    assertEquals(devPortString(name, 100), String(DEV_PORTS[name] + 100));
  }
  assertEquals(
    devHttpOrigin("loader", "/loader/index.ts", 100),
    "http://localhost:5281/loader/index.ts",
  );
  assertEquals(devHttpOrigin("settings", "/", 100), "http://localhost:5283/");
  // default arguments keep the byte-identical base topology
  assertEquals(
    devHttpOrigin("loader", "/loader/index.ts"),
    "http://localhost:5181/loader/index.ts",
  );
});

Deno.test("marionette port follows the browserRemote table default", () => {
  assertEquals(marionettePortForOffset(0), 5180);
  assertEquals(marionettePortForOffset(100), 5280);
  assertEquals(marionettePortForOffset(100), devPort("browserRemote", 100));
});

Deno.test("marionette port never overlaps between concurrent offsets", () => {
  for (const a of [0, STEP, 100]) {
    for (const b of [0, STEP, 100]) {
      assertEquals(
        a !== b,
        devPortOffsetsAreDisjoint(a, b),
        `${a} vs ${b}`,
      );
      if (a !== b) {
        assertEquals(
          marionettePortForOffset(a) !== marionettePortForOffset(b),
          true,
        );
      }
    }
  }
  // every table port is disjoint between far-apart offsets
  for (const name of Object.keys(DEV_PORTS) as DevPortName[]) {
    const others = (Object.keys(DEV_PORTS) as DevPortName[])
      .map((other) => devPort(other, 100));
    assertEquals(others.includes(devPort(name, 0)), false, `${name}`);
  }
});

Deno.test("disjointness Uses the table span as the minimum safe difference", () => {
  assertEquals(DEV_PORT_NON_OVERLAP_SPAN, DEV_PORT_MAX - DEV_PORT_MIN + 1);
  assertEquals(DEV_PORT_NON_OVERLAP_SPAN >= 20, true, "table stays 20 wide");
  assertEquals(devPortOffsetsAreDisjoint(0, 0), false);
  assertEquals(devPortOffsetsAreDisjoint(0, STEP), true);
  assertEquals(devPortOffsetsAreDisjoint(0, STEP - 1), false);
  assertEquals(devPortOffsetsAreDisjoint(100, 100 + STEP), true);
  assertEquals(devPortOffsetsAreDisjoint(100, 100), false);
});

// ── trust / module URL decisions shifted by offset ─────────────

Deno.test("trust checks trust only the offset ports a launch uses", () => {
  // base port is no longer trusted when offset > 0
  assertEquals(isTrustedDevPort("5183", HUB_DEV_PAGES, 0), true);
  assertEquals(isTrustedDevPort("5183", HUB_DEV_PAGES, 100), false);
  assertEquals(isTrustedDevPort(5183, HUB_DEV_PAGES, 100), false);
  // offset port is trusted
  assertEquals(isTrustedDevPort("5283", HUB_DEV_PAGES, 100), true);
  assertEquals(isTrustedDevPort(5283, HUB_DEV_PAGES, 100), true);
  // other table ports keep their individual trust boundary
  assertEquals(
    isTrustedDevPort(5279, ["profileManager", "settings"], 100),
    true,
  );
  assertEquals(isTrustedDevPort(5179, ["profileManager"], 100), false);
  // empty allowlist trusts nothing, as before
  for (const port of [0, 5183, 5283]) {
    assertEquals(isTrustedDevPort(port, [], 100), false);
  }
});

Deno.test("module URL dev-server detection is offset-aware or offset-free", () => {
  assertEquals(
    moduleUrlServedFromDevPort(
      "http://localhost:5283/src/index.ts",
      "settings",
      100,
    ),
    true,
  );
  assertEquals(
    moduleUrlServedFromDevPort(
      "http://localhost:5183/src/index.ts",
      "settings",
      100,
    ),
    false,
  );
  // the offset-free detector accepts any localhost http dev server
  assertEquals(
    moduleUrlServedByDevHttpServer("http://localhost:5283/src/index.ts"),
    true,
  );
  assertEquals(
    moduleUrlServedByDevHttpServer("http://127.0.0.1:5207/index.js"),
    true,
  );
  assertEquals(
    moduleUrlServedByDevHttpServer(
      "chrome://noraneko-notes/content/index.html",
    ),
    false,
  );
  assertEquals(
    moduleUrlServedByDevHttpServer("resource://noraneko/modules/x.js"),
    false,
  );
  assertEquals(moduleUrlServedByDevHttpServer(null), false);
  assertEquals(moduleUrlServedByDevHttpServer(undefined), false);
});

// ── pref-backed launch resolver ────────────────────────────────

function fakePrefs(value: number | null): {
  getIntPref(prefName: string, fallback: number): number;
  calls: string[];
} {
  const calls: string[] = [];
  return {
    calls,
    getIntPref(prefName: string, _fallback: number): number {
      calls.push(prefName);
      if (value === null) throw new Error("pref missing backend");
      return value;
    },
  };
}

Deno.test("pref resolver maps table names through the launch offset", () => {
  const launch = createLaunchDevPorts(fakePrefs(100));
  assertEquals(launch.offset(), 100);
  assertEquals(launch.port("settings"), 5283);
  assertEquals(launch.port("loader"), 5281);
  assertEquals(launch.portString("browserRemote"), "5280");
  assertEquals(launch.httpOrigin("settings", "/"), "http://localhost:5283/");
  assertEquals(launch.isTrustedPort("5183", HUB_DEV_PAGES), false);
  assertEquals(launch.isTrustedPort("5283", HUB_DEV_PAGES), true);
});

Deno.test("pref resolver without the pref resolves the frozen base table", () => {
  for (const prefs of [undefined, null, fakePrefs(0), fakePrefs(null)]) {
    const launch = createLaunchDevPorts(prefs);
    assertEquals(launch.offset(), 0, `${prefs === undefined}`);
    assertEquals(launch.port("settings"), 5183);
    assertEquals(
      launch.httpOrigin("loader", "/loader/index.ts"),
      "http://localhost:5181/loader/index.ts",
    );
    assertEquals(launch.isTrustedPort("5183", HUB_DEV_PAGES), true);
  }
});

Deno.test("pref resolver is pref-name bound", () => {
  const prefs = fakePrefs(100);
  const _launch = createLaunchDevPorts(prefs);
  assertEquals(prefs.calls, [DEV_PORT_OFFSET_PREF]);
});

Deno.test("readDevPortOffset clamps non-admissible pref values to 0", () => {
  assertEquals(readDevPortOffset(fakePrefs(0)), 0);
  assertEquals(readDevPortOffset(fakePrefs(5)), 0);
  assertEquals(readDevPortOffset(fakePrefs(-3)), 0);
  assertEquals(readDevPortOffset(fakePrefs(100)), 100);
  assertEquals(
    readDevPortOffset(fakePrefs(DEV_PORT_OFFSET_MAX)),
    DEV_PORT_OFFSET_MAX,
  );
  assertEquals(readDevPortOffset(fakePrefs(DEV_PORT_OFFSET_MAX + 1)), 0);
  assertEquals(readDevPortOffset(fakePrefs(null)), 0);
});

// sanity: the offset pref name is stable API
Deno.test("offset pref name", () => {
  assertEquals(DEV_PORT_OFFSET_PREF, "nora.dev.port_offset");
});
