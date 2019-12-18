/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/File closed/);

// On debug test slave, it takes about 50s to run the test.
requestLongerTimeout(4);

// This test is used to test fission-like features via the Browser Toolbox:
// - computed view is correct when selecting an element in a remote frame

add_task(async function() {
  const ToolboxTask = await initBrowserToolboxTask({
    enableBrowserToolboxFission: true,
  });
  await ToolboxTask.importFunctions({
    selectNodeFront,
  });

  const tab = await addTab(
    `data:text/html,<div id="my-div" style="color: red">Foo</div>`
  );

  // Set a custom attribute on the tab's browser, in order to easily select it in the markup view
  tab.linkedBrowser.setAttribute("test-tab", "true");

  const color = await ToolboxTask.spawn(null, async () => {
    /* global gToolbox */
    const inspector = await gToolbox.selectTool("inspector");
    const onSidebarSelect = inspector.sidebar.once("select");
    inspector.sidebar.select("computedview");
    await onSidebarSelect;

    const browser = await selectNodeFront(
      inspector,
      inspector.walker,
      'browser[remote="true"][test-tab]'
    );
    const browserTarget = await browser.connectToRemoteFrame();
    const walker = (await browserTarget.getFront("inspector")).walker;
    await selectNodeFront(inspector, walker, "#my-div");

    const view = inspector.getPanel("computedview").computedView;
    function getProperty(name) {
      const propertyViews = view.propertyViews;
      for (const propView of propertyViews) {
        if (propView.name == name) {
          return propView;
        }
      }
      return null;
    }
    const prop = getProperty("color");
    return prop.valueNode.textContent;
  });

  is(
    color,
    "rgb(255, 0, 0)",
    "The color property of the <div> within a tab isn't red"
  );

  await ToolboxTask.destroy();
});
