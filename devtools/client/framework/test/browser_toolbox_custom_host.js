/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = "data:text/html,test custom host";

function test() {
  let {Toolbox} = require("devtools/client/framework/toolbox");

  let toolbox, iframe, target;

  window.addEventListener("message", onMessage);

  iframe = document.createElement("iframe");
  document.documentElement.appendChild(iframe);

  addTab(TEST_URL).then(function (tab) {
    target = TargetFactory.forTab(tab);
    let options = {customIframe: iframe};
    gDevTools.showToolbox(target, null, Toolbox.HostType.CUSTOM, options)
             .then(testCustomHost, console.error)
             .catch(console.error);
  });

  function onMessage(event) {
    if (typeof(event.data) !== "string") {
      return;
    }
    info("onMessage: " + event.data);
    let json = JSON.parse(event.data);
    if (json.name == "toolbox-close") {
      ok("Got the `toolbox-close` message");
      window.removeEventListener("message", onMessage);
      cleanup();
    }
  }

  function testCustomHost(t) {
    toolbox = t;
    is(toolbox.win.top, window, "Toolbox is included in browser.xul");
    is(toolbox.doc, iframe.contentDocument, "Toolbox is in the custom iframe");
    executeSoon(() => gBrowser.removeCurrentTab());
  }

  function cleanup() {
    iframe.remove();

    // Even if we received "toolbox-close", the toolbox may still be destroying
    // toolbox.destroy() returns a singleton promise that ensures
    // everything is cleaned up before proceeding.
    toolbox.destroy().then(() => {
      toolbox = iframe = target = null;
      finish();
    });
  }
}
