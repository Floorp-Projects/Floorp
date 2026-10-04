// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  excludeTrackedReplacement,
  FirefoxTabReplacementTracker,
  getOriginUserContextId,
  hasFirefoxReplacementSignal,
  hasOriginUserContextId,
} from "../utils/tab-replacement-lifecycle.ts";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../test/utils/test_harness.ts";

type FakeTab = {
  id: string;
  closing?: unknown;
  _endRemoveArgs?: unknown;
  url?: string;
  userContextId?: number;
  getAttribute?: (name: string) => string;
};

type FakeBrowser = {
  getAttribute?: (name: string) => string;
  browsingContext?: {
    originAttributes?: {
      userContextId?: unknown;
    } | null;
  } | null;
};

function makeReplacementSource(id = "closing"): FakeTab {
  return {
    id,
    closing: true,
    _endRemoveArgs: [false, true],
  };
}

function testFirefoxReplacementSignalIsCapabilityChecked(): void {
  assertEquals(hasFirefoxReplacementSignal(null), false, "null is not a tab");
  assertEquals(
    hasFirefoxReplacementSignal({ closing: true }),
    false,
    "missing private removal args must fail safe",
  );
  assertEquals(
    hasFirefoxReplacementSignal({
      closing: true,
      _endRemoveArgs: { 1: true },
    }),
    false,
    "non-array private removal args must fail safe",
  );
  assertEquals(
    hasFirefoxReplacementSignal({
      closing: true,
      _endRemoveArgs: [false, false],
    }),
    false,
    "a close without Firefox's new-tab signal is not a replacement source",
  );
  assertEquals(
    hasFirefoxReplacementSignal({
      closing: false,
      _endRemoveArgs: [false, true],
    }),
    false,
    "the tab must also be actively closing",
  );
  assertEquals(
    hasFirefoxReplacementSignal(makeReplacementSource()),
    true,
    "closing plus _endRemoveArgs[1] === true is the required signal",
  );
}

function testLinkedBrowserOriginAttributesAreAuthoritative(): void {
  const replacementTab: FakeTab = {
    id: "native-replacement",
    getAttribute: (name) => name === "usercontextid" ? "7" : "",
  };
  const replacementBrowser: FakeBrowser = {
    getAttribute: (name) => name === "usercontextid" ? "7" : "",
    browsingContext: {
      originAttributes: { userContextId: 0 },
    },
  };

  assertEquals(
    replacementTab.getAttribute?.("usercontextid"),
    "7",
    "the tab attribute models WorkspacesService's mirrored container value",
  );
  assertEquals(
    replacementBrowser.getAttribute?.("usercontextid"),
    "7",
    "the browser attribute can mirror the same non-authoritative value",
  );
  assertEquals(
    getOriginUserContextId(replacementBrowser),
    0,
    "the browsing-context origin wins over tab and browser attributes",
  );
  assertEquals(
    hasOriginUserContextId(replacementBrowser, 7),
    false,
    "mirrored usercontextid=7 attributes cannot authorize container reuse",
  );
  assertEquals(
    hasOriginUserContextId(replacementBrowser, 0),
    true,
    "the authoritative default-container origin remains reusable for context 0",
  );

  const containerBrowser: FakeBrowser = {
    browsingContext: {
      originAttributes: { userContextId: 7 },
    },
  };
  assertEquals(
    hasOriginUserContextId(containerBrowser, 7),
    true,
    "an exact positive browsing-context match is reusable",
  );
  assertEquals(
    hasOriginUserContextId(containerBrowser, 0),
    false,
    "a container origin cannot be reused for the default context",
  );
}

function testLinkedBrowserOriginLookupFailsClosed(): void {
  const missingCases: unknown[] = [
    null,
    {},
    { browsingContext: null },
    { browsingContext: {} },
    {
      browsingContext: { originAttributes: null },
    },
    {
      browsingContext: { originAttributes: {} },
    },
    {
      browsingContext: {
        originAttributes: { userContextId: "7" },
      },
    },
    {
      browsingContext: {
        originAttributes: { userContextId: -1 },
      },
    },
    {
      browsingContext: {
        originAttributes: { userContextId: 1.5 },
      },
    },
  ];

  for (const browser of missingCases) {
    assertEquals(
      getOriginUserContextId(browser),
      null,
      "missing or malformed authoritative context must be unknown",
    );
    assertEquals(
      hasOriginUserContextId(browser, 0),
      false,
      "unknown authoritative context must not authorize replacement reuse",
    );
  }

  const throwingBrowser: Record<string, unknown> = {};
  Object.defineProperty(throwingBrowser, "browsingContext", {
    get(): never {
      throw new Error("browsing context unavailable");
    },
  });
  assertEquals(
    getOriginUserContextId(throwingBrowser),
    null,
    "a throwing browsing-context getter must fail closed",
  );
  assertEquals(
    hasOriginUserContextId(
      {
        browsingContext: {
          originAttributes: { userContextId: 7 },
        },
      },
      Number.NaN,
    ),
    false,
    "an invalid expected context must also fail closed",
  );
}

function testNativeReplacementIsCapturedByExactIdentity(): void {
  const tracker = new FirefoxTabReplacementTracker<FakeTab>();
  const closing = makeReplacementSource();
  const replacement = { id: "native-replacement", url: "about:newtab" };

  tracker.observeTabOpen(replacement, [closing, replacement]);

  assertEquals(
    tracker.finishTabClose(closing),
    replacement,
    "TabClose should consume the exact TabOpen object",
  );
}

function testAmbiguousClosingSourcesFailSafe(): void {
  const tracker = new FirefoxTabReplacementTracker<FakeTab>();
  const closingA = makeReplacementSource("closing-a");
  const closingB = makeReplacementSource("closing-b");
  const opened = { id: "opened", url: "about:newtab" };

  tracker.observeTabOpen(opened, [closingA, closingB, opened]);

  assertEquals(
    tracker.finishTabClose(closingA),
    null,
    "ambiguous lifecycle must not classify the opened tab",
  );
  assertEquals(
    tracker.finishTabClose(closingB),
    null,
    "neither ambiguous source may claim the opened tab",
  );
}

function testOpenedTabCannotBeItsOwnClosingSource(): void {
  const tracker = new FirefoxTabReplacementTracker<FakeTab>();
  const opened = makeReplacementSource("opened");

  tracker.observeTabOpen(opened, [opened]);

  assertEquals(
    tracker.finishTabClose(opened),
    null,
    "the qualifying closing tab must be another tab",
  );
}

function testFirstCapturedTabCannotBeOverwritten(): void {
  const tracker = new FirefoxTabReplacementTracker<FakeTab>();
  const closing = makeReplacementSource();
  const replacement = { id: "native-replacement" };
  const laterOpened = { id: "later-opened" };

  tracker.observeTabOpen(replacement, [closing, replacement]);
  tracker.observeTabOpen(laterOpened, [closing, replacement, laterOpened]);

  assertEquals(
    tracker.finishTabClose(closing),
    replacement,
    "the first TabOpen object remains authoritative for the transaction",
  );
}

function testOnlyCurrentReplacementIsExcluded(): void {
  const nativeReplacement: FakeTab = {
    id: "native-replacement",
    url: "about:newtab",
    userContextId: 0,
  };
  const floorpStart: FakeTab = {
    id: "floorp-start",
    url: "about:newtab",
    userContextId: 0,
  };
  const userNewtab: FakeTab = {
    id: "user-newtab",
    url: "about:newtab",
    userContextId: 0,
  };
  const leftoverReplacement: FakeTab = {
    id: "leftover-replacement",
    url: "about:newtab",
    userContextId: 0,
  };
  const containerWorkspaceTab: FakeTab = {
    id: "container-workspace",
    url: "about:newtab",
    userContextId: 7,
  };

  const remaining = excludeTrackedReplacement(
    [
      nativeReplacement,
      floorpStart,
      userNewtab,
      leftoverReplacement,
      containerWorkspaceTab,
    ],
    nativeReplacement,
  );

  assert(
    !remaining.includes(nativeReplacement),
    "exact replacement is removed",
  );
  assert(
    remaining.includes(floorpStart),
    "Floorp Start must remain a user tab",
  );
  assert(remaining.includes(userNewtab), "user-created newtab must remain");
  assert(
    remaining.includes(leftoverReplacement),
    "a replacement left from an earlier close must remain",
  );
  assert(
    remaining.includes(containerWorkspaceTab),
    "container workspace tab must remain",
  );
}

function testMissingSignalPreservesEveryTab(): void {
  const floorpStart: FakeTab = { id: "floorp-start", url: "about:newtab" };
  const userNewtab: FakeTab = { id: "user-newtab", url: "about:newtab" };
  const remaining = excludeTrackedReplacement(
    [floorpStart, userNewtab],
    null,
  );

  assertEquals(remaining.length, 2, "missing signal must preserve all tabs");
  assert(remaining.includes(floorpStart), "Floorp Start is preserved");
  assert(remaining.includes(userNewtab), "user newtab is preserved");
}

function testSuppressedCloseStillConsumesTransaction(): void {
  const tracker = new FirefoxTabReplacementTracker<FakeTab>();
  const closing = makeReplacementSource();
  const replacement = { id: "native-replacement" };
  tracker.observeTabOpen(replacement, [closing, replacement]);

  assertEquals(
    tracker.finishTabClose(closing),
    replacement,
    "the transaction is consumed before a suppressed handler returns",
  );
  assertEquals(
    tracker.finishTabClose(closing),
    null,
    "a consumed transaction cannot be reused",
  );
}

function testTerminalGuardRejectsManagerCreatedReentrancy(): void {
  const tracker = new FirefoxTabReplacementTracker<FakeTab>();
  const closing = makeReplacementSource();
  const replacement = { id: "native-replacement" };
  tracker.observeTabOpen(replacement, [closing, replacement]);
  tracker.finishTabClose(closing);

  const managerCreated = {
    id: "manager-created",
    url: "about:newtab",
    userContextId: 7,
  };
  tracker.observeTabOpen(managerCreated, [closing, managerCreated]);

  assertEquals(
    tracker.finishTabClose(closing),
    null,
    "tabs opened reentrantly during TabClose cannot revive the transaction",
  );
}

export async function runAllTests(): Promise<void> {
  const tests: TestCase[] = [
    {
      name: "Firefox replacement signal is capability checked",
      fn: testFirefoxReplacementSignalIsCapabilityChecked,
    },
    {
      name: "linked-browser origin attributes are authoritative",
      fn: testLinkedBrowserOriginAttributesAreAuthoritative,
    },
    {
      name: "linked-browser origin lookup fails closed",
      fn: testLinkedBrowserOriginLookupFailsClosed,
    },
    {
      name: "native replacement is captured by exact identity",
      fn: testNativeReplacementIsCapturedByExactIdentity,
    },
    {
      name: "ambiguous closing sources fail safe",
      fn: testAmbiguousClosingSourcesFailSafe,
    },
    {
      name: "opened tab cannot be its own closing source",
      fn: testOpenedTabCannotBeItsOwnClosingSource,
    },
    {
      name: "first captured tab cannot be overwritten",
      fn: testFirstCapturedTabCannotBeOverwritten,
    },
    {
      name: "only current replacement is excluded",
      fn: testOnlyCurrentReplacementIsExcluded,
    },
    {
      name: "missing signal preserves every tab",
      fn: testMissingSignalPreservesEveryTab,
    },
    {
      name: "suppressed close consumes transaction",
      fn: testSuppressedCloseStillConsumesTransaction,
    },
    {
      name: "terminal guard rejects manager-created reentrancy",
      fn: testTerminalGuardRejectsManagerCreatedReentrancy,
    },
  ];

  await runTests("tabReplacementLifecycle.test.ts", tests);
}

await runAllTests();
