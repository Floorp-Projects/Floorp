/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();
  requestLongerTimeout(2);

  let inspector, searchBox, state, panel;
  let panelOpeningStates = [0, 3, 14, 17];
  let panelClosingStates = [2, 13, 16];

  // The various states of the inspector: [key, query]
  // [
  //  what key to press,
  //  what should be the text in the searchbox
  // ]
  let keyStates = [
    ["d", "d"],
    ["i", "di"],
    ["v", "div"],
    [".", "div."],
    ["VK_UP", "div.c1"],
    ["VK_DOWN", "div.l1"],
    ["VK_DOWN", "div.l1"],
    ["VK_BACK_SPACE", "div.l"],
    ["VK_TAB", "div.l1"],
    [" ", "div.l1 "],
    ["VK_UP", "div.l1 DIV"],
    ["VK_UP", "div.l1 DIV"],
    [".", "div.l1 DIV."],
    ["VK_TAB", "div.l1 DIV.c1"],
    ["VK_BACK_SPACE", "div.l1 DIV.c"],
    ["VK_BACK_SPACE", "div.l1 DIV."],
    ["VK_BACK_SPACE", "div.l1 DIV"],
    ["VK_BACK_SPACE", "div.l1 DI"],
    ["VK_BACK_SPACE", "div.l1 D"],
    ["VK_BACK_SPACE", "div.l1 "],
    ["VK_UP", "div.l1 DIV"],
    ["VK_BACK_SPACE", "div.l1 DI"],
    ["VK_BACK_SPACE", "div.l1 D"],
    ["VK_BACK_SPACE", "div.l1 "],
    ["VK_UP", "div.l1 DIV"],
    ["VK_UP", "div.l1 DIV"],
    ["VK_TAB", "div.l1 DIV"],
    ["VK_BACK_SPACE", "div.l1 DI"],
    ["VK_BACK_SPACE", "div.l1 D"],
    ["VK_BACK_SPACE", "div.l1 "],
    ["VK_DOWN", "div.l1 DIV"],
    ["VK_DOWN", "div.l1 SPAN"],
    ["VK_DOWN", "div.l1 SPAN"],
    ["VK_BACK_SPACE", "div.l1 SPA"],
    ["VK_BACK_SPACE", "div.l1 SP"],
    ["VK_BACK_SPACE", "div.l1 S"],
    ["VK_BACK_SPACE", "div.l1 "],
    ["VK_BACK_SPACE", "div.l1"],
    ["VK_BACK_SPACE", "div.l"],
    ["VK_BACK_SPACE", "div."],
    ["VK_BACK_SPACE", "div"],
    ["VK_BACK_SPACE", "di"],
    ["VK_BACK_SPACE", "d"],
    ["VK_BACK_SPACE", ""],
  ];

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(setupTest, content);
  }, true);

  content.location = "http://mochi.test:8888/browser/browser/devtools/inspector/test/browser_inspector_bug_831693_search_suggestions.html";

  function $(id) {
    if (id == null) return null;
    return content.document.getElementById(id);
  }

  function setupTest()
  {
    openInspector(startTest);
  }

  function startTest(aInspector)
  {
    inspector = aInspector;
    searchBox =
      inspector.panelWin.document.getElementById("inspector-searchbox");
    panel = inspector.searchSuggestions.searchPopup._list;

    focusSearchBoxUsingShortcut(inspector.panelWin, function() {
      searchBox.addEventListener("keypress", checkState, true);
      panel.addEventListener("keypress", checkState, true);
      checkStateAndMoveOn(0);
    });
  }

  function checkStateAndMoveOn(index) {
    if (index == keyStates.length) {
      finishUp();
      return;
    }

    let [key, query] = keyStates[index];
    state = index;

    info("pressing key " + key + " to get searchbox value as " + query);
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
  }

  function checkState(event) {
    if (panelOpeningStates.indexOf(state) != -1 &&
        !inspector.searchSuggestions.searchPopup.isOpen) {
      info("Panel is not open, should wait before it shows up.");
      panel.parentNode.addEventListener("popupshown", function retry() {
        panel.parentNode.removeEventListener("popupshown", retry, false);
        info("Panel is visible now");
        executeSoon(checkState);
      }, false);
      return;
    }
    else if (panelClosingStates.indexOf(state) != -1 &&
             panel.parentNode.state != "closed") {
      info("Panel is open, should wait for it to close.");
      panel.parentNode.addEventListener("popuphidden", function retry() {
        panel.parentNode.removeEventListener("popuphidden", retry, false);
        info("Panel is hidden now");
        executeSoon(checkState);
      }, false);
      return;
    }

    // Using setTimout as the "command" event fires at delay after keypress
    window.setTimeout(function() {
      let [key, query] = keyStates[state];

      if (searchBox.value == query) {
        ok(true, "The suggestion at " + state + "th step on " +
           "pressing " + key + " key is correct.");
      }
      else {
        info("value is not correct, waiting longer for state " + state +
             " with panel " + panel.parentNode.state);
        checkState();
        return;
      }
      checkStateAndMoveOn(state + 1);
    }, 200);
  }

  function finishUp() {
    searchBox = null;
    panel = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
