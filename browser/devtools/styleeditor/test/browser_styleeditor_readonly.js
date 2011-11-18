/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "simple.html";


let gEditorAddedCount = 0;
let gEditorReadOnlyCount = 0;
let gChromeListener = {
  onEditorAdded: function (aChrome, aEditor) {
    gEditorAddedCount++;
    if (aEditor.readOnly) {
      gEditorReadOnlyCount++;
    }

    if (gEditorAddedCount == aChrome.editors.length) {
      // continue testing after all editors are ready

      is(gEditorReadOnlyCount, 0,
         "all editors are NOT read-only initially");

      // all editors have been loaded, queue closing the content tab
      executeSoon(function () {
        gBrowser.removeCurrentTab();
      });
    }
  },
  onContentDetach: function (aChrome) {
    // check that the UI has switched to read-only
    run(aChrome);
  }
};

function test()
{
  waitForExplicitFinish();

  gBrowser.addTab(); // because we'll close the next one
  addTabAndLaunchStyleEditorChromeWhenLoaded(function (aChrome) {
    aChrome.addChromeListener(gChromeListener);
  });

  content.location = TESTCASE_URI;
}

function run(aChrome)
{
  let document = gChromeWindow.document;
  let disabledCount;
  let elements;

  disabledCount = 0;
  elements = document.querySelectorAll("button,input,select");
  for (let i = 0; i < elements.length; ++i) {
    if (elements[i].hasAttribute("disabled")) {
      disabledCount++;
    }
  }
  ok(elements.length && disabledCount == elements.length,
     "all buttons, input and select elements are disabled");

  disabledCount = 0;
  aChrome.editors.forEach(function (aEditor) {
    if (aEditor.readOnly) {
      disabledCount++;
    }
  });
  ok(aChrome.editors.length && disabledCount == aChrome.editors.length,
     "all editors are read-only");

  aChrome.removeChromeListener(gChromeListener);
  finish();
}
