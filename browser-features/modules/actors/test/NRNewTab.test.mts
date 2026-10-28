// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../chrome/test/utils/test_harness.ts";
import { isNewTabDocument } from "#libs/newtab/origin.ts";
import { validWorkspaceAssignments } from "#libs/newtab/assignments.ts";
import { validateNewTabRequest } from "#libs/newtab/validation.ts";
import {
  NRNewTabParent,
  privateNewTabSnapshot,
} from "../NRNewTabParent.sys.mts";
import {
  parseSponsorCatalog,
  SponsorService,
} from "../../modules/newtab/SponsorService.sys.mts";

function check(
  condition: unknown,
  message = "new tab invariant",
): asserts condition {
  assert(condition, message);
}
function equal<T>(actual: T, expected: T, message = "new tab result"): void {
  assertEquals(actual, expected, message);
}
function fakeParent(
  invoke: (
    request: import("#libs/newtab/types.ts").NewTabRequest,
  ) => Promise<unknown>,
) {
  const parent = Object.create(NRNewTabParent.prototype) as NRNewTabParent;
  let listener:
    | ((event: import("#libs/newtab/types.ts").NewTabChangedEvent) => void)
    | null = null;
  const events: string[] = [];
  const trace: string[] = [];
  const context = { window: {}, browsingContext: {}, private: false };
  Object.defineProperties(parent, {
    requests: { value: new Map() },
    completed: { value: new Set() },
    context: { value: () => context },
    getService: {
      value: () => ({
        subscribe(fn: typeof listener) {
          trace.push("subscribe");
          listener = fn;
          return () => {
            listener = null;
          };
        },
        invoke(request: import("#libs/newtab/types.ts").NewTabRequest) {
          trace.push("invoke");
          return invoke(request);
        },
      }),
    },
    sendAsyncMessage: {
      value: (name: string) => {
        events.push(name);
      },
    },
  });
  return {
    parent,
    trace,
    events,
    emit: () => listener?.({ topic: "settings", revision: 1 }),
  };
}
async function testFirstSnapshotNotifications(): Promise<void> {
  const fake = fakeParent(async () => {
    await Promise.resolve();
    fake.emit();
    return {};
  });
  try {
    const result = await fake.parent.receiveMessage(
      {
        name: "NRNewTab:Request",
        data: { id: "initial", request: { method: "snapshot", params: {} } },
      } as ReceiveMessageArgument,
    );
    check(result.ok, "snapshot succeeds");
    equal(
      fake.trace.join(","),
      "subscribe,invoke",
      "observation starts before the asynchronous snapshot",
    );
    equal(
      fake.events.length,
      1,
      "a commit during the first snapshot is not lost",
    );
  } finally {
    fake.parent.didDestroy();
  }
}
async function testBoundedReceipts(): Promise<void> {
  let calls = 0;
  const fake = fakeParent(() => {
    calls++;
    return Promise.resolve("x".repeat(256_000));
  });
  try {
    const read = {
      name: "NRNewTab:Request",
      data: {
        id: "large-image",
        request: { method: "folderImage", params: {} },
      },
    } as ReceiveMessageArgument;
    check((await fake.parent.receiveMessage(read)).ok);
    equal(
      (Reflect.get(fake.parent, "requests") as Map<string, unknown>).size,
      0,
      "resolved image payloads are not retained by the actor",
    );
    await fake.parent.receiveMessage(read);
    equal(calls, 2, "completed reads are not memoized");
    const write = {
      name: "NRNewTab:Request",
      data: {
        id: "create-once",
        request: {
          method: "bookmark",
          params: {
            action: "create",
            type: "folder",
            parentGuid: "flrpSDcomm__",
            title: "One",
          },
        },
      },
    } as ReceiveMessageArgument;
    check((await fake.parent.receiveMessage(write)).ok);
    const repeated = await fake.parent.receiveMessage(write);
    check(!repeated.ok);
    equal(
      repeated.error.code,
      "CONFLICT",
      "completed mutation IDs are not applied twice",
    );
    equal(calls, 3);
  } finally {
    fake.parent.didDestroy();
  }
}
function testStaleAssignments(): void {
  const current = { removedWorkspace: "removedGroup", active: "removedGroup" };
  const cleaned = validWorkspaceAssignments(
    { ...current, active: "liveGroup" },
    current,
    ["active"],
    ["liveGroup"],
  );
  equal(Object.keys(cleaned).join(","), "active");
  equal(cleaned.active, "liveGroup");
  let rejected = false;
  try {
    validWorkspaceAssignments({ active: "newMissingGroup" }, current, [
      "active",
    ], ["liveGroup"]);
  } catch {
    rejected = true;
  }
  check(rejected, "a newly selected invalid group still reports a conflict");
}
function testOrigins(): void {
  for (
    const uri of [
      "about:hub#/features/newtab",
      "about:newtab",
      "about:home",
      "chrome://noraneko-newtab/content/index.html",
    ]
  ) check(isNewTabDocument(uri, false));
  for (
    const uri of [
      "about:config",
      "about:hub-attacker",
      "chrome://browser/content/browser.xhtml",
      "https://example.com/",
      "http://localhost:5186.evil.test/",
      "http://127.0.0.1:5186/",
    ]
  ) check(!isNewTabDocument(uri, false));
  check(isNewTabDocument("http://localhost:5186/#feed", true));
  check(isNewTabDocument("http://127.0.0.1:5183/", true));
  check(!isNewTabDocument("http://localhost:5186/attacker.html", true));
  check(!isNewTabDocument("http://localhost:4000/", true));
}
function testRequestValidation(): void {
  equal(
    validateNewTabRequest({ method: "snapshot", params: {} }).method,
    "snapshot",
  );
  for (
    const request of [
      { method: "setStringPref", params: { prefName: "security.disable" } },
      { method: "folder", params: { guid: "../../private" } },
      { method: "articles", params: { limit: 100000 } },
      { method: "articleState", params: { ids: [123], starred: true } },
      {
        method: "patchSettings",
        params: {
          expectedRevision: 1,
          changes: { background: { customImage: "data:image/svg+xml,<svg>" } },
        },
      },
    ]
  ) {
    let rejected = false;
    try {
      validateNewTabRequest(request);
    } catch {
      rejected = true;
    }
    check(rejected, "untrusted request is rejected before dispatch");
  }
}
function testPrivateSnapshot(): void {
  const snapshot = privateNewTabSnapshot();
  check(snapshot.private && snapshot.readOnly);
  equal(snapshot.groups.length, 0);
  equal(snapshot.feeds.length, 0);
  equal(snapshot.sponsors.length, 0);
  equal(snapshot.settings.topSites.pinned.length, 0);
  equal(snapshot.settings.background.type, "none");
  equal(snapshot.settings.background.folderPath, null);
}
async function testForbiddenParent(): Promise<void> {
  const parent = Object.create(NRNewTabParent.prototype) as NRNewTabParent;
  Object.defineProperty(parent, "manager", {
    value: {
      documentURI: Services.io.newURI("about:config"),
      documentPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    },
  });
  Object.defineProperty(parent, "browsingContext", { value: { parent: null } });
  const result = await parent.receiveMessage(
    {
      name: "NRNewTab:Request",
      data: {
        id: "rejected-test",
        request: { method: "snapshot", params: {} },
      },
    } as ReceiveMessageArgument,
  );
  check(!result.ok);
  equal(result.error.code, "READ_ONLY");
}
async function testConcurrentSponsorChoices(): Promise<void> {
  const settingsKey = "floorp.newtab.configs",
    catalogKey = "floorp.newtab.sponsorCatalog";
  const oldSettings = Services.prefs.prefHasUserValue(settingsKey)
    ? Services.prefs.getStringPref(settingsKey)
    : null;
  const oldCatalog = Services.prefs.prefHasUserValue(catalogKey)
    ? Services.prefs.getStringPref(catalogKey)
    : null;
  const { SettingsService } = await import(
    "../../modules/newtab/SettingsService.sys.mts"
  );
  try {
    Services.prefs.setStringPref(
      catalogKey,
      JSON.stringify([
        { id: "choice-a", title: "A", url: "https://example.com/a" },
        { id: "choice-b", title: "B", url: "https://example.com/b" },
      ]),
    );
    await SettingsService.updateInternal({
      sponsors: { enabled: true, dismissedIds: [] },
    });
    await Promise.all([
      SponsorService.dismiss("choice-a"),
      SponsorService.dismiss("choice-b"),
    ]);
    equal(
      SettingsService.get().sponsors.dismissedIds.join(","),
      "choice-a,choice-b",
      "concurrent windows preserve both choices",
    );
    let conflict = false;
    try {
      await SponsorService.save(
        "choice-a",
        "flrpSDcomm__",
        "A",
        "https://example.com/old-ad",
      );
    } catch (error) {
      conflict = error instanceof Error &&
        Reflect.get(error, "code") === "CONFLICT";
    }
    check(
      conflict,
      "a replaced placement cannot save a different URL from the displayed tile",
    );
    await SponsorService.restore();
    equal(
      SettingsService.get().sponsors.dismissedIds.length,
      0,
      "explicit restore clears choices",
    );
  } finally {
    if (oldSettings === null) Services.prefs.clearUserPref(settingsKey);
    else Services.prefs.setStringPref(settingsKey, oldSettings);
    if (oldCatalog === null) Services.prefs.clearUserPref(catalogKey);
    else Services.prefs.setStringPref(catalogKey, oldCatalog);
  }
}
function testCatalog(): void {
  const catalog = parseSponsorCatalog([
    { id: "valid", title: "Valid partner", url: "https://example.com/" },
    { id: "valid", title: "Duplicate", url: "https://example.org/" },
    { id: "unsafe", title: "Unsafe", url: "javascript:alert(1)" },
    {
      id: "auth",
      title: "Credentials",
      url: "https://user:password@example.com/",
    },
    {
      id: "expired",
      title: "Expired",
      url: "https://example.net/",
      activeUntil: 1,
    },
    {
      id: "future",
      title: "Future",
      url: "https://example.net/",
      activeFrom: 1000,
    },
  ], 100);
  equal(catalog.length, 1);
  equal(catalog[0].id, "valid");
}
export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    { name: "exact origins and development-only loopback", fn: testOrigins },
    {
      name: "bounded typed requests reject unsafe inputs",
      fn: testRequestValidation,
    },
    {
      name: "private snapshot carries no profile content",
      fn: testPrivateSnapshot,
    },
    {
      name: "parent rejects unrelated privileged documents",
      fn: testForbiddenParent,
    },
    { name: "catalog validates identities URLs and validity", fn: testCatalog },
    {
      name: "concurrent sponsor choices are preserved",
      fn: testConcurrentSponsorChoices,
    },
  ];
  tests.push(
    {
      name: "first snapshot cannot lose concurrent notifications",
      fn: testFirstSnapshotNotifications,
    },
    {
      name: "large response payloads are released and writes cannot replay",
      fn: testBoundedReceipts,
    },
    {
      name: "obsolete assignments do not block live workspace changes",
      fn: testStaleAssignments,
    },
  );
  await runTests("NRNewTab", tests);
}
