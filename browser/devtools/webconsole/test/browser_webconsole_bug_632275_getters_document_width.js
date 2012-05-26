/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-632275-getters.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", function onLoad() {
    browser.removeEventListener("load", onLoad, true);
    openConsole(null, consoleOpened);
  }, true);
}

function consoleOpened(HUD) {
  let jsterm = HUD.jsterm;

  let doc = content.wrappedJSObject.document;

  let panel = jsterm.openPropertyPanel({ data: { object: doc }});

  let view = panel.treeView;
  let find = function(regex) {
    for (let i = 0; i < view.rowCount; i++) {
      if (regex.test(view.getCellText(i))) {
        return true;
      }
    }
    return false;
  };

  ok(!find(/^(width|height):/), "no document.width/height");

  panel.destroy();

  let getterValue = doc.foobar._val;

  panel = jsterm.openPropertyPanel({ data: { object: doc.foobar }});
  view = panel.treeView;

  is(getterValue, doc.foobar._val, "getter did not execute");
  is(getterValue+1, doc.foobar.val, "getter executed");
  is(getterValue+1, doc.foobar._val, "getter executed (recheck)");

  ok(find(/^val: Getter$/),
     "getter is properly displayed");

  ok(find(new RegExp("^_val: " + getterValue + "$")),
     "getter _val is properly displayed");

  panel.destroy();

  executeSoon(function() {
    let textContent = HUD.outputNode.textContent;
    is(textContent.indexOf("document.body.client"), -1,
       "no document.width/height warning displayed");

    finishTest();
  });
}
