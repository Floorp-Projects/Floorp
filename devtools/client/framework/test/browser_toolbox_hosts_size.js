/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that getPanelWhenReady returns the correct panel in promise
// resolutions regardless of whether it has opened first.

const URL = "data:text/html;charset=utf8,test for host sizes";

var {Toolbox} = require("devtools/client/framework/toolbox");

add_task(async function() {
  // Set size prefs to make the hosts way too big, so that the size has
  // to be clamped to fit into the browser window.
  Services.prefs.setIntPref("devtools.toolbox.footer.height", 10000);
  Services.prefs.setIntPref("devtools.toolbox.sidebar.width", 10000);

  const tab = await addTab(URL);
  const nbox = gBrowser.getNotificationBox();
  const {clientHeight: nboxHeight, clientWidth: nboxWidth} = nbox;
  const toolbox = await gDevTools.showToolbox(TargetFactory.forTab(tab));

  is(nbox.clientHeight, nboxHeight, "Opening the toolbox hasn't changed the height of the nbox");
  is(nbox.clientWidth, nboxWidth, "Opening the toolbox hasn't changed the width of the nbox");

  let iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-bottom-iframe");
  is(iframe.clientHeight, nboxHeight - 25, "The iframe fits within the available space");

  await toolbox.switchHost(Toolbox.HostType.RIGHT);
  iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-side-iframe");
  iframe.style.minWidth = "1px"; // Disable the min width set in css
  is(iframe.clientWidth, nboxWidth - 25, "The iframe fits within the available space");

  await cleanup(toolbox);
});

add_task(async function() {
  // Set size prefs to something reasonable, so we can check to make sure
  // they are being set properly.
  Services.prefs.setIntPref("devtools.toolbox.footer.height", 100);
  Services.prefs.setIntPref("devtools.toolbox.sidebar.width", 100);

  const tab = await addTab(URL);
  const nbox = gBrowser.getNotificationBox();
  const {clientHeight: nboxHeight, clientWidth: nboxWidth} = nbox;
  const toolbox = await gDevTools.showToolbox(TargetFactory.forTab(tab));

  is(nbox.clientHeight, nboxHeight, "Opening the toolbox hasn't changed the height of the nbox");
  is(nbox.clientWidth, nboxWidth, "Opening the toolbox hasn't changed the width of the nbox");

  let iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-bottom-iframe");
  is(iframe.clientHeight, 100, "The iframe is resized properly");

  await toolbox.switchHost(Toolbox.HostType.RIGHT);
  iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-side-iframe");
  iframe.style.minWidth = "1px"; // Disable the min width set in css
  is(iframe.clientWidth, 100, "The iframe is resized properly");

  await cleanup(toolbox);
});

async function cleanup(toolbox) {
  Services.prefs.clearUserPref("devtools.toolbox.host");
  Services.prefs.clearUserPref("devtools.toolbox.footer.height");
  Services.prefs.clearUserPref("devtools.toolbox.sidebar.width");
  await toolbox.destroy();
  gBrowser.removeCurrentTab();
}
