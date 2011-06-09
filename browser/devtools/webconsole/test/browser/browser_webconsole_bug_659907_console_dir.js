/* vim:set ts=2 sw=2 sts=2 et: */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that console.dir works as intended.

function test() {
  addTab("data:text/html,Web Console test for bug 659907: Expand console " +
         "object with a dir method");
  browser.addEventListener("load", onLoad, true);
}

function onLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  openConsole();
  let hudId = HUDService.getHudIdByWindow(content);
  let hud = HUDService.hudReferences[hudId];
  outputNode = hud.outputNode;
  content.console.dir(content.document);
  findLogEntry("[object HTMLDocument");
  let msg = outputNode.querySelectorAll(".webconsole-msg-inspector");
  is(msg.length, 1, "one message node displayed");
  let rows = msg[0].propertyTreeView._rows;
  let foundQSA = false;
  let foundLocation = false;
  let foundWrite = false;
  for (let i = 0; i < rows.length; i++) {
    if (rows[i].display == "querySelectorAll: function querySelectorAll()") {
      foundQSA = true;
    }
    else if (rows[i].display  == "location: Object") {
      foundLocation = true;
    }
    else if (rows[i].display  == "write: function write()") {
      foundWrite = true;
    }
  }
  ok(foundQSA, "found document.querySelectorAll");
  ok(foundLocation, "found document.location");
  ok(foundWrite, "found document.write");
  finishTest();
}
