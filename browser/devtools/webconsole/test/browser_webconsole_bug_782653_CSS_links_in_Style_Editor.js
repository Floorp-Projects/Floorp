/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 * ***** END LICENSE BLOCK ***** */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test" +
                 "/test-bug-782653-css-errors.html";

let nodes, hud, SEC;

function test()
{
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, testViewSource);
  }, true);
}

function testViewSource(aHud)
{
  hud = aHud;

  registerCleanupFunction(function() {
    nodes = hud = SEC = null;
  });

  let selector = ".webconsole-msg-cssparser .webconsole-location";

  waitForSuccess({
    name: "find the location node",
    validatorFn: function()
    {
      return hud.outputNode.querySelector(selector);
    },
    successFn: function()
    {
      nodes = hud.outputNode.querySelectorAll(selector);
      is(nodes.length, 2, "correct number of css messages");

      Services.ww.registerNotification(observer);

      EventUtils.sendMouseEvent({ type: "click" }, nodes[0]);
    },
    failureFn: finishTest,
  });
}

let observer = {
  observe: function(aSubject, aTopic, aData)
  {
    if (aTopic != "domwindowopened") {
      return;
    }
    Services.ww.unregisterNotification(observer);
    info("Style Editor window was opened in response to clicking " +
         "the location node");

    function sheetForNode(aNode)
    {
      let href = aNode.getAttribute("title");
      let sheet, i = 0;
      while((sheet = content.document.styleSheets[i++])) {
        if (sheet.href == href) {
          return sheet;
        }
      }
    }

    executeSoon(function() {
      let styleEditorWin = window.StyleEditor.StyleEditorManager
                          .getEditorForWindow(content.window);
      ok(styleEditorWin, "Style Editor window is defined");

      waitForFocus(function() {
        SEC = styleEditorWin.styleEditorChrome;
        ok(SEC, "Style Editor Chrome is defined");

        let sheet = sheetForNode(nodes[0]);
        ok(sheet, "sheet found");
        let line = nodes[0].sourceLine;
        ok(line, "found source line");

        checkStyleEditorForSheetAndLine(sheet, line - 1, function() {
          let sheet = sheetForNode(nodes[1]);
          ok(sheet, "sheet found");
          let line = nodes[1].sourceLine;
          ok(line, "found source line");

          EventUtils.sendMouseEvent({ type: "click" }, nodes[1]);

          checkStyleEditorForSheetAndLine(sheet, line - 1, function() {
            window.StyleEditor.toggle();
            finishTest();
          });
        });
      }, styleEditorWin);
    });
  }
};

function checkStyleEditorForSheetAndLine(aStyleSheet, aLine, aCallback)
{
  let editor = null;

  let performLineCheck = {
    name: "source editor load",
    validatorFn: function()
    {
      return editor.sourceEditor;
    },
    successFn: function()
    {
      is(editor.sourceEditor.getCaretPosition().line, aLine,
         "correct line is selected");
      is(SEC.selectedStyleSheetIndex, editor.styleSheetIndex,
         "correct stylesheet is selected in the editor");

      executeSoon(aCallback);
    },
    failureFn: aCallback,
  };

  waitForSuccess({
    name: "editor for stylesheet",
    validatorFn: function()
    {
      for (let item of SEC.editors) {
        if (item.styleSheet == aStyleSheet) {
          editor = item;
          return true;
        }
      }
      return false;
    },
    successFn: function()
    {
      waitForSuccess(performLineCheck);
    },
    failureFn: finishTest,
  });
}
