// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import {
  CONTEXT_MENU_SCHEMA_VERSION,
  type ContextMenuCatalogReporter,
  type ContextMenuCatalogSnapshot,
} from "../../../chrome/common/context-menu/types.ts";
import { NRSettingsChild } from "../NRSettingsChild.sys.mts";
import { NRSettingsParent } from "../NRSettingsParent.sys.mts";
import { isSecondaryContextMenuDocumentUri } from "../NRContextMenuChild.sys.mts";

interface CatalogServiceModule {
  ContextMenuCatalogService: ContextMenuCatalogReporter & {
    getSnapshot(): ContextMenuCatalogSnapshot;
  };
}

const OWNER_ID = "settings-catalog-actor-test";
const SURFACE_KEY = "test.settings.catalog";
const BOOL_CAS_PREF = "floorp.test.context-menu.settings-cas.bool";
const STRING_CAS_PREF = "floorp.test.context-menu.settings-cas.string";

function makeSnapshot(): ContextMenuCatalogSnapshot {
  return {
    schemaVersion: CONTEXT_MENU_SCHEMA_VERSION,
    revision: 7,
    locale: "en-US",
    surfaces: [{
      key: SURFACE_KEY,
      label: "Settings catalog test",
      profiles: [{
        key: "default",
        label: "Default",
        containers: [{
          key: "root",
          label: "Root",
          complete: true,
          items: [],
        }],
      }],
    }],
  };
}

function getCatalogService(): CatalogServiceModule[
  "ContextMenuCatalogService"
] {
  const module = ChromeUtils.importESModule(
    "resource://noraneko/modules/context-menu/ContextMenuCatalogService.sys.mjs",
  ) as CatalogServiceModule;
  return module.ContextMenuCatalogService;
}

function getSurfaceLabel(
  snapshot: ContextMenuCatalogSnapshot,
): string | undefined {
  return snapshot.surfaces.find((surface) => surface.key === SURFACE_KEY)
    ?.label;
}

async function testParentActorCatalogQuery(): Promise<void> {
  const service = getCatalogService();
  service.report(OWNER_ID, makeSnapshot());
  try {
    const parent = Object.create(
      NRSettingsParent.prototype,
    ) as NRSettingsParent;
    const result = await parent.receiveMessage({
      name: "getContextMenuCatalog",
    }) as ContextMenuCatalogSnapshot;
    assertEquals(
      getSurfaceLabel(result),
      "Settings catalog test",
      "the parent actor returns the process-wide catalog",
    );
  } finally {
    service.removeOwner(OWNER_ID);
  }
}

async function testChildActorCatalogQuery(): Promise<void> {
  const child = Object.create(NRSettingsChild.prototype) as NRSettingsChild;
  let queryName = "";
  Object.defineProperty(child, "sendQuery", {
    value: (name: string): Promise<ContextMenuCatalogSnapshot> => {
      queryName = name;
      return Promise.resolve(makeSnapshot());
    },
  });

  const result = await child.NRSGetContextMenuCatalog();
  assertEquals(
    queryName,
    "getContextMenuCatalog",
    "the child actor uses the catalog query name",
  );
  assertEquals(
    getSurfaceLabel(result),
    "Settings catalog test",
    "the child actor preserves the structured-clone response",
  );
}

async function testChildActorCatalogFailureRejects(): Promise<void> {
  const child = Object.create(NRSettingsChild.prototype) as NRSettingsChild;
  Object.defineProperty(child, "sendQuery", {
    value: (): Promise<never> => Promise.reject(new Error("catalog offline")),
  });

  let error: unknown;
  try {
    await child.NRSGetContextMenuCatalog();
  } catch (caught) {
    error = caught;
  }
  assert(error instanceof Error, "transport failures must reject the Hub RPC");
  assertEquals(
    error.message,
    "catalog offline",
    "the original catalog transport error is preserved",
  );
}

async function testParentActorCompareAndSetIsAtomic(): Promise<void> {
  const parent = Object.create(NRSettingsParent.prototype) as NRSettingsParent;
  Services.prefs.clearUserPref(BOOL_CAS_PREF);
  Services.prefs.clearUserPref(STRING_CAS_PREF);
  try {
    const createdBool = await parent.receiveMessage({
      name: "compareAndSetBoolPref",
      data: { name: BOOL_CAS_PREF, expectedValue: null, prefValue: true },
    }) as { updated: boolean; currentValue: boolean | null };
    assert(
      createdBool.updated && createdBool.currentValue === true,
      "an absent boolean pref is created when the expected value is null",
    );

    const staleBool = await parent.receiveMessage({
      name: "compareAndSetBoolPref",
      data: { name: BOOL_CAS_PREF, expectedValue: null, prefValue: false },
    }) as { updated: boolean; currentValue: boolean | null };
    assert(
      !staleBool.updated && staleBool.currentValue === true,
      "a stale boolean comparison is rejected with the current value",
    );
    assertEquals(
      Services.prefs.getBoolPref(BOOL_CAS_PREF),
      true,
      "a rejected boolean comparison cannot overwrite the winner",
    );

    const createdString = await parent.receiveMessage({
      name: "compareAndSetStringPref",
      data: {
        name: STRING_CAS_PREF,
        expectedValue: null,
        prefValue: "first",
      },
    }) as { updated: boolean; currentValue: string | null };
    assert(
      createdString.updated && createdString.currentValue === "first",
      "an absent string pref is created when the expected value is null",
    );

    const staleString = await parent.receiveMessage({
      name: "compareAndSetStringPref",
      data: {
        name: STRING_CAS_PREF,
        expectedValue: "stale",
        prefValue: "second",
      },
    }) as { updated: boolean; currentValue: string | null };
    assert(
      !staleString.updated && staleString.currentValue === "first",
      "a stale string comparison is rejected with the current value",
    );
    assertEquals(
      Services.prefs.getStringPref(STRING_CAS_PREF),
      "first",
      "a rejected string comparison cannot overwrite the winner",
    );

    Services.prefs.clearUserPref(STRING_CAS_PREF);
    Services.prefs.setIntPref(STRING_CAS_PREF, 42);
    const wrongTypeString = await parent.receiveMessage({
      name: "compareAndSetStringPref",
      data: {
        name: STRING_CAS_PREF,
        expectedValue: null,
        prefValue: "replacement",
      },
    }) as {
      updated: boolean;
      currentValue: string | null;
      typeMismatch?: boolean;
    };
    assert(
      !wrongTypeString.updated && wrongTypeString.typeMismatch === true,
      "an existing pref of another type never matches the absent token",
    );
    assertEquals(
      Services.prefs.getIntPref(STRING_CAS_PREF),
      42,
      "a wrong-type preference is not overwritten by compare-and-set",
    );
  } finally {
    Services.prefs.clearUserPref(BOOL_CAS_PREF);
    Services.prefs.clearUserPref(STRING_CAS_PREF);
  }
}

async function testParentActorPreferenceReadsReportType(): Promise<void> {
  const parent = Object.create(NRSettingsParent.prototype) as NRSettingsParent;
  Services.prefs.clearUserPref(BOOL_CAS_PREF);
  Services.prefs.clearUserPref(STRING_CAS_PREF);
  try {
    const missingBool = await parent.receiveMessage({
      name: "getBoolPrefState",
      data: { name: BOOL_CAS_PREF },
    }) as { value: boolean | null; typeMismatch: boolean };
    assertEquals(
      JSON.stringify(missingBool),
      JSON.stringify({ value: null, typeMismatch: false }),
      "an absent boolean preference is distinct from a wrong-type value",
    );

    Services.prefs.setBoolPref(BOOL_CAS_PREF, false);
    const boolValue = await parent.receiveMessage({
      name: "getBoolPrefState",
      data: { name: BOOL_CAS_PREF },
    }) as { value: boolean | null; typeMismatch: boolean };
    assertEquals(
      JSON.stringify(boolValue),
      JSON.stringify({ value: false, typeMismatch: false }),
      "a boolean preference read includes its value",
    );

    Services.prefs.setIntPref(STRING_CAS_PREF, 42);
    const wrongTypeString = await parent.receiveMessage({
      name: "getStringPrefState",
      data: { name: STRING_CAS_PREF },
    }) as { value: string | null; typeMismatch: boolean };
    assertEquals(
      JSON.stringify(wrongTypeString),
      JSON.stringify({ value: null, typeMismatch: true }),
      "a wrong-type string preference is reported without reading through it",
    );
  } finally {
    Services.prefs.clearUserPref(BOOL_CAS_PREF);
    Services.prefs.clearUserPref(STRING_CAS_PREF);
  }
}

async function testChildActorCompareAndSetQueryRouting(): Promise<void> {
  const child = Object.create(NRSettingsChild.prototype) as NRSettingsChild;
  const queries: Array<{ name: string; data: unknown }> = [];
  Object.defineProperty(child, "sendQuery", {
    value: (name: string, data: unknown): Promise<unknown> => {
      queries.push({ name, data });
      return Promise.resolve(
        name === "compareAndSetBoolPref"
          ? { updated: true, currentValue: true }
          : { updated: false, currentValue: "server-value" },
      );
    },
  });

  const responses: string[] = [];
  child.NRSettingsRegisterReceiveCallback((data) => responses.push(data));
  child.NRSettingsSend(JSON.stringify({
    t: "q",
    i: "bool-request",
    m: "compareAndSetBoolPref",
    a: [BOOL_CAS_PREF, null, true],
  }));
  child.NRSettingsSend(JSON.stringify({
    t: "q",
    i: "string-request",
    m: "compareAndSetStringPref",
    a: [STRING_CAS_PREF, "client-value", "next-value"],
  }));
  await Promise.resolve();
  await Promise.resolve();
  await Promise.resolve();

  assertEquals(
    queries.map((query) => query.name).join(","),
    "compareAndSetBoolPref,compareAndSetStringPref",
    "the child routes both compare-and-set calls to their parent query names",
  );
  assertEquals(
    JSON.stringify(queries[0]?.data),
    JSON.stringify({
      name: BOOL_CAS_PREF,
      expectedValue: null,
      prefValue: true,
    }),
    "the child preserves the boolean comparison payload",
  );
  assertEquals(
    JSON.stringify(queries[1]?.data),
    JSON.stringify({
      name: STRING_CAS_PREF,
      expectedValue: "client-value",
      prefValue: "next-value",
    }),
    "the child preserves the string comparison payload",
  );

  const parsedResponses = new Map(
    responses.map((response) => {
      const parsed = JSON.parse(response) as {
        t: string;
        i: string;
        r: unknown;
      };
      return [parsed.i, parsed] as const;
    }),
  );
  assertEquals(
    parsedResponses.size,
    2,
    "the child returns exactly one response for each compare-and-set request",
  );
  assertEquals(
    JSON.stringify(parsedResponses.get("bool-request")),
    JSON.stringify({
      t: "s",
      i: "bool-request",
      r: { updated: true, currentValue: true },
    }),
    "the child returns the boolean compare-and-set result through birpc",
  );
  assertEquals(
    JSON.stringify(parsedResponses.get("string-request")),
    JSON.stringify({
      t: "s",
      i: "string-request",
      r: { updated: false, currentValue: "server-value" },
    }),
    "the child returns the string compare-and-set result through birpc",
  );
}

async function testProductionDirectCatalogRoute(): Promise<void> {
  const service = getCatalogService();
  service.report(OWNER_ID, makeSnapshot());
  try {
    const { rpc } = await import(
      "../../../pages-settings/src/lib/rpc/rpc.ts"
    );
    const result = await rpc.getContextMenuCatalog();
    assertEquals(
      getSurfaceLabel(result),
      "Settings catalog test",
      "the production direct-service route reads the same catalog singleton",
    );
  } finally {
    service.removeOwner(OWNER_ID);
  }
}

function testSecondaryChromeDocumentRouting(): void {
  for (
    const uri of [
      "chrome://browser/content/places/places.xhtml",
      "chrome://browser/content/places/bookmarksSidebar.xhtml?view=bookmarks",
      "chrome://browser/content/places/historySidebar.xhtml#history",
      "chrome://browser/content/webext-panels.xhtml",
    ]
  ) {
    assert(
      isSecondaryContextMenuDocumentUri(uri),
      `${uri} should start the secondary context-menu controller`,
    );
  }
  assert(
    !isSecondaryContextMenuDocumentUri(
      "chrome://browser/content/browser.xhtml",
    ),
    "browser.xhtml keeps using the Nora component controller",
  );
  assert(
    !isSecondaryContextMenuDocumentUri("https://example.com/places.xhtml"),
    "web content can never opt in to the privileged controller",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "parent actor catalog query", fn: testParentActorCatalogQuery },
    { name: "child actor catalog query", fn: testChildActorCatalogQuery },
    {
      name: "child actor catalog failure rejects",
      fn: testChildActorCatalogFailureRejects,
    },
    {
      name: "parent actor compare-and-set is atomic",
      fn: testParentActorCompareAndSetIsAtomic,
    },
    {
      name: "parent actor preference reads report type",
      fn: testParentActorPreferenceReadsReportType,
    },
    {
      name: "child actor compare-and-set query routing",
      fn: testChildActorCompareAndSetQueryRouting,
    },
    {
      name: "production direct catalog route",
      fn: testProductionDirectCatalogRoute,
    },
    {
      name: "secondary chrome document routing",
      fn: testSecondaryChromeDocumentRouting,
    },
  ];
  await runTests("NRSettingsContextMenuCatalog.test.mts", tests);
}
