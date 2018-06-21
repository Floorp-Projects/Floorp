/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = "data:text/html,test custom host";

add_task(async function() {
  const { Toolbox } = require("devtools/client/framework/toolbox");

  const messageReceived = new Promise(resolve => {
    function onMessage(event) {
      if (!event.data) {
        return;
      }
      const msg = event.data;
      info(`onMessage: ${JSON.stringify(msg)}`);
      switch (msg.name) {
        case "toolbox-close":
          ok(true, "Got the `toolbox-close` message");
          window.removeEventListener("message", onMessage);
          resolve();
          break;
      }
    }
    window.addEventListener("message", onMessage);
  });

  let iframe = document.createElement("iframe");
  document.documentElement.appendChild(iframe);

  const tab = await addTab(TEST_URL);
  let target = TargetFactory.forTab(tab);
  const options = { customIframe: iframe };
  let toolbox = await gDevTools.showToolbox(target, null, Toolbox.HostType.CUSTOM, options);

  is(toolbox.win.top, window, "Toolbox is included in browser.xul");
  is(toolbox.doc, iframe.contentDocument, "Toolbox is in the custom iframe");

  iframe.remove();
  await toolbox.destroy();
  await messageReceived;

  iframe = toolbox = target = null;
});
