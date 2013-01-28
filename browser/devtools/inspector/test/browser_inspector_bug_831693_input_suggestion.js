/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  let inspector, searchBox, state, popup;

  // The various states of the inspector: [key, suggestions array]
  // [
  //  what key to press,
  //  suggestions array with count [
  //    [suggestion1, count1], [suggestion2] ...
  //  ] count can be left to represent 1
  // ]
  let keyStates = [
    ["d", [["div", 2]]],
    ["i", [["div", 2]]],
    ["v", []],
    [".", [["div.c1"]]],
    ["VK_BACK_SPACE", []],
    ["#", [["div#d1"], ["div#d2"]]],
    ["VK_BACK_SPACE", []],
    ["VK_BACK_SPACE", [["div", 2]]],
    ["VK_BACK_SPACE", [["div", 2]]],
    ["VK_BACK_SPACE", []],
    [".", [[".c1", 3], [".c2"]]],
    ["c", [[".c1", 3], [".c2"]]],
    ["2", []],
    ["VK_BACK_SPACE", [[".c1", 3], [".c2"]]],
    ["1", []],
    ["#", [["#d2"], ["#p1"], ["#s2"]]],
    ["VK_BACK_SPACE", []],
    ["VK_BACK_SPACE", [[".c1", 3], [".c2"]]],
    ["VK_BACK_SPACE", [[".c1", 3], [".c2"]]],
    ["VK_BACK_SPACE", []],
    ["#", [["#b1"], ["#d1"], ["#d2"], ["#p1"], ["#p2"], ["#p3"], ["#s1"], ["#s2"]]],
    ["p", [["#p1"], ["#p2"], ["#p3"]]],
    ["VK_BACK_SPACE", [["#b1"], ["#d1"], ["#d2"], ["#p1"], ["#p2"], ["#p3"], ["#s1"], ["#s2"]]],
    ["VK_BACK_SPACE", []],
  ];

  gBrowser.selectedTab = gBrowser.addTab();
  gBrowser.selectedBrowser.addEventListener("load", function onload() {
    gBrowser.selectedBrowser.removeEventListener("load", onload, true);
    waitForFocus(setupTest, content);
  }, true);

  content.location = "http://mochi.test:8888/browser/browser/devtools/inspector/test/browser_inspector_bug_650804_search.html";

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
    popup = inspector.searchSuggestions.searchPopup;

    focusSearchBoxUsingShortcut(inspector.panelWin, function() {
      searchBox.addEventListener("command", checkState, true);
      checkStateAndMoveOn(0);
    });
  }

  function checkStateAndMoveOn(index) {
    if (index == keyStates.length) {
      finishUp();
      return;
    }

    let [key, suggestions] = keyStates[index];
    state = index;

    info("pressing key " + key + " to get suggestions " +
         JSON.stringify(suggestions));
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
  }

  function checkState(event) {
    executeSoon(function() {
      let [key, suggestions] = keyStates[state];
      let actualSuggestions = popup.getItems();
      is(popup._panel.state == "open" || popup._panel.state == "showing"
         ? actualSuggestions.length: 0, suggestions.length,
         "There are expected number of suggestions at " + state + "th step.");
      actualSuggestions = actualSuggestions.reverse();
      for (let i = 0; i < suggestions.length; i++) {
        is(suggestions[i][0], actualSuggestions[i].label,
           "The suggestion at " + i + "th index for " + state +
           "th step is correct.")
        is(suggestions[i][1] || 1, actualSuggestions[i].count,
           "The count for suggestion at " + i + "th index for " + state +
           "th step is correct.")
      }
      checkStateAndMoveOn(state + 1);
    });
  }

  function finishUp() {
    searchBox = null;
    popup = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
