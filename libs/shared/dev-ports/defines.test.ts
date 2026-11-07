/// <reference lib="deno.ns" />
// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";

import {
  DEV_PORT_MAX,
  DEV_PORT_MIN,
  DEV_PORTS,
  devHttpOrigin,
  devPort,
  type DevPortName,
  devPortString,
  HUB_DEV_PAGES,
  HUB_DEV_PAGES_PLUS_PROFILE,
  isProductionTrustedHttpOrigin,
  isTrustedDevPort,
  moduleUrlServedFromDevPort,
  PRODUCTION_TRUSTED_HTTP_ORIGINS,
} from "./defines.ts";

const portName: DevPortName = "settings";

Deno.test("dev port table has one entry per named dev/test server", () => {
  const expected = new Set<number>([
    5173,
    5174,
    5179,
    5180,
    5181,
    5183,
    5185,
    5186,
    5187,
    5188,
    5192,
  ]);
  assertEquals([...Object.values(DEV_PORTS)].length, expected.size);
  assertEquals(new Set(Object.values(DEV_PORTS)), expected);
});

Deno.test("dev port table stays inside its declared numeric span", () => {
  assertEquals(DEV_PORT_MIN, 5173);
  assertEquals(DEV_PORT_MAX, 5192);
  for (const port of Object.values(DEV_PORTS)) {
    assertEquals(port >= DEV_PORT_MIN && port <= DEV_PORT_MAX, true, `${port}`);
  }
});

Deno.test("dev port accessors agree with the table", () => {
  assertEquals(devPort(portName), 5183);
  assertEquals(devPortString(portName), "5183");
  assertEquals(devPortString("loader"), "5181");
});

Deno.test("dev http origins stay byte-identical with the frozen topology", () => {
  assertEquals(
    devHttpOrigin("loader", "/loader/index.ts"),
    "http://localhost:5181/loader/index.ts",
  );
  assertEquals(
    devHttpOrigin("loader", "/loader/test/index.ts"),
    "http://localhost:5181/loader/test/index.ts",
  );
  assertEquals(devHttpOrigin("loader"), "http://localhost:5181");
  assertEquals(devHttpOrigin("settings", "/"), "http://localhost:5183/");
  assertEquals(
    devHttpOrigin("settings", "/#/features/design"),
    "http://localhost:5183/#/features/design",
  );
  assertEquals(
    devHttpOrigin("designs", "/lepton/icons"),
    "http://localhost:5174/lepton/icons",
  );
  assertEquals(
    devHttpOrigin("designs", "/fluerial/icons"),
    "http://localhost:5174/fluerial/icons",
  );
  assertEquals(devHttpOrigin("profileManager"), "http://localhost:5179");
  assertEquals(devHttpOrigin("modalChild", "/"), "http://localhost:5185/");
  assertEquals(devHttpOrigin("newtab", "/"), "http://localhost:5186/");
  assertEquals(devHttpOrigin("welcome", "/"), "http://localhost:5187/");
  assertEquals(devHttpOrigin("notes"), "http://localhost:5188");
  assertEquals(
    devHttpOrigin("workflowProgress", "/"),
    "http://localhost:5192/",
  );
  assertEquals(
    devHttpOrigin("updateDevServer", "/update.xml"),
    "http://localhost:5173/update.xml",
  );
});

Deno.test("trust groups mirror the historical actor allowlists", () => {
  // actors comparing dev page ports individually
  assertEquals([...HUB_DEV_PAGES].map((n) => DEV_PORTS[n]), [
    5183,
    5186,
    5187,
    5188,
  ]);
  assertEquals(
    [...HUB_DEV_PAGES_PLUS_PROFILE].map((n) => DEV_PORTS[n]),
    [5183, 5186, 5187, 5188, 5179],
  );
});

Deno.test("dev port trust decision honors the explicit allowlist", () => {
  // string form (DOM location.port)
  assertEquals(isTrustedDevPort("5183", HUB_DEV_PAGES), true);
  // numeric form (nsIURI.port)
  assertEquals(isTrustedDevPort(5183, HUB_DEV_PAGES), true);
  assertEquals(
    isTrustedDevPort(5179, ["profileManager", "settings"] as const),
    true,
  );
  // default ports / missing values are never trusted
  assertEquals(isTrustedDevPort("", HUB_DEV_PAGES), false);
  assertEquals(isTrustedDevPort(0, HUB_DEV_PAGES), false);
  assertEquals(isTrustedDevPort(null, HUB_DEV_PAGES), false);
  assertEquals(isTrustedDevPort(undefined, HUB_DEV_PAGES), false);
  assertEquals(isTrustedDevPort(Number.NaN, HUB_DEV_PAGES), false);
  // ports outside the explicit allowlist are never trusted
  for (const port of Object.values(DEV_PORTS)) {
    assertEquals(isTrustedDevPort(port, []), false);
  }
  assertEquals(isTrustedDevPort(5180, HUB_DEV_PAGES), false);
  assertEquals(isTrustedDevPort(5182, HUB_DEV_PAGES), false);
  assertEquals(isTrustedDevPort("9999", HUB_DEV_PAGES), false);
});

Deno.test("module URL dev-server detection uses table values", () => {
  assertEquals(
    moduleUrlServedFromDevPort(
      "http://localhost:5183/src/index.ts",
      "settings",
    ),
    true,
  );
  assertEquals(
    moduleUrlServedFromDevPort("http://localhost:5183/src/index.ts", "newtab"),
    false,
  );
  assertEquals(
    moduleUrlServedFromDevPort(
      "chrome://noraneko-settings/content/index.html",
      "settings",
    ),
    false,
  );
  assertEquals(moduleUrlServedFromDevPort(undefined, "settings"), false);
  assertEquals(moduleUrlServedFromDevPort(null, "settings"), false);
});

Deno.test("PRODUCTION BUILDS TRUST NO HTTP ORIGIN AT ALL", () => {
  // The production trust surface is a single, empty table; the test is the
  // only place where it may ever become non-empty with review.
  assertEquals([...PRODUCTION_TRUSTED_HTTP_ORIGINS], []);
  for (const name of Object.keys(DEV_PORTS) as DevPortName[]) {
    const origin = devHttpOrigin(name, "/loader/index.ts");
    assertEquals(
      isProductionTrustedHttpOrigin(origin),
      false,
      `production must not trust ${origin}`,
    );
  }

  // exhaustive battery: every dev port on localhost loopback alternates
  for (const port of Object.values(DEV_PORTS)) {
    const origins = [
      `http://localhost:${port}`,
      `http://127.0.0.1:${port}`,
      `http://[::1]:${port}`,
      `http://example.com:${port}`,
      `https://localhost:${port}`,
      `http://localhost.evil:${port}`,
      `http://evil.example.com:${port}`,
    ];
    for (const origin of origins) {
      assertEquals(
        isProductionTrustedHttpOrigin(origin),
        false,
        `production must not trust ${origin}`,
      );
    }
  }

  // empty allowlist means zero trust for the shared dev decision helper
  for (const name of Object.keys(DEV_PORTS) as DevPortName[]) {
    assertEquals(isTrustedDevPort(devPort(name), []), false);
  }
});
