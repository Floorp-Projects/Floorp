/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

(function() {

const TEST_MSG = "ContentSearchUIControllerTest";
const ENGINE_NAME = "browser_searchSuggestionEngine searchSuggestionEngine.xml";
var gController;

addMessageListener(TEST_MSG, msg => {
  messageHandlers[msg.data.type](msg.data.data);
});

var messageHandlers = {

  init: function() {
    Services.search.currentEngine = Services.search.getEngineByName(ENGINE_NAME);
    let input = content.document.querySelector("input");
    gController =
      new content.ContentSearchUIController(input, input.parentNode, "test", "test");
    content.addEventListener("ContentSearchService", function listener(aEvent) {
      if (aEvent.detail.type == "State" &&
          gController.defaultEngine.name == ENGINE_NAME) {
        content.removeEventListener("ContentSearchService", listener);
        ack("init");
      }
    });
    gController.remoteTimeout = 5000;
  },

  key: function(arg) {
    let keyName = typeof(arg) == "string" ? arg : arg.key;
    content.synthesizeKey(keyName, arg.modifiers || {});
    let wait = arg.waitForSuggestions ? waitForSuggestions : cb => cb();
    wait(ack.bind(null, "key"));
  },

  startComposition: function(arg) {
    content.synthesizeComposition({ type: "compositionstart", data: "" });
    ack("startComposition");
  },

  changeComposition: function(arg) {
    let data = typeof(arg) == "string" ? arg : arg.data;
    content.synthesizeCompositionChange({
      composition: {
        string: data,
        clauses: [
          { length: data.length, attr: content.COMPOSITION_ATTR_RAW_CLAUSE }
        ]
      },
      caret: { start: data.length, length: 0 }
    });
    let wait = arg.waitForSuggestions ? waitForSuggestions : cb => cb();
    wait(ack.bind(null, "changeComposition"));
  },

  commitComposition: function() {
    content.synthesizeComposition({ type: "compositioncommitasis" });
    ack("commitComposition");
  },

  focus: function() {
    gController.input.focus();
    ack("focus");
  },

  blur: function() {
    gController.input.blur();
    ack("blur");
  },

  waitForSearch: function() {
    waitForContentSearchEvent("Search", aData => ack("waitForSearch", aData));
  },

  waitForSearchSettings: function() {
    waitForContentSearchEvent("ManageEngines",
                              aData => ack("waitForSearchSettings", aData));
  },

  mousemove: function(itemIndex) {
    let row;
    if (itemIndex == -1) {
      row = gController._table.firstChild;
    }
    else {
      let allElts = [...gController._suggestionsList.children,
                     ...gController._oneOffButtons,
                     content.document.getElementById("contentSearchSettingsButton")];
      row = allElts[itemIndex];
    }
    let event = {
      type: "mousemove",
      clickcount: 0,
    }
    row.addEventListener("mousemove", function handler() {
      row.removeEventListener("mousemove", handler);
      ack("mousemove");
    });
    content.synthesizeMouseAtCenter(row, event);
  },

  click: function(arg) {
    let eltIdx = typeof(arg) == "object" ? arg.eltIdx : arg;
    let row;
    if (eltIdx == -1) {
      row = gController._table.firstChild;
    }
    else {
      let allElts = [...gController._suggestionsList.children,
                     ...gController._oneOffButtons,
                     content.document.getElementById("contentSearchSettingsButton")];
      row = allElts[eltIdx];
    }
    let event = arg.modifiers || {};
    // synthesizeMouseAtCenter defaults to sending a mousedown followed by a
    // mouseup if the event type is not specified.
    content.synthesizeMouseAtCenter(row, event);
    ack("click");
  },

  addInputValueToFormHistory: function() {
    gController.addInputValueToFormHistory();
    ack("addInputValueToFormHistory");
  },

  addDuplicateOneOff: function() {
    let btn = gController._oneOffButtons[gController._oneOffButtons.length - 1];
    let newBtn = btn.cloneNode(true);
    btn.parentNode.appendChild(newBtn);
    gController._oneOffButtons.push(newBtn);
    ack("addDuplicateOneOff");
  },

  removeLastOneOff: function() {
    gController._oneOffButtons.pop().remove();
    ack("removeLastOneOff");
  },

  reset: function() {
    // Reset both the input and suggestions by select all + delete. If there was
    // no text entered, this won't have any effect, so also escape to ensure the
    // suggestions table is closed.
    gController.input.focus();
    content.synthesizeKey("a", { accelKey: true });
    content.synthesizeKey("VK_DELETE", {});
    content.synthesizeKey("VK_ESCAPE", {});
    ack("reset");
  },
};

function ack(aType, aData) {
  sendAsyncMessage(TEST_MSG, { type: aType, data: aData || currentState() });
}

function waitForSuggestions(cb) {
  let observer = new content.MutationObserver(() => {
    if (gController.input.getAttribute("aria-expanded") == "true") {
      observer.disconnect();
      cb();
    }
  });
  observer.observe(gController.input, {
    attributes: true,
    attributeFilter: ["aria-expanded"],
  });
}

function waitForContentSearchEvent(messageType, cb) {
  let mm = content.SpecialPowers.Cc["@mozilla.org/globalmessagemanager;1"].
    getService(content.SpecialPowers.Ci.nsIMessageListenerManager);
  mm.addMessageListener("ContentSearch", function listener(aMsg) {
    if (aMsg.data.type != messageType) {
      return;
    }
    mm.removeMessageListener("ContentSearch", listener);
    cb(aMsg.data.data);
  });
}

function currentState() {
  let state = {
    selectedIndex: gController.selectedIndex,
    selectedButtonIndex: gController.selectedButtonIndex,
    numSuggestions: gController._table.hidden ? 0 : gController.numSuggestions,
    suggestionAtIndex: [],
    isFormHistorySuggestionAtIndex: [],

    tableHidden: gController._table.hidden,

    inputValue: gController.input.value,
    ariaExpanded: gController.input.getAttribute("aria-expanded"),
  };

  if (state.numSuggestions) {
    for (let i = 0; i < gController.numSuggestions; i++) {
      state.suggestionAtIndex.push(gController.suggestionAtIndex(i));
      state.isFormHistorySuggestionAtIndex.push(
        gController.isFormHistorySuggestionAtIndex(i));
    }
  }

  return state;
}

})();
