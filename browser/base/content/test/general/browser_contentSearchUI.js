/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_PAGE_BASENAME = "contentSearchUI.html";
const TEST_CONTENT_SCRIPT_BASENAME = "contentSearchUI.js";
const TEST_ENGINE_PREFIX = "browser_searchSuggestionEngine";
const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";
const TEST_ENGINE_2_BASENAME = "searchSuggestionEngine2.xml";

const TEST_MSG = "ContentSearchUIControllerTest";

let { SearchTestUtils } = ChromeUtils.import(
  "resource://testing-common/SearchTestUtils.jsm"
);

SearchTestUtils.init(Assert, registerCleanupFunction);

requestLongerTimeout(2);

function waitForSuggestions() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    return ContentTaskUtils.waitForCondition(() => {
      return content.gController.input.getAttribute("aria-expanded") == "true";
    });
  });
}

async function waitForSearch() {
  await BrowserTestUtils.waitForContentEvent(
    gBrowser.selectedBrowser,
    "ContentSearchClient",
    true,
    event => {
      if (event.detail.type == "Search") {
        event.target._eventDetail = event.detail.data;
        return true;
      }
      return false;
    }
  );
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let eventDetail = content._eventDetail;
    delete content.document._eventDetail;
    return eventDetail;
  });
}

async function waitForSearchSettings() {
  await BrowserTestUtils.waitForContentEvent(
    gBrowser.selectedBrowser,
    "ContentSearchClient",
    true,
    event => {
      if (event.detail.type == "ManageEngines") {
        event.target._eventDetail = event.detail.data;
        return true;
      }
      return false;
    }
  );
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let eventDetail = content._eventDetail;
    delete content.document._eventDetail;
    return eventDetail;
  });
}

function getCurrentState() {
  return SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let controller = content.gController;
    let state = {
      selectedIndex: controller.selectedIndex,
      selectedButtonIndex: controller.selectedButtonIndex,
      numSuggestions: controller._table.hidden ? 0 : controller.numSuggestions,
      suggestionAtIndex: [],
      isFormHistorySuggestionAtIndex: [],

      tableHidden: controller._table.hidden,

      inputValue: controller.input.value,
      ariaExpanded: controller.input.getAttribute("aria-expanded"),
    };

    if (state.numSuggestions) {
      for (let i = 0; i < controller.numSuggestions; i++) {
        state.suggestionAtIndex.push(controller.suggestionAtIndex(i));
        state.isFormHistorySuggestionAtIndex.push(
          controller.isFormHistorySuggestionAtIndex(i)
        );
      }
    }

    return state;
  });
}

async function msg(type, data = null) {
  switch (type) {
    case "reset":
      // Reset both the input and suggestions by select all + delete. If there was
      // no text entered, this won't have any effect, so also escape to ensure the
      // suggestions table is closed.
      await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
        content.gController.input.focus();
        content.synthesizeKey("a", { accelKey: true });
        content.synthesizeKey("KEY_Delete");
        content.synthesizeKey("KEY_Escape");
      });
      break;

    case "key": {
      let keyName = typeof data == "string" ? data : data.key;
      await BrowserTestUtils.synthesizeKey(
        keyName,
        data.modifiers || {},
        gBrowser.selectedBrowser
      );
      if (data?.waitForSuggestions) {
        await waitForSuggestions();
      }
      break;
    }
    case "startComposition":
      await BrowserTestUtils.synthesizeComposition(
        "compositionstart",
        gBrowser.selectedBrowser
      );
      break;
    case "changeComposition": {
      await BrowserTestUtils.synthesizeCompositionChange(
        {
          composition: {
            string: data.data,
            clauses: [
              {
                length: data.length,
                attr: content.COMPOSITION_ATTR_RAW_CLAUSE,
              },
            ],
          },
          caret: { start: data.length, length: 0 },
        },
        gBrowser.selectedBrowser
      );
      if (data?.waitForSuggestions) {
        await waitForSuggestions();
      }
      break;
    }
    case "commitComposition":
      await BrowserTestUtils.synthesizeComposition(
        "compositioncommitasis",
        gBrowser.selectedBrowser
      );
      break;
    case "mousemove":
    case "click": {
      let event;
      let index;
      if (type == "mousemove") {
        event = {
          type: "mousemove",
          clickcount: 0,
        };
        index = data;
      } else {
        event = data.modifiers || null;
        index = data.eltIdx;
      }

      await SpecialPowers.spawn(
        gBrowser.selectedBrowser,
        [type, event, index],
        (eventType, eventArgs, itemIndex) => {
          let controller = content.gController;
          return new Promise(resolve => {
            let row;
            if (itemIndex == -1) {
              row = controller._table.firstChild;
            } else {
              let allElts = [
                ...controller._suggestionsList.children,
                ...controller._oneOffButtons,
                content.document.getElementById("contentSearchSettingsButton"),
              ];
              row = allElts[itemIndex];
            }
            row.addEventListener(eventType, () => resolve(), { once: true });
            content.synthesizeMouseAtCenter(row, eventArgs);
          });
        }
      );
      break;
    }
  }

  return getCurrentState();
}

add_task(async function emptyInput() {
  await setUp();

  let state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("key", "VK_BACK_SPACE");
  checkState(state, "", [], -1);

  await msg("reset");
});

add_task(async function blur() {
  await setUp();

  let state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.gController.input.blur();
  });
  state = await getCurrentState();
  checkState(state, "x", [], -1);

  await msg("reset");
});

add_task(async function upDownKeys() {
  await setUp();

  let state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  // Cycle down the suggestions starting from no selection.
  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = await msg("key", "VK_DOWN");
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = await msg("key", "VK_DOWN");
  checkState(state, "x", ["xfoo", "xbar"], 3);

  state = await msg("key", "VK_DOWN");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  // Cycle up starting from no selection.
  state = await msg("key", "VK_UP");
  checkState(state, "x", ["xfoo", "xbar"], 3);

  state = await msg("key", "VK_UP");
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = await msg("key", "VK_UP");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = await msg("key", "VK_UP");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  state = await msg("key", "VK_UP");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  await msg("reset");
});

add_task(async function rightLeftKeys() {
  await setUp();

  let state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("key", "VK_LEFT");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("key", "VK_LEFT");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("key", "VK_RIGHT");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("key", "VK_RIGHT");
  checkState(state, "x", [], -1);

  state = await msg("key", { key: "VK_DOWN", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  // This should make the xfoo suggestion sticky.  To make sure it sticks,
  // trigger suggestions again and cycle through them by pressing Down until
  // nothing is selected again.
  state = await msg("key", "VK_RIGHT");
  checkState(state, "xfoo", [], -1);

  state = await msg("key", { key: "VK_DOWN", waitForSuggestions: true });
  checkState(state, "xfoo", ["xfoofoo", "xfoobar"], -1);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoofoo", ["xfoofoo", "xfoobar"], 0);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoobar", ["xfoofoo", "xfoobar"], 1);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoofoo", "xfoobar"], 2);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoofoo", "xfoobar"], 3);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoofoo", "xfoobar"], -1);

  await msg("reset");
});

add_task(async function tabKey() {
  await setUp();
  await msg("key", { key: "x", waitForSuggestions: true });

  let state = await msg("key", "VK_TAB");
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = await msg("key", "VK_TAB");
  checkState(state, "x", ["xfoo", "xbar"], 3);

  state = await msg("key", { key: "VK_TAB", modifiers: { shiftKey: true } });
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = await msg("key", { key: "VK_TAB", modifiers: { shiftKey: true } });
  checkState(state, "x", [], -1);

  await setUp();

  await msg("key", { key: "VK_DOWN", waitForSuggestions: true });

  for (let i = 0; i < 3; ++i) {
    state = await msg("key", "VK_TAB");
  }
  checkState(state, "x", [], -1);

  await setUp();

  await msg("key", { key: "VK_DOWN", waitForSuggestions: true });
  state = await msg("key", "VK_DOWN");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0);

  state = await msg("key", "VK_TAB");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0, 0);

  state = await msg("key", "VK_TAB");
  checkState(state, "xfoo", ["xfoo", "xbar"], 0, 1);

  state = await msg("key", "VK_DOWN");
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = await msg("key", "VK_DOWN");
  checkState(state, "x", ["xfoo", "xbar"], 2);

  state = await msg("key", "VK_UP");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = await msg("key", "VK_TAB");
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 0);

  state = await msg("key", "VK_TAB");
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = await msg("key", "VK_TAB");
  checkState(state, "xbar", [], -1);

  await msg("reset");
});

add_task(async function cycleSuggestions() {
  await setUp();
  await msg("key", { key: "x", waitForSuggestions: true });

  let cycle = async function(aSelectedButtonIndex) {
    let modifiers = {
      shiftKey: true,
      accelKey: true,
    };

    let state = await msg("key", { key: "VK_DOWN", modifiers });
    checkState(state, "xfoo", ["xfoo", "xbar"], 0, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_DOWN", modifiers });
    checkState(state, "xbar", ["xfoo", "xbar"], 1, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_DOWN", modifiers });
    checkState(state, "x", ["xfoo", "xbar"], -1, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_DOWN", modifiers });
    checkState(state, "xfoo", ["xfoo", "xbar"], 0, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_UP", modifiers });
    checkState(state, "x", ["xfoo", "xbar"], -1, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_UP", modifiers });
    checkState(state, "xbar", ["xfoo", "xbar"], 1, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_UP", modifiers });
    checkState(state, "xfoo", ["xfoo", "xbar"], 0, aSelectedButtonIndex);

    state = await msg("key", { key: "VK_UP", modifiers });
    checkState(state, "x", ["xfoo", "xbar"], -1, aSelectedButtonIndex);
  };

  await cycle();

  // Repeat with a one-off selected.
  let state = await msg("key", "VK_TAB");
  checkState(state, "x", ["xfoo", "xbar"], 2);
  await cycle(0);

  // Repeat with the settings button selected.
  state = await msg("key", "VK_TAB");
  checkState(state, "x", ["xfoo", "xbar"], 3);
  await cycle(1);

  await msg("reset");
});

add_task(async function cycleOneOffs() {
  await setUp();
  await msg("key", { key: "x", waitForSuggestions: true });

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    let btn =
      content.gController._oneOffButtons[
        content.gController._oneOffButtons.length - 1
      ];
    let newBtn = btn.cloneNode(true);
    btn.parentNode.appendChild(newBtn);
    content.gController._oneOffButtons.push(newBtn);
  });

  let state = await msg("key", "VK_DOWN");
  state = await msg("key", "VK_DOWN");
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  let modifiers = {
    altKey: true,
  };

  state = await msg("key", { key: "VK_DOWN", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 0);

  state = await msg("key", { key: "VK_DOWN", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = await msg("key", { key: "VK_DOWN", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  state = await msg("key", { key: "VK_UP", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = await msg("key", { key: "VK_UP", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 0);

  state = await msg("key", { key: "VK_UP", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1);

  // If the settings button is selected, pressing alt+up/down should select the
  // last/first one-off respectively (and deselect the settings button).
  await msg("key", "VK_TAB");
  await msg("key", "VK_TAB");
  state = await msg("key", "VK_TAB"); // Settings button selected.
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 2);

  state = await msg("key", { key: "VK_UP", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 1);

  state = await msg("key", "VK_TAB");
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 2);

  state = await msg("key", { key: "VK_DOWN", modifiers });
  checkState(state, "xbar", ["xfoo", "xbar"], 1, 0);

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.gController._oneOffButtons.pop().remove();
  });
  await msg("reset");
});

add_task(async function mouse() {
  await setUp();

  let state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("mousemove", 0);
  checkState(state, "x", ["xfoo", "xbar"], 0);

  state = await msg("mousemove", 1);
  checkState(state, "x", ["xfoo", "xbar"], 1);

  state = await msg("mousemove", 2);
  checkState(state, "x", ["xfoo", "xbar"], 2, 0);

  state = await msg("mousemove", 3);
  checkState(state, "x", ["xfoo", "xbar"], 3, 1);

  state = await msg("mousemove", -1);
  checkState(state, "x", ["xfoo", "xbar"], -1);

  await msg("reset");
  await setUp();

  state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  state = await msg("mousemove", 0);
  checkState(state, "x", ["xfoo", "xbar"], 0);

  state = await msg("mousemove", 2);
  checkState(state, "x", ["xfoo", "xbar"], 2, 0);

  state = await msg("mousemove", -1);
  checkState(state, "x", ["xfoo", "xbar"], -1);

  await msg("reset");
});

add_task(async function formHistory() {
  await setUp();

  // Type an X and add it to form history.
  let state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);
  // Wait for Satchel to say it's been added to form history.
  let observePromise = new Promise(resolve => {
    Services.obs.addObserver(function onAdd(subj, topic, data) {
      if (data == "formhistory-add") {
        Services.obs.removeObserver(onAdd, "satchel-storage-changed");
        executeSoon(resolve);
      }
    }, "satchel-storage-changed");
  });

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.gController.addInputValueToFormHistory();
  });
  await observePromise;

  // Reset the input.
  state = await msg("reset");
  checkState(state, "", [], -1);

  // Type an X again.  The form history entry should appear.
  state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(
    state,
    "x",
    [{ str: "x", type: "formHistory" }, "xfoo", "xbar"],
    -1
  );

  // Select the form history entry and delete it.
  state = await msg("key", "VK_DOWN");
  checkState(
    state,
    "x",
    [{ str: "x", type: "formHistory" }, "xfoo", "xbar"],
    0
  );

  // Wait for Satchel.
  observePromise = new Promise(resolve => {
    Services.obs.addObserver(function onRemove(subj, topic, data) {
      if (data == "formhistory-remove") {
        Services.obs.removeObserver(onRemove, "satchel-storage-changed");
        executeSoon(resolve);
      }
    }, "satchel-storage-changed");
  });
  state = await msg("key", "VK_DELETE");
  checkState(state, "x", ["xfoo", "xbar"], -1);

  await observePromise;

  // Reset the input.
  state = await msg("reset");
  checkState(state, "", [], -1);

  // Type an X again.  The form history entry should still be gone.
  state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  await msg("reset");
});

add_task(async function cycleEngines() {
  await setUp();
  await msg("key", { key: "VK_DOWN", waitForSuggestions: true });

  let promiseEngineChange = function(newEngineName) {
    return new Promise(resolve => {
      Services.obs.addObserver(function resolver(subj, topic, data) {
        if (data != "engine-default") {
          return;
        }
        subj.QueryInterface(Ci.nsISearchEngine);
        SimpleTest.is(subj.name, newEngineName, "Engine cycled correctly");
        Services.obs.removeObserver(resolver, "browser-search-engine-modified");
        resolve();
      }, "browser-search-engine-modified");
    });
  };

  let p = promiseEngineChange(
    TEST_ENGINE_PREFIX + " " + TEST_ENGINE_2_BASENAME
  );
  await msg("key", { key: "VK_DOWN", modifiers: { accelKey: true } });
  await p;

  p = promiseEngineChange(TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME);
  await msg("key", { key: "VK_UP", modifiers: { accelKey: true } });
  await p;

  await msg("reset");
});

add_task(async function search() {
  await setUp();

  let modifiers = {};
  ["altKey", "ctrlKey", "metaKey", "shiftKey"].forEach(
    k => (modifiers[k] = true)
  );

  // Test typing a query and pressing enter.
  let p = waitForSearch();
  await msg("key", { key: "x", waitForSuggestions: true });
  await msg("key", { key: "VK_RETURN", modifiers });
  let mesg = await p;
  let eventData = {
    engineName: TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME,
    searchString: "x",
    healthReportKey: "test",
    searchPurpose: "test",
    originalEvent: modifiers,
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test typing a query, then selecting a suggestion and pressing enter.
  p = waitForSearch();
  await msg("key", { key: "x", waitForSuggestions: true });
  await msg("key", "VK_DOWN");
  await msg("key", "VK_DOWN");
  await msg("key", { key: "VK_RETURN", modifiers });
  mesg = await p;
  eventData.searchString = "xfoo";
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME;
  eventData.selection = {
    index: 1,
    kind: "key",
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test typing a query, then selecting a one-off button and pressing enter.
  p = waitForSearch();
  await msg("key", { key: "x", waitForSuggestions: true });
  await msg("key", "VK_UP");
  await msg("key", "VK_UP");
  await msg("key", { key: "VK_RETURN", modifiers });
  mesg = await p;
  delete eventData.selection;
  eventData.searchString = "x";
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_2_BASENAME;
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test typing a query and clicking the search engine header.
  p = waitForSearch();
  modifiers.button = 0;
  await msg("key", { key: "x", waitForSuggestions: true });
  await msg("mousemove", -1);
  await msg("click", { eltIdx: -1, modifiers });
  mesg = await p;
  eventData.originalEvent = modifiers;
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME;
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test typing a query and then clicking a suggestion.
  await msg("key", { key: "x", waitForSuggestions: true });
  p = waitForSearch();
  await msg("mousemove", 1);
  await msg("click", { eltIdx: 1, modifiers });
  mesg = await p;
  eventData.searchString = "xfoo";
  eventData.selection = {
    index: 1,
    kind: "mouse",
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test typing a query and then clicking a one-off button.
  await msg("key", { key: "x", waitForSuggestions: true });
  p = waitForSearch();
  await msg("mousemove", 3);
  await msg("click", { eltIdx: 3, modifiers });
  mesg = await p;
  eventData.searchString = "x";
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_2_BASENAME;
  delete eventData.selection;
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test selecting a suggestion, then clicking a one-off without deselecting the
  // suggestion, using the keyboard.
  delete modifiers.button;
  await msg("key", { key: "x", waitForSuggestions: true });
  p = waitForSearch();
  await msg("key", "VK_DOWN");
  await msg("key", "VK_DOWN");
  await msg("key", "VK_TAB");
  await msg("key", { key: "VK_RETURN", modifiers });
  mesg = await p;
  eventData.searchString = "xfoo";
  eventData.selection = {
    index: 1,
    kind: "key",
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Test searching when using IME composition.
  let state = await msg("startComposition", { data: "" });
  checkState(state, "", [], -1);
  state = await msg("changeComposition", {
    data: "x",
    waitForSuggestions: true,
  });
  checkState(
    state,
    "x",
    [
      { str: "x", type: "formHistory" },
      { str: "xfoo", type: "formHistory" },
      "xbar",
    ],
    -1
  );
  await msg("commitComposition");
  delete modifiers.button;
  p = waitForSearch();
  await msg("key", { key: "VK_RETURN", modifiers });
  mesg = await p;
  eventData.searchString = "x";
  eventData.originalEvent = modifiers;
  eventData.engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME;
  delete eventData.selection;
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  state = await msg("startComposition", { data: "" });
  checkState(state, "", [], -1);
  state = await msg("changeComposition", {
    data: "x",
    waitForSuggestions: true,
  });
  checkState(
    state,
    "x",
    [
      { str: "x", type: "formHistory" },
      { str: "xfoo", type: "formHistory" },
      "xbar",
    ],
    -1
  );

  // Mouse over the first suggestion.
  state = await msg("mousemove", 0);
  checkState(
    state,
    "x",
    [
      { str: "x", type: "formHistory" },
      { str: "xfoo", type: "formHistory" },
      "xbar",
    ],
    0
  );

  // Mouse over the second suggestion.
  state = await msg("mousemove", 1);
  checkState(
    state,
    "x",
    [
      { str: "x", type: "formHistory" },
      { str: "xfoo", type: "formHistory" },
      "xbar",
    ],
    1
  );

  modifiers.button = 0;
  p = waitForSearch();
  await msg("click", { eltIdx: 1, modifiers });
  mesg = await p;
  eventData.searchString = "xfoo";
  eventData.originalEvent = modifiers;
  eventData.selection = {
    index: 1,
    kind: "mouse",
  };
  SimpleTest.isDeeply(eventData, mesg, "Search event data");

  await promiseTab();
  await setUp();

  // Remove form history entries.
  // Wait for Satchel.
  let observePromise = new Promise(resolve => {
    let historyCount = 2;
    Services.obs.addObserver(function onRemove(subj, topic, data) {
      if (data == "formhistory-remove") {
        if (--historyCount) {
          return;
        }
        Services.obs.removeObserver(onRemove, "satchel-storage-changed");
        executeSoon(resolve);
      }
    }, "satchel-storage-changed");
  });

  await msg("key", { key: "x", waitForSuggestions: true });
  await msg("key", "VK_DOWN");
  await msg("key", "VK_DOWN");
  await msg("key", "VK_DELETE");
  await msg("key", "VK_DOWN");
  await msg("key", "VK_DELETE");
  await observePromise;

  await msg("reset");
  state = await msg("key", { key: "x", waitForSuggestions: true });
  checkState(state, "x", ["xfoo", "xbar"], -1);

  await promiseTab();
  await setUp();
  await msg("reset");
});

add_task(async function settings() {
  await setUp();
  await msg("key", { key: "VK_DOWN", waitForSuggestions: true });
  await msg("key", "VK_UP");
  let p = waitForSearchSettings();
  await msg("key", "VK_RETURN");
  await p;

  await msg("reset");
});

var gDidInitialSetUp = false;

async function setUp(aNoEngine) {
  if (!gDidInitialSetUp) {
    var { ContentSearch } = ChromeUtils.import(
      "resource:///actors/ContentSearchParent.jsm"
    );
    let originalOnMessageSearch = ContentSearch._onMessageSearch;
    let originalOnMessageManageEngines = ContentSearch._onMessageManageEngines;
    ContentSearch._onMessageSearch = () => {};
    ContentSearch._onMessageManageEngines = () => {};
    registerCleanupFunction(() => {
      ContentSearch._onMessageSearch = originalOnMessageSearch;
      ContentSearch._onMessageManageEngines = originalOnMessageManageEngines;
    });
    await setUpEngines();
    await promiseTab();
    gDidInitialSetUp = true;
  }

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.gController.input.focus();
  });
}

function checkState(
  actualState,
  expectedInputVal,
  expectedSuggestions,
  expectedSelectedIdx,
  expectedSelectedButtonIdx
) {
  expectedSuggestions = expectedSuggestions.map(sugg => {
    return typeof sugg == "object"
      ? sugg
      : {
          str: sugg,
          type: "remote",
        };
  });

  if (expectedSelectedIdx == -1 && expectedSelectedButtonIdx != undefined) {
    expectedSelectedIdx =
      expectedSuggestions.length + expectedSelectedButtonIdx;
  }

  let expectedState = {
    selectedIndex: expectedSelectedIdx,
    numSuggestions: expectedSuggestions.length,
    suggestionAtIndex: expectedSuggestions.map(s => s.str),
    isFormHistorySuggestionAtIndex: expectedSuggestions.map(
      s => s.type == "formHistory"
    ),

    tableHidden: !expectedSuggestions.length,

    inputValue: expectedInputVal,
    ariaExpanded: !expectedSuggestions.length ? "false" : "true",
  };
  if (expectedSelectedButtonIdx != undefined) {
    expectedState.selectedButtonIndex = expectedSelectedButtonIdx;
  } else if (expectedSelectedIdx < expectedSuggestions.length) {
    expectedState.selectedButtonIndex = -1;
  } else {
    expectedState.selectedButtonIndex =
      expectedSelectedIdx - expectedSuggestions.length;
  }

  SimpleTest.isDeeply(actualState, expectedState, "State");
}

var gMsgMan;

async function promiseTab() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  registerCleanupFunction(() => BrowserTestUtils.removeTab(tab));
  let pageURL = getRootDirectory(gTestPath) + TEST_PAGE_BASENAME;

  let loadedPromise = BrowserTestUtils.firstBrowserLoaded(window);
  openTrustedLinkIn(pageURL, "current");
  await loadedPromise;

  const engineName = TEST_ENGINE_PREFIX + " " + TEST_ENGINE_BASENAME;
  await SpecialPowers.spawn(
    window.gBrowser.selectedBrowser,
    [engineName],
    engineNameChild => {
      Services.search.defaultEngine = Services.search.getEngineByName(
        engineNameChild
      );
      let input = content.document.querySelector("input");
      content.gController = new content.ContentSearchUIController(
        input,
        input.parentNode,
        "test",
        "test"
      );
      return new Promise(resolve => {
        content.addEventListener("ContentSearchService", function listener(
          aEvent
        ) {
          if (
            aEvent.detail.type == "State" &&
            content.gController.defaultEngine.name == engineNameChild
          ) {
            content.removeEventListener("ContentSearchService", listener);
            resolve();
          }
        });
      });
    }
  );
}

async function setUpEngines() {
  info("Removing default search engines");
  let currentEngineName = (await Services.search.getDefault()).name;
  let currentEngines = await Services.search.getVisibleEngines();
  info("Adding test search engines");
  let rootDir = getRootDirectory(gTestPath);
  let engine1 = await SearchTestUtils.promiseNewSearchEngine(
    rootDir + TEST_ENGINE_BASENAME
  );
  await SearchTestUtils.promiseNewSearchEngine(
    rootDir + TEST_ENGINE_2_BASENAME
  );
  await Services.search.setDefault(engine1);
  for (let engine of currentEngines) {
    await Services.search.removeEngine(engine);
  }
  registerCleanupFunction(async () => {
    Services.search.restoreDefaultEngines();
    await Services.search.setDefault(
      Services.search.getEngineByName(currentEngineName)
    );
  });
}
