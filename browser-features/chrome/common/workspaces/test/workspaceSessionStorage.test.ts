// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import Workspaces from "../index.ts";
import { getWorkspaceSessionIdentityService } from "../utils/workspace-sessions.ts";
import { getOriginUserContextId } from "../utils/tab-replacement-lifecycle.ts";
import { workspacesDataStore } from "../data/data.ts";
import { enabled, setEnabled } from "../data/config.ts";
import { WORKSPACE_DATA_PREF_NAME } from "../utils/workspaces-static-names.ts";
import { waitFor } from "./workspace-last-tab-test-utils.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const host = "floorp-workspace-session-storage.invalid";
const origin = `https://${host}/`;
const key = "floorp-workspace-session-test";
const databaseName = "floorp-workspace-session-isolation";
const clientValidationPref = "dom.storage.client_validation";

function storage(principal: nsIPrincipal): Storage {
  return Services.domStorageManager.createStorage(
    null as unknown as mozIDOMWindow,
    principal,
    principal,
    origin,
  );
}

function clearContext(userContextId: number): Promise<void> {
  return new Promise((resolve, reject) => {
    Services.clearData.deleteDataFromOriginAttributesPattern(
      { userContextId },
      {
        onDataDeleted(failedFlags) {
          if (failedFlags) {
            reject(new Error(`Site data cleanup failed: ${failedFlags}`));
          } else resolve();
        },
      },
    );
  });
}

function cookie(principal: nsIPrincipal): string | null {
  return Services.cookies.getCookiesFromHost(host, principal.originAttributes)
    .find((item) => item.name === key)?.value ?? null;
}

function writeCookie(principal: nsIPrincipal, value: string): void {
  const validation = Services.cookies.add(
    host,
    "/",
    key,
    value,
    true,
    true,
    false,
    // This runtime's nsICookieManager.add takes milliseconds since the epoch.
    Date.now() + 3_600_000,
    principal.originAttributes,
    Ci.nsICookie.SAMESITE_LAX ?? 1,
    Ci.nsICookie.SCHEME_HTTPS,
  );
  assertEquals(
    validation.result,
    Ci.nsICookieValidation.eOK,
    "fixture cookie accepted",
  );
}

async function withDatabase<T>(
  principal: nsIPrincipal,
  check: (database: IDBDatabase) => Promise<T>,
): Promise<T> {
  const database = await new Promise<IDBDatabase>((resolve, reject) => {
    const request = indexedDB.openForPrincipal(principal, databaseName, 1);
    request.onupgradeneeded = () => request.result.createObjectStore("values");
    request.onerror = () =>
      reject(request.error ?? new Error("IndexedDB open failed"));
    request.onblocked = () => reject(new Error("IndexedDB fixture is blocked"));
    request.onsuccess = () => resolve(request.result);
  });
  try {
    return await check(database);
  } finally {
    database.close();
  }
}

function writeDatabase(principal: nsIPrincipal, value: string): Promise<void> {
  return withDatabase(
    principal,
    (database) =>
      new Promise<void>((resolve, reject) => {
        const transaction = database.transaction("values", "readwrite");
        transaction.objectStore("values").put(value, key);
        transaction.oncomplete = () => resolve();
        transaction.onerror = transaction.onabort = () =>
          reject(transaction.error ?? new Error("IndexedDB write failed"));
      }),
  );
}

function readDatabase(principal: nsIPrincipal): Promise<string | null> {
  return withDatabase(
    principal,
    (database) =>
      new Promise((resolve, reject) => {
        const transaction = database.transaction("values", "readonly");
        const request = transaction.objectStore("values").get(key);
        let value: unknown;
        request.onsuccess = () => {
          value = request.result;
        };
        transaction.oncomplete = () =>
          resolve(value === undefined ? null : String(value));
        transaction.onerror = transaction.onabort = () =>
          reject(transaction.error ?? new Error("IndexedDB read failed"));
      }),
  );
}

async function testRealStorageIsolationAndSharing(): Promise<void> {
  await waitFor(
    () => Boolean(Workspaces.getCtx()),
    "workspace service initialized",
  );
  const service = Workspaces.getCtx();
  assert(service, "workspace service exists");
  const identities = getWorkspaceSessionIdentityService();
  const originalData = Services.prefs.getStringPref(WORKSPACE_DATA_PREF_NAME);
  const originalWorkspace = service.getSelectedWorkspaceID();
  const originalTab = globalThis.gBrowser.selectedTab;
  const originalEnabled = enabled();
  const hadClientValidation = Services.prefs.prefHasUserValue(
    clientValidationPref,
  );
  const originalClientValidation = Services.prefs.getBoolPref(
    clientValidationPref,
    true,
  );
  const originalTabs = new Set(globalThis.gBrowser.tabs);
  const originalWorkspaces = new Set(workspacesDataStore.order);
  const createModal = service.modalCtx.showCreateWorkspacesModal;
  const createdContexts = new Set<number>();
  const create = async (choice: "new" | number) => {
    service.modalCtx.showCreateWorkspacesModal = () =>
      Promise.resolve({
        name: "Workspace storage test",
        userContextId: String(choice),
      });
    const id = await service.createWorkspaceFromDialog();
    assert(id, "storage workspace created");
    const tab = globalThis.gBrowser.selectedTab;
    const actual = getOriginUserContextId(
      globalThis.gBrowser.getBrowserForTab(tab),
    );
    assert(
      actual !== null && actual > 0,
      "real native browsing context is containerized",
    );
    assertEquals(
      actual,
      service.getRawWorkspace(id)?.userContextId,
      "native origin attributes match chosen workspace session",
    );
    if (choice === "new") createdContexts.add(actual);
    return Services.scriptSecurityManager.createContentPrincipal(
      Services.io.newURI(origin),
      { userContextId: actual },
    );
  };
  try {
    // Like Gecko's dom/localstorage/test/unit/head.js, this privileged test
    // creates storage without a Window client. The principals still come from
    // actual tab origin attributes; only client presence validation is bypassed.
    Services.prefs.setBoolPref(clientValidationPref, false);
    setEnabled(true);
    const a = await create("new");
    const b = await create("new");
    const aContext = a.originAttributes.userContextId as number;
    const sharedA = await create(aContext);
    assert(a.origin !== b.origin, "A and B use distinct origin partitions");
    assertEquals(
      a.origin,
      sharedA.origin,
      "shared workspaces use the same native origin partition",
    );
    writeCookie(a, "account-A");
    writeCookie(b, "account-B");
    storage(a).setItem(key, "local-A");
    storage(b).setItem(key, "local-B");
    await writeDatabase(a, "database-A");
    await writeDatabase(b, "database-B");
    assertEquals(cookie(a), "account-A", "A's cookie is isolated");
    assertEquals(cookie(b), "account-B", "B's cookie is isolated");
    assertEquals(
      cookie(sharedA),
      "account-A",
      "shared workspace sees A's cookie",
    );
    assertEquals(
      storage(a).getItem(key),
      "local-A",
      "A's localStorage is isolated",
    );
    assertEquals(
      storage(b).getItem(key),
      "local-B",
      "B's localStorage is isolated",
    );
    assertEquals(
      storage(sharedA).getItem(key),
      "local-A",
      "shared workspace sees A's localStorage",
    );
    assertEquals(
      await readDatabase(a),
      "database-A",
      "A's IndexedDB is isolated",
    );
    assertEquals(
      await readDatabase(b),
      "database-B",
      "B's IndexedDB is isolated",
    );
    assertEquals(
      await readDatabase(sharedA),
      "database-A",
      "shared workspace sees A's IndexedDB",
    );
    await clearContext(aContext);
    assertEquals(cookie(a), null, "clearing A removes A's cookie");
    assertEquals(
      cookie(sharedA),
      null,
      "shared workspace follows A's cookie clearing",
    );
    assertEquals(
      storage(a).getItem(key),
      null,
      "clearing A removes A's localStorage",
    );
    assertEquals(
      storage(sharedA).getItem(key),
      null,
      "shared workspace follows A's localStorage clearing",
    );
    assertEquals(
      await readDatabase(a),
      null,
      "clearing A removes A's IndexedDB",
    );
    assertEquals(
      await readDatabase(sharedA),
      null,
      "shared workspace follows A's IndexedDB clearing",
    );
    assertEquals(
      cookie(b),
      "account-B",
      "clearing A keeps B's authenticated cookie",
    );
    assertEquals(
      storage(b).getItem(key),
      "local-B",
      "clearing A keeps B's localStorage",
    );
    assertEquals(
      await readDatabase(b),
      "database-B",
      "clearing A keeps B's IndexedDB",
    );
  } finally {
    if (hadClientValidation) {
      Services.prefs.setBoolPref(
        clientValidationPref,
        originalClientValidation,
      );
    } else Services.prefs.clearUserPref(clientValidationPref);
    service.modalCtx.showCreateWorkspacesModal = createModal;
    setEnabled(true);
    service.changeWorkspace(originalWorkspace);
    globalThis.gBrowser.selectedTab = originalTab;
    for (const id of [...workspacesDataStore.order]) {
      if (!originalWorkspaces.has(id)) service.deleteWorkspace(id);
    }
    for (const tab of [...globalThis.gBrowser.tabs]) {
      if (!originalTabs.has(tab)) globalThis.gBrowser.removeTab(tab);
    }
    Services.prefs.setStringPref(WORKSPACE_DATA_PREF_NAME, originalData);
    service.changeWorkspace(originalWorkspace);
    globalThis.gBrowser.selectedTab = originalTab;
    for (const context of createdContexts) {
      try {
        await clearContext(context);
      } finally {
        identities.remove(context);
      }
    }
    setEnabled(originalEnabled);
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("workspaceSessionStorage.test.ts", [{
    name:
      "Native cookies, localStorage and IndexedDB isolate sessions and share matching sessions",
    fn: testRealStorageIsolationAndSharing,
  }]);
}
