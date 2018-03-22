/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/test-bug-632275-getters.html";

var getterValue = null;

function test() {
  loadTab(TEST_URI).then(() => {
    openConsole().then(consoleOpened);
  });
}

async function consoleOpened(hud) {
  getterValue = await ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    return content.wrappedJSObject.document.foobar._val;
  });
  hud.jsterm.execute("console.dir(document)");

  let onOpen = onViewOpened.bind(null, hud);
  hud.jsterm.once("variablesview-fetched", onOpen);
}

async function onViewOpened(hud, view) {
  await findVariableViewProperties(view, [
    { name: /^(width|height)$/, dontMatch: 1 },
    { name: "foobar._val", value: getterValue },
    { name: "foobar.val", isGetter: true },
  ], { webconsole: hud });

  await ContentTask.spawn(gBrowser.selectedBrowser, getterValue, function(value) {
    let doc = content.wrappedJSObject.document;
    is(doc.foobar._val, value, "getter did not execute");
    is(doc.foobar.val, value + 1, "getter executed");
    is(doc.foobar._val, value + 1, "getter executed (recheck)");
  });

  let textContent = hud.outputNode.textContent;
  is(textContent.indexOf("document.body.client"), -1,
     "no document.width/height warning displayed");

  getterValue = null;
  finishTest();
}
