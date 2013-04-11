/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let tempScope = {};
Cu.import("resource:///modules/devtools/Target.jsm", tempScope);
let TargetFactory = tempScope.TargetFactory;

function test() {

  // http rather than chrome to improve coverage
  const TESTCASE_URI = TEST_BASE_HTTP + "simple.gz.html";

  const Cc = Components.classes;
  const Ci = Components.interfaces;

  let toolbox;
  let tempScope = {};
  Components.utils.import("resource://gre/modules/FileUtils.jsm", tempScope);
  let FileUtils = tempScope.FileUtils;


  waitForExplicitFinish();

  addTabAndLaunchStyleEditorChromeWhenLoaded(function (aChrome) {
    let target = TargetFactory.forTab(gBrowser.selectedTab);
    toolbox = gDevTools.getToolbox(target);

    aChrome.addChromeListener({
      onEditorAdded: function (aChrome, aEditor) {
        if (aEditor.styleSheetIndex != 0) {
          return; // we want to test against the first stylesheet
        }

        if (aEditor.sourceEditor) {
          run(aEditor); // already attached to input element
        } else {
          aEditor.addActionListener({
            onAttach: run
          });
        }
      }
    });

    toolbox.once("destroyed", function onClose() {
      gChromeWindow = null;
      executeSoon(function () {
        waitForFocus(function () {
          // wait that browser has focus again
          // open StyleEditorChrome again (a new one since we closed the previous one)
          launchStyleEditorChrome(function (aChrome) {
            is(gChromeWindow.document.documentElement.hasAttribute("data-marker"),
              false,
              "opened a completely new StyleEditorChrome window");

            aChrome.addChromeListener({
              onEditorAdded: function (aChrome, aEditor) {
                if (aEditor.styleSheetIndex != 0) {
                  return; // we want to test against the first stylesheet
                }

                if (aEditor.sourceEditor) {
                  testNewChrome(aEditor); // already attached to input element
                } else {
                  aEditor.addActionListener({
                    onAttach: testNewChrome
                  });
                }
              }
            });
          });
        });
      });
    }, true);
  });

  content.location = TESTCASE_URI;

  let gFilename;

  function run(aEditor)
  {
    gFilename = FileUtils.getFile("ProfD", ["styleeditor-test.css"])

    aEditor.saveToFile(gFilename, function (aFile) {
      ok(aFile, "file got saved successfully");

      aEditor.addActionListener({
        onFlagChange: function (aEditor, aFlag) {
          if (aFlag != "unsaved") {
            return;
          }

          ok(aEditor.hasFlag("unsaved"),
            "first stylesheet has UNSAVED flag after making a change");

          // marker used to check it does not exist when we reopen
          // ie. the window we opened is indeed a new one
          gChromeWindow.document.documentElement.setAttribute("data-marker", "true");
          toolbox.destroy();
        }
      });

      waitForFocus(function () {
        // insert char so that this stylesheet has the UNSAVED flag
        EventUtils.synthesizeKey("x", {}, gChromeWindow);
      }, gChromeWindow);
    });
  }

  function testNewChrome(aEditor)
  {
    ok(aEditor.savedFile,
      "first stylesheet editor will save directly into the same file");

    is(aEditor.getFriendlyName(), gFilename.leafName,
      "first stylesheet still has the filename as it was saved");
    gFilename = null;

    ok(aEditor.hasFlag("unsaved"),
      "first stylesheet still has UNSAVED flag at reopening");

    ok(!aEditor.hasFlag("inline"),
      "first stylesheet does not have INLINE flag");

    ok(!aEditor.hasFlag("error"),
      "editor does not have error flag initially");
    let hadError = false;

    let onSaveCallback = function (aFile) {
      aEditor.addActionListener({
        onFlagChange: function (aEditor, aFlag) {
          if (!hadError && aFlag == "error") {
            ok(aEditor.hasFlag("error"),
              "editor has ERROR flag after attempting to save with invalid path");
            hadError = true;

            // save using source editor key binding (previous successful path)
            waitForFocus(function () {
              EventUtils.synthesizeKey("S", {accelKey: true}, gChromeWindow);
            }, gChromeWindow);
            return;
          }

          if (hadError && aFlag == "unsaved") {
            executeSoon(function () {
              ok(!aEditor.hasFlag("unsaved"),
                "first stylesheet has no UNSAVED flag after successful save");
              ok(!aEditor.hasFlag("error"),
                "ERROR flag has been removed since last operation succeeded");
              finish();
            });
          }
        }
      });
    }

    let os = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).OS;
    if (os == "WINNT") {
      aEditor.saveToFile("C:\\I_DO_NOT_EXIST_42\\bogus.css", onSaveCallback);
    } else {
      aEditor.saveToFile("/I_DO_NOT_EXIST_42/bogos.css", onSaveCallback);
    }
  }
}
