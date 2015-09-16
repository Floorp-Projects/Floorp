/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that getPanelWhenReady returns the correct panel in promise
// resolutions regardless of whether it has opened first.

const URL = "data:text/html;charset=utf8,test for host sizes";

var {Toolbox} = require("devtools/framework/toolbox");

add_task(function*() {
  // Set size prefs to make the hosts way too big, so that the size has
  // to be clamped to fit into the browser window.
  Services.prefs.setIntPref("devtools.toolbox.footer.height", 10000);
  Services.prefs.setIntPref("devtools.toolbox.sidebar.width", 10000);

  let tab = yield addTab(URL);
  let nbox = gBrowser.getNotificationBox();
  let {clientHeight: nboxHeight, clientWidth: nboxWidth} = nbox;
  let toolbox = yield gDevTools.showToolbox(TargetFactory.forTab(tab));

  is (nbox.clientHeight, nboxHeight, "Opening the toolbox hasn't changed the height of the nbox");
  is (nbox.clientWidth, nboxWidth, "Opening the toolbox hasn't changed the width of the nbox");

  let iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-bottom-iframe");
  is (iframe.clientHeight, nboxHeight - 25, "The iframe fits within the available space");

  yield toolbox.switchHost(Toolbox.HostType.SIDE);
  iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-side-iframe");
  iframe.style.minWidth = "1px"; // Disable the min width set in css
  is (iframe.clientWidth, nboxWidth - 25, "The iframe fits within the available space");

  yield cleanup(toolbox);
});

add_task(function*() {
  // Set size prefs to something reasonable, so we can check to make sure
  // they are being set properly.
  Services.prefs.setIntPref("devtools.toolbox.footer.height", 100);
  Services.prefs.setIntPref("devtools.toolbox.sidebar.width", 100);

  let tab = yield addTab(URL);
  let nbox = gBrowser.getNotificationBox();
  let {clientHeight: nboxHeight, clientWidth: nboxWidth} = nbox;
  let toolbox = yield gDevTools.showToolbox(TargetFactory.forTab(tab));

  is (nbox.clientHeight, nboxHeight, "Opening the toolbox hasn't changed the height of the nbox");
  is (nbox.clientWidth, nboxWidth, "Opening the toolbox hasn't changed the width of the nbox");

  let iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-bottom-iframe");
  is (iframe.clientHeight, 100, "The iframe is resized properly");

  yield toolbox.switchHost(Toolbox.HostType.SIDE);
  iframe = document.getAnonymousElementByAttribute(nbox, "class", "devtools-toolbox-side-iframe");
  iframe.style.minWidth = "1px"; // Disable the min width set in css
  is (iframe.clientWidth, 100, "The iframe is resized properly");

  yield cleanup(toolbox);
});

function* cleanup(toolbox) {
  Services.prefs.clearUserPref("devtools.toolbox.host");
  Services.prefs.clearUserPref("devtools.toolbox.footer.height");
  Services.prefs.clearUserPref("devtools.toolbox.sidebar.width");
  yield toolbox.destroy();
  gBrowser.removeCurrentTab();
}
