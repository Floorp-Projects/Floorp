// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";
import type { RemoteSettingsClient, Selector } from "./types.ts";

const { SearchEngineSelector } = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/search/SearchEngineSelector.sys.mjs",
) as { SearchEngineSelector: new (listener: () => void) => Selector };
const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs",
) as { RemoteSettings: (collection: string) => RemoteSettingsClient };
const { SearchUtils } = ChromeUtils.importESModule(
  "moz-src:///toolkit/components/search/SearchUtils.sys.mjs",
) as { SearchUtils: { SETTINGS_KEY: string; SETTINGS_OVERRIDES_KEY: string } };

const engineId = "floorp-contextual-test";
const engineBase = {
  name: "Contextual Test",
  classification: "general",
  urls: {
    search: {
      base: "https://www.contextual.example/search",
      searchTermParamName: "q",
    },
  },
};
const records = [
  {
    recordType: "engine",
    identifier: engineId,
    base: engineBase,
    variants: [{ environment: { allRegionsAndLocales: true } }],
  },
  { recordType: "engineOrders", orders: [] },
  {
    recordType: "defaultEngines",
    globalDefault: engineId,
    specificDefaults: [],
  },
  { recordType: "availableLocales", locales: [] },
];

function replaceGet(
  client: RemoteSettingsClient,
  get: RemoteSettingsClient["get"],
): () => void {
  const descriptor = Object.getOwnPropertyDescriptor(client, "get");
  Object.defineProperty(client, "get", {
    value: get,
    configurable: true,
    writable: true,
  });
  return () => {
    if (descriptor) Object.defineProperty(client, "get", descriptor);
    else Reflect.deleteProperty(client, "get");
  };
}

async function testConcurrentFreshLookups(): Promise<void> {
  await Services.search.init();
  let release = () => {};
  const ready = new Promise<void>((resolve) => release = resolve);
  let configurationLoads = 0;
  let overrideLoads = 0;
  const restores: Array<() => void> = [];
  let selector: Selector | undefined;
  try {
    restores.push(replaceGet(
      RemoteSettings(SearchUtils.SETTINGS_KEY),
      async () => {
        configurationLoads++;
        await ready;
        return records;
      },
    ));
    restores.push(replaceGet(
      RemoteSettings(SearchUtils.SETTINGS_OVERRIDES_KEY),
      async () => {
        overrideLoads++;
        await ready;
        return [];
      },
    ));
    selector = new SearchEngineSelector(() => {});
    const results = Promise.all([
      selector.findContextualSearchEngineByHost("contextual.example"),
      selector.findContextualSearchEngineById(engineId),
      selector.findContextualSearchEngineByHost("contextual.example"),
    ]);
    release();
    for (const result of await results) {
      assertEquals(
        result?.identifier,
        engineId,
        "all concurrent lookups must match",
      );
      assertEquals(
        result?.name,
        "Contextual Test",
        "engine configuration is preserved",
      );
    }
    assertEquals(
      configurationLoads,
      1,
      "concurrent lookups share one configuration load",
    );
    assertEquals(
      overrideLoads,
      1,
      "concurrent lookups share one overrides load",
    );
    const cached = await selector.findContextualSearchEngineById(engineId);
    assertEquals(cached?.identifier, engineId, "cached lookup still matches");
    assertEquals(
      configurationLoads,
      1,
      "cached lookups must not reload configuration",
    );
    assertEquals(overrideLoads, 1, "cached lookups must not reload overrides");
    assertEquals(
      await selector.findContextualSearchEngineByHost("unmatched.example"),
      null,
      "unmatched host does not reject other candidate providers",
    );
    assertEquals(
      await selector.findContextualSearchEngineById("missing-engine"),
      null,
      "unmatched identifier returns no match",
    );
    if (cached) cached.name = "Changed result";
    assertEquals(
      engineBase.name,
      "Contextual Test",
      "results do not mutate source records",
    );
  } finally {
    release();
    try {
      selector?.reset();
    } finally {
      for (const restore of restores.reverse()) restore();
    }
  }
}

async function testUnavailableConfiguration(): Promise<void> {
  const selector = new SearchEngineSelector(() => {});
  // Avoid invoking Remote Settings' database recovery path for this failure case.
  selector.getEngineConfiguration = () =>
    Promise.reject(
      new Error("expected unavailable test configuration"),
    );
  try {
    assertEquals(
      await selector.findContextualSearchEngineByHost("unmatched.example"),
      null,
      "host lookup allows other providers to continue when configuration is unavailable",
    );
    assertEquals(
      await selector.findContextualSearchEngineById("missing-engine"),
      null,
      "identifier lookup fails without rejecting the action",
    );
  } finally {
    selector.reset();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("contextualSearch.test.ts", [
    {
      name:
        "fresh concurrent host and identifier lookups initialize once and cache real selector data",
      fn: testConcurrentFreshLookups,
    },
    {
      name:
        "unavailable optional configuration does not reject candidate generation",
      fn: testUnavailableConfiguration,
    },
  ]);
}
