/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test()
{
  waitForExplicitFinish();

  let inspector, searchBox, state;
  let keypressStates = [3,4,8,18,19,20,21,22];

  // The various states of the inspector: [key, id, isValid]
  // [
  //  what key to press,
  //  what id should be selected after the keypress,
  //  is the searched text valid selector
  // ]
  let keyStates = [
    ["d", "b1", false],
    ["i", "b1", false],
    ["v", "d1", true],
    ["VK_DOWN", "d2", true],
    ["VK_ENTER", "d1", true],
    [".", "d1", true],
    ["c", "d1", false],
    ["1", "d2", true],
    ["VK_DOWN", "d2", true],
    ["VK_BACK_SPACE", "d2", false],
    ["VK_BACK_SPACE", "d2", false],
    ["VK_BACK_SPACE", "d1", true],
    ["VK_BACK_SPACE", "d1", false],
    ["VK_BACK_SPACE", "d1", false],
    ["VK_BACK_SPACE", "d1", true],
    [".", "d1", true],
    ["c", "d1", false],
    ["1", "d2", true],
    ["VK_DOWN", "s2", true],
    ["VK_DOWN", "p1", true],
    ["VK_UP", "s2", true],
    ["VK_UP", "d2", true],
    ["VK_UP", "p1", true],
    ["VK_BACK_SPACE", "p1", false],
    ["2", "p3", true],
    ["VK_BACK_SPACE", "p3", false],
    ["VK_BACK_SPACE", "p3", false],
    ["VK_BACK_SPACE", "p3", true],
    ["r", "p3", false],
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
    inspector.selection.setNode($("b1"));
    searchBox =
      inspector.panelWin.document.getElementById("inspector-searchbox");

    focusSearchBoxUsingShortcut(inspector.panelWin, function() {
      searchBox.addEventListener("command", checkState, true);
      searchBox.addEventListener("keypress", checkState, true);
      checkStateAndMoveOn(0);
    });
  }

  function checkStateAndMoveOn(index) {
    if (index == keyStates.length) {
      finishUp();
      return;
    }

    let [key, id, isValid] = keyStates[index];
    state = index;

    info("pressing key " + key + " to get id " + id);
    EventUtils.synthesizeKey(key, {}, inspector.panelWin);
  }

  function checkState(event) {
    if (event.type == "keypress" && keypressStates.indexOf(state) == -1) {
      return;
    }
    executeSoon(function() {
      let [key, id, isValid] = keyStates[state];
      info(inspector.selection.node.id + " is selected with text " +
           inspector.searchBox.value);
      is(inspector.selection.node, $(id),
         "Correct node is selected for state " + state);
      is(!searchBox.classList.contains("devtools-no-search-result"), isValid,
         "Correct searchbox result state for state " + state);
      checkStateAndMoveOn(state + 1);
    });
  }

  function finishUp() {
    searchBox = null;
    gBrowser.removeCurrentTab();
    finish();
  }
}
