/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = "data:text/html,test browsertoolbox host";

add_task(async function () {
  const {
    Toolbox,
  } = require("resource://devtools/client/framework/toolbox.js");

  const tab = await addTab(TEST_URL);
  const options = { doc: document };
  const toolbox = await gDevTools.showToolboxForTab(tab, {
    hostType: Toolbox.HostType.BROWSERTOOLBOX,
    hostOptions: options,
  });

  is(toolbox.topWindow, window, "Toolbox is included in browser.xhtml");
  const iframe = document.querySelector(
    ".devtools-toolbox-browsertoolbox-iframe"
  );
  ok(iframe, "A toolbox iframe was created in the provided document");
  is(toolbox.doc, iframe.contentDocument, "Toolbox is in the custom iframe");

  await toolbox.destroy();
  iframe.remove();
});
