"use strict";

// We need to test a lot of permutations here, and there isn't any sensible way
// to split them up or run them faster.
requestLongerTimeout(6);

const BASE_URL = "http://mochi.test:8888/browser/docshell/test/browser/";

const TEST_PAGE = BASE_URL + "file_onbeforeunload_0.html";

const CONTENT_PROMPT_SUBDIALOG = Services.prefs.getBoolPref(
  "prompts.contentPromptSubDialog",
  false
);

const { PromptTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromptTestUtils.jsm"
);

async function withTabModalPromptCount(expected, task) {
  const DIALOG_TOPIC = CONTENT_PROMPT_SUBDIALOG
    ? "common-dialog-loaded"
    : "tabmodal-dialog-loaded";

  let count = 0;
  function observer() {
    count++;
  }

  Services.obs.addObserver(observer, DIALOG_TOPIC);
  try {
    return await task();
  } finally {
    Services.obs.removeObserver(observer, DIALOG_TOPIC);
    is(count, expected, "Should see expected number of tab modal prompts");
  }
}

function promiseAllowUnloadPrompt(browser, allowNavigation) {
  return PromptTestUtils.handleNextPrompt(
    browser,
    { modalType: Services.prompt.MODAL_TYPE_CONTENT, promptType: "confirmEx" },
    { buttonNumClick: allowNavigation ? 0 : 1 }
  );
}

// Maintain a pool of background tabs with our test document loaded so
// we don't have to wait for a load prior to each test step (potentially
// tearing down and recreating content processes in the process).
const TabPool = {
  poolSize: 5,

  pendingCount: 0,

  readyTabs: [],

  readyPromise: null,
  resolveReadyPromise: null,

  spawnTabs() {
    while (this.pendingCount + this.readyTabs.length < this.poolSize) {
      this.pendingCount++;
      let tab = BrowserTestUtils.addTab(gBrowser, TEST_PAGE);
      BrowserTestUtils.browserLoaded(tab.linkedBrowser).then(() => {
        this.readyTabs.push(tab);
        this.pendingCount--;

        if (this.resolveReadyPromise) {
          this.readyPromise = null;
          this.resolveReadyPromise();
          this.resolveReadyPromise = null;
        }

        this.spawnTabs();
      });
    }
  },

  getReadyPromise() {
    if (!this.readyPromise) {
      this.readyPromise = new Promise(resolve => {
        this.resolveReadyPromise = resolve;
      });
    }
    return this.readyPromise;
  },

  async getTab() {
    while (!this.readyTabs.length) {
      this.spawnTabs();
      await this.getReadyPromise();
    }

    let tab = this.readyTabs.shift();
    this.spawnTabs();

    gBrowser.selectedTab = tab;
    return tab;
  },

  async cleanup() {
    this.poolSize = 0;

    while (this.pendingCount) {
      await this.getReadyPromise();
    }

    while (this.readyTabs.length) {
      await BrowserTestUtils.removeTab(this.readyTabs.shift());
    }
  },
};

const ACTIONS = {
  NONE: 0,
  LISTEN_AND_ALLOW: 1,
  LISTEN_AND_BLOCK: 2,
};

const ACTION_NAMES = new Map(Object.entries(ACTIONS).map(([k, v]) => [v, k]));

function* generatePermutations(depth) {
  if (depth == 0) {
    yield [];
    return;
  }
  for (let subActions of generatePermutations(depth - 1)) {
    for (let action of Object.values(ACTIONS)) {
      yield [action, ...subActions];
    }
  }
}

const PERMUTATIONS = Array.from(generatePermutations(4));

const FRAMES = [
  { process: 0 },
  { process: SpecialPowers.useRemoteSubframes ? 1 : 0 },
  { process: 0 },
  { process: SpecialPowers.useRemoteSubframes ? 1 : 0 },
];

function addListener(bc, block) {
  return SpecialPowers.spawn(bc, [block], block => {
    return new Promise(resolve => {
      function onbeforeunload(event) {
        if (block) {
          event.preventDefault();
        }
        resolve({ event: "beforeunload" });
      }
      content.addEventListener("beforeunload", onbeforeunload, { once: true });
      content.unlisten = () => {
        content.removeEventListener("beforeunload", onbeforeunload);
      };

      content.addEventListener(
        "unload",
        () => {
          resolve({ event: "unload" });
        },
        { once: true }
      );
    });
  });
}

function descendants(bc) {
  if (bc) {
    return [bc, ...descendants(bc.children[0])];
  }
  return [];
}

async function addListeners(frames, actions, startIdx) {
  let process = startIdx >= 0 ? FRAMES[startIdx].process : -1;

  let roundTripPromises = [];

  let expectNestedEventLoop = false;
  let numBlockers = 0;
  let unloadPromises = [];
  let beforeUnloadPromises = [];

  for (let [i, frame] of frames.entries()) {
    let action = actions[i];
    if (action === ACTIONS.NONE) {
      continue;
    }

    let block = action === ACTIONS.LISTEN_AND_BLOCK;
    let promise = addListener(frame, block);
    if (startIdx <= i) {
      if (block || FRAMES[i].process !== process) {
        expectNestedEventLoop = true;
      }
      beforeUnloadPromises.push(promise);
      numBlockers += block;
    } else {
      unloadPromises.push(promise);
    }

    roundTripPromises.push(SpecialPowers.spawn(frame, [], () => {}));
  }

  // Wait for round trip messages to any processes with event listeners to
  // return so we're sure that all listeners are registered and their state
  // flags are propagated before we continue.
  await Promise.all(roundTripPromises);

  return {
    expectNestedEventLoop,
    expectPrompt: !!numBlockers,
    unloadPromises,
    beforeUnloadPromises,
  };
}

async function doTest(actions, startIdx, navigate) {
  let tab = await TabPool.getTab();
  let browser = tab.linkedBrowser;

  let frames = descendants(browser.browsingContext);
  let expected = await addListeners(frames, actions, startIdx);

  let awaitingPrompt = false;
  let promptPromise;
  if (expected.expectPrompt) {
    awaitingPrompt = true;
    promptPromise = promiseAllowUnloadPrompt(browser, false).then(() => {
      awaitingPrompt = false;
    });
  }

  let promptCount = expected.expectPrompt ? 1 : 0;
  await withTabModalPromptCount(promptCount, async () => {
    await navigate(tab, frames).then(result => {
      ok(
        !awaitingPrompt,
        "Navigation should not complete while we're still expecting a prompt"
      );

      is(
        result.eventLoopSpun,
        expected.expectNestedEventLoop,
        "Should have nested event loop?"
      );
    });

    for (let result of await Promise.all(expected.beforeUnloadPromises)) {
      is(
        result.event,
        "beforeunload",
        "Should have seen beforeunload event before unload"
      );
    }
    await promptPromise;

    await Promise.all(
      frames.map(frame =>
        SpecialPowers.spawn(frame, [], () => {
          if (content.unlisten) {
            content.unlisten();
          }
        }).catch(() => {})
      )
    );

    await BrowserTestUtils.removeTab(tab);
  });

  for (let result of await Promise.all(expected.unloadPromises)) {
    is(result.event, "unload", "Should have seen unload event");
  }
}

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });

  for (let actions of PERMUTATIONS) {
    info(
      `Testing frame actions: [${actions.map(action =>
        ACTION_NAMES.get(action)
      )}]`
    );

    for (let startIdx = 0; startIdx < FRAMES.length; startIdx++) {
      info(`Testing content reload from frame ${startIdx}`);

      await doTest(actions, startIdx, (tab, frames) => {
        return SpecialPowers.spawn(frames[startIdx], [], () => {
          let eventLoopSpun = false;
          SpecialPowers.Services.tm.dispatchToMainThread(() => {
            eventLoopSpun = true;
          });

          content.location.reload();

          return { eventLoopSpun };
        });
      });
    }

    info(`Testing tab close from parent process`);
    await doTest(actions, -1, (tab, frames) => {
      let eventLoopSpun = false;
      Services.tm.dispatchToMainThread(() => {
        eventLoopSpun = true;
      });

      BrowserTestUtils.removeTab(tab);

      let result = { eventLoopSpun };

      // Make an extra couple of trips through the event loop to give us time
      // to process SpecialPowers.spawn responses before resolving.
      return new Promise(resolve => {
        executeSoon(() => {
          executeSoon(() => resolve(result));
        });
      });
    });
  }
});

add_task(async function cleanup() {
  await TabPool.cleanup();
});
