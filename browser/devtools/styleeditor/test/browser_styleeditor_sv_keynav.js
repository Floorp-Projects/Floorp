/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "four.html";


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

let gChrome;

function run(aChrome)
{
  gChrome = aChrome;
  aChrome.editors[0].addActionListener({onAttach: onEditor0Attach});
  aChrome.editors[2].addActionListener({onAttach: onEditor2Attach});
}

function getStylesheetNameLinkFor(aEditor)
{
  return gChrome.getSummaryElementForEditor(aEditor).querySelector(".stylesheet-name");
}

function onEditor0Attach(aEditor)
{
  waitForFocus(function () {
    let summary = gChrome.getSummaryElementForEditor(aEditor);
    EventUtils.synthesizeMouseAtCenter(summary, {}, gChromeWindow);

    let item = getStylesheetNameLinkFor(gChrome.editors[0]);
    is(gChromeWindow.document.activeElement, item,
       "editor 0 item is the active element");

    EventUtils.synthesizeKey("VK_DOWN", {}, gChromeWindow);
    item = getStylesheetNameLinkFor(gChrome.editors[1]);
    is(gChromeWindow.document.activeElement, item,
       "editor 1 item is the active element");

    EventUtils.synthesizeKey("VK_HOME", {}, gChromeWindow);
    item = getStylesheetNameLinkFor(gChrome.editors[0]);
    is(gChromeWindow.document.activeElement, item,
       "fist editor item is the active element");

    EventUtils.synthesizeKey("VK_END", {}, gChromeWindow);
    item = getStylesheetNameLinkFor(gChrome.editors[3]);
    is(gChromeWindow.document.activeElement, item,
       "last editor item is the active element");

    EventUtils.synthesizeKey("VK_UP", {}, gChromeWindow);
    item = getStylesheetNameLinkFor(gChrome.editors[2]);
    is(gChromeWindow.document.activeElement, item,
       "editor 2 item is the active element");

    EventUtils.synthesizeKey("VK_RETURN", {}, gChromeWindow);
    // this will attach and give focus editor 2
  }, gChromeWindow);
}

function onEditor2Attach(aEditor)
{
  ok(aEditor.sourceEditor.hasFocus(),
     "editor 2 has focus");

  gChrome = null;
  finish();
}
