/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that getPanelWhenReady returns the correct panel in promise
// resolutions regardless of whether it has opened first.

const URL = "data:text/html;charset=utf8,test for host sizes";

var { Toolbox } = require("devtools/client/framework/toolbox");

add_task(async function() {
  // Set size prefs to make the hosts way too big, so that the size has
  // to be clamped to fit into the browser window.
  Services.prefs.setIntPref("devtools.toolbox.footer.height", 10000);
  Services.prefs.setIntPref("devtools.toolbox.sidebar.width", 10000);

  const tab = await addTab(URL);
  const panel = gBrowser.getPanel();
  const { clientHeight: panelHeight, clientWidth: panelWidth } = panel;
  const toolbox = await gDevTools.showToolboxForTab(tab);

  is(
    panel.clientHeight,
    panelHeight,
    "Opening the toolbox hasn't changed the height of the panel"
  );
  is(
    panel.clientWidth,
    panelWidth,
    "Opening the toolbox hasn't changed the width of the panel"
  );

  let iframe = panel.querySelector(".devtools-toolbox-bottom-iframe");
  is(
    iframe.clientHeight,
    panelHeight - 25,
    "The iframe fits within the available space"
  );

  await toolbox.switchHost(Toolbox.HostType.RIGHT);
  iframe = panel.querySelector(".devtools-toolbox-side-iframe");
  iframe.style.minWidth = "1px"; // Disable the min width set in css
  is(
    iframe.clientWidth,
    panelWidth - 25,
    "The iframe fits within the available space"
  );

  // on shutdown, the sidebar width will be set to the clientWidth of the iframe
  const expectedWidth = iframe.clientWidth;
  await cleanup(toolbox);
  // Wait until the toolbox-host-manager was destroyed and updated the preferences
  // to avoid side effects in the next test.
  await waitUntil(() => {
    const savedWidth = Services.prefs.getIntPref(
      "devtools.toolbox.sidebar.width"
    );
    return savedWidth === expectedWidth;
  });
});

add_task(async function() {
  // Set size prefs to something reasonable, so we can check to make sure
  // they are being set properly.
  Services.prefs.setIntPref("devtools.toolbox.footer.height", 100);
  Services.prefs.setIntPref("devtools.toolbox.sidebar.width", 100);

  const tab = await addTab(URL);
  const panel = gBrowser.getPanel();
  const { clientHeight: panelHeight, clientWidth: panelWidth } = panel;
  const toolbox = await gDevTools.showToolboxForTab(tab);

  is(
    panel.clientHeight,
    panelHeight,
    "Opening the toolbox hasn't changed the height of the panel"
  );
  is(
    panel.clientWidth,
    panelWidth,
    "Opening the toolbox hasn't changed the width of the panel"
  );

  let iframe = panel.querySelector(".devtools-toolbox-bottom-iframe");
  is(iframe.clientHeight, 100, "The iframe is resized properly");
  const horzSplitter = panel.querySelector(".devtools-horizontal-splitter");
  dragElement(horzSplitter, { startX: 1, startY: 1, deltaX: 0, deltaY: -50 });
  is(iframe.clientHeight, 150, "The iframe was resized by the splitter");

  await toolbox.switchHost(Toolbox.HostType.RIGHT);
  iframe = panel.querySelector(".devtools-toolbox-side-iframe");
  iframe.style.minWidth = "1px"; // Disable the min width set in css
  is(iframe.clientWidth, 100, "The iframe is resized properly");

  info("Resize the toolbox manually by 50 pixels");
  const sideSplitter = panel.querySelector(".devtools-side-splitter");
  dragElement(sideSplitter, { startX: 1, startY: 1, deltaX: -50, deltaY: 0 });
  is(iframe.clientWidth, 150, "The iframe was resized by the splitter");

  await cleanup(toolbox);
});

function dragElement(el, { startX, startY, deltaX, deltaY }) {
  const endX = startX + deltaX;
  const endY = startY + deltaY;
  EventUtils.synthesizeMouse(el, startX, startY, { type: "mousedown" }, window);
  EventUtils.synthesizeMouse(el, endX, endY, { type: "mousemove" }, window);
  EventUtils.synthesizeMouse(el, endX, endY, { type: "mouseup" }, window);
}

async function cleanup(toolbox) {
  Services.prefs.clearUserPref("devtools.toolbox.host");
  Services.prefs.clearUserPref("devtools.toolbox.footer.height");
  Services.prefs.clearUserPref("devtools.toolbox.sidebar.width");
  await toolbox.destroy();
  gBrowser.removeCurrentTab();
}
