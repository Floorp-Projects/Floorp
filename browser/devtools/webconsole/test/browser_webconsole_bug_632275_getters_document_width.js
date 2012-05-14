/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URI = "http://example.com/browser/browser/devtools/webconsole/test/test-bug-632275-getters.html";

function test() {
  addTab(TEST_URI);
  browser.addEventListener("load", tabLoaded, true);
}

function tabLoaded() {
  browser.removeEventListener("load", tabLoaded, true);
  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  let HUD = HUDService.hudReferences[hudId];
  let jsterm = HUD.jsterm;

  let doc = content.wrappedJSObject.document;

  let panel = jsterm.openPropertyPanel("Test1", doc);

  let rows = panel.treeView._rows;
  let find = function(regex) {
    return rows.some(function(row) {
      return regex.test(row.display);
    });
  };

  ok(!find(/^(width|height):/), "no document.width/height");

  panel.destroy();

  let getterValue = doc.foobar._val;

  panel = jsterm.openPropertyPanel("Test2", doc.foobar);
  rows = panel.treeView._rows;

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
