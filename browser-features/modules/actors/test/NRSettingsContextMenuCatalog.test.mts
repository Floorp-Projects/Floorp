// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import type {
  ContextMenuCatalogReporter,
  ContextMenuCatalogSnapshot,
} from "#features-chrome/common/context-menu/types.ts";
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

function makeSnapshot(): ContextMenuCatalogSnapshot {
  return {
    schemaVersion: 1,
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
