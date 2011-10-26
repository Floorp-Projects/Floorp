/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "simple.html";


function test()
{
  waitForExplicitFinish();

  addTabAndLaunchStyleEditorChromeWhenLoaded(function (aChrome) {
    aChrome.addChromeListener({
      onContentAttach: run
    });
    if (aChrome.isContentAttached) {
      run(aChrome);
    }
  });

  content.location = TESTCASE_URI;
}

function getFilteredItemsCount(nav)
{
  let matches = nav.querySelectorAll("*.splitview-filtered");
  return matches ? matches.length : 0;
}

function run(aChrome)
{
  aChrome.editors[0].addActionListener({onAttach: onFirstEditorAttach});
  aChrome.editors[1].addActionListener({onAttach: onSecondEditorAttach});
}

function onFirstEditorAttach(aEditor)
{
  let filter = gChromeWindow.document.querySelector(".splitview-filter");
  // force the command event on input since it is not possible to disable
  // the search textbox's timeout.
  let forceCommandEvent = function forceCommandEvent() {
    let evt = gChromeWindow.document.createEvent("XULCommandEvent");
    evt.initCommandEvent("command", true, true, gChromeWindow, 0, false, false,
                         false, false, null);
    filter.dispatchEvent(evt);
  }
  filter.addEventListener("input", forceCommandEvent, false);

  let nav = gChromeWindow.document.querySelector(".splitview-nav");
  nav.focus();

  is(getFilteredItemsCount(nav), 0,
     "there is 0 filtered item initially");

  waitForFocus(function () {
    // Search [s] (type-on-search since we focused nav above - not filter directly)
    EventUtils.synthesizeKey("s", {}, gChromeWindow);

    // the search space is "simple.css" and "inline stylesheet #1" (2 sheets)
    is(getFilteredItemsCount(nav), 0,
       "there is 0 filtered item if searching for 's'");

    EventUtils.synthesizeKey("i", {}, gChromeWindow); // Search [si]

    is(getFilteredItemsCount(nav), 1, // inline stylesheet is filtered
       "there is 1 filtered item if searching for 's'");

    // use uppercase to check that filtering is case-insensitive
    EventUtils.synthesizeKey("X", {}, gChromeWindow); // Search [siX]
    is(getFilteredItemsCount(nav), 2,
       "there is 2 filtered items if searching for 's'"); // no match

    // clear the search
    EventUtils.synthesizeKey("VK_ESCAPE", {}, gChromeWindow);

    is(filter.value, "",
       "filter is back to empty");
    is(getFilteredItemsCount(nav), 0,
       "there is 0 filtered item when filter is empty again");

    for each (let c in "inline") {
      EventUtils.synthesizeKey(c, {}, gChromeWindow);
    }

    is(getFilteredItemsCount(nav), 1, // simple.css is filtered
       "there is 1 filtered item if searching for 'inline'");

    // auto-select the only result (enter the editor)
    EventUtils.synthesizeKey("VK_ENTER", {}, gChromeWindow);

    filter.removeEventListener("input", forceCommandEvent, false);
  }, gChromeWindow);
}

function onSecondEditorAttach(aEditor)
{
  ok(aEditor.sourceEditor.hasFocus(),
     "second editor has been selected and focused automatically.");

  finish();
}
