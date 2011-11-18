/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TESTCASE_URI = TEST_BASE + "simple.html";


function test()
{
  waitForExplicitFinish();

  addTabAndLaunchStyleEditorChromeWhenLoaded(function (aChrome) {
    aChrome.addChromeListener({
      onContentAttach: run,
      onEditorAdded: testEditorAdded
    });
    if (aChrome.isContentAttached) {
      run(aChrome);
    }
  });

  content.location = TESTCASE_URI;
}

let gContentAttachHandled = false;
function run(aChrome)
{
  gContentAttachHandled = true;
  is(aChrome.contentWindow.document.readyState, "complete",
     "content document is complete");

  let SEC = gChromeWindow.styleEditorChrome;
  is(SEC, aChrome, "StyleEditorChrome object exists as new window property");

  ok(gChromeWindow.document.title.indexOf("simple testcase") >= 0,
     "the Style Editor window title contains the document's title");

  // check editors are instantiated
  is(SEC.editors.length, 2,
     "there is two StyleEditor instances managed");
  ok(SEC.editors[0].styleSheetIndex < SEC.editors[1].styleSheetIndex,
     "editors are ordered by styleSheetIndex");

  // check StyleEditorChrome is a singleton wrt to the same DOMWindow
  let chromeWindow = StyleEditor.openChrome();
  is(chromeWindow, gChromeWindow,
     "attempt to edit the same document returns the same Style Editor window");
}

let gEditorAddedCount = 0;
function testEditorAdded(aChrome, aEditor)
{
  if (!gEditorAddedCount) {
    is(gContentAttachHandled, true,
       "ContentAttach event triggered before EditorAdded");
  }

  if (aEditor.styleSheetIndex == 0) {
    gEditorAddedCount++;
    testFirstStyleSheetEditor(aChrome, aEditor);
  }
  if (aEditor.styleSheetIndex == 1) {
    gEditorAddedCount++;
    testSecondStyleSheetEditor(aChrome, aEditor);
  }

  if (gEditorAddedCount == 2) {
    finish();
  }
}

function testFirstStyleSheetEditor(aChrome, aEditor)
{
  //testing TESTCASE's simple.css stylesheet
  is(aEditor.styleSheetIndex, 0,
     "first stylesheet is at index 0");

  is(aEditor, aChrome.editors[0],
     "first stylesheet corresponds to StyleEditorChrome.editors[0]");

  ok(!aEditor.hasFlag("inline"),
     "first stylesheet does not have INLINE flag");

  let summary = aChrome.getSummaryElementForEditor(aEditor);
  ok(!summary.classList.contains("inline"),
     "first stylesheet UI does not have INLINE class");

  let name = summary.querySelector(".stylesheet-name").textContent;
  is(name, "simple.css",
     "first stylesheet's name is `simple.css`");

  let ruleCount = summary.querySelector(".stylesheet-rule-count").textContent;
  is(parseInt(ruleCount), 1,
     "first stylesheet UI shows rule count as 1");

  ok(summary.classList.contains("splitview-active"),
     "first stylesheet UI is focused/active");
}

function testSecondStyleSheetEditor(aChrome, aEditor)
{
  //testing TESTCASE's inline stylesheet
  is(aEditor.styleSheetIndex, 1,
     "second stylesheet is at index 1");

  is(aEditor, aChrome.editors[1],
     "second stylesheet corresponds to StyleEditorChrome.editors[1]");

  ok(aEditor.hasFlag("inline"),
     "second stylesheet has INLINE flag");

  let summary = aChrome.getSummaryElementForEditor(aEditor);
  ok(summary.classList.contains("inline"),
     "second stylesheet UI has INLINE class");

  let name = summary.querySelector(".stylesheet-name").textContent;
  ok(/^<.*>$/.test(name),
     "second stylesheet's name is surrounded by `<>`");

  let ruleCount = summary.querySelector(".stylesheet-rule-count").textContent;
  is(parseInt(ruleCount), 3,
     "second stylesheet UI shows rule count as 3");

  ok(!summary.classList.contains("splitview-active"),
     "second stylesheet UI is NOT focused/active");
}
