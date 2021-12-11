/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test about:devtools-toolbox?target which allows opening a toolbox in an
// iframe while defining which document to debug by setting a `target`
// attribute refering to the document to debug.

const { Toolbox } = require("devtools/client/framework/toolbox");

add_task(async function() {
  // iframe loads the document to debug
  const iframe = document.createXULElement("browser");
  iframe.setAttribute("type", "content");
  document.documentElement.appendChild(iframe);

  let onLoad = once(iframe, "load", true);
  iframe.setAttribute("src", "data:text/html,document to debug");
  await onLoad;
  is(iframe.contentWindow.document.body.innerHTML, "document to debug");

  // toolbox loads the toolbox document
  const toolboxIframe = document.createXULElement("iframe");
  document.documentElement.appendChild(toolboxIframe);

  // Important step to define which target to debug
  toolboxIframe.target = iframe;

  const onToolboxReady = gDevTools.once("toolbox-ready");

  onLoad = once(toolboxIframe, "load", true);
  toolboxIframe.setAttribute("src", "about:devtools-toolbox?target");
  await onLoad;

  // Also wait for toolbox-ready, as toolbox document load isn't enough, there
  // is plenty of asynchronous steps during toolbox load
  info("Waiting for toolbox-ready");
  const toolbox = await onToolboxReady;
  const { client } = toolbox.descriptorFront;

  is(
    toolbox.hostType,
    Toolbox.HostType.PAGE,
    "Host type of this toolbox shuld be Toolbox.HostType.PAGE"
  );

  const onToolboxDestroyed = gDevTools.once("toolbox-destroyed");

  info("Removing the iframes");
  toolboxIframe.remove();

  // And wait for toolbox-destroyed as toolbox unload is also full of
  // asynchronous operation that outlast unload event
  info("Waiting for toolbox-destroyed");
  await onToolboxDestroyed;
  info("Toolbox destroyed");

  // The descriptor involved with this special case won't close
  // the client, so we have to do it manually from this test.
  await client.close();

  iframe.remove();
});
