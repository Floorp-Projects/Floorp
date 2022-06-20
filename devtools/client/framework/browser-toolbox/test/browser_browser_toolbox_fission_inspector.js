/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

/* import-globals-from ../../../inspector/test/shared-head.js */
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/inspector/test/shared-head.js",
  this
);

// On debug test machine, it takes about 50s to run the test.
requestLongerTimeout(4);

// This test is used to test fission-like features via the Browser Toolbox:
// - computed view is correct when selecting an element in a remote frame

add_task(async function() {
  // Forces the Browser Toolbox to open on the inspector by default
  await pushPref("devtools.browsertoolbox.panel", "inspector");

  const ToolboxTask = await initBrowserToolboxTask({
    enableBrowserToolboxFission: true,
  });
  await ToolboxTask.importFunctions({
    getNodeFront,
    getNodeFrontInFrames,
    selectNode,
    // selectNodeInFrames depends on selectNode, getNodeFront, getNodeFrontInFrames.
    selectNodeInFrames,
  });

  // Open the tab *after* opening the Browser Toolbox in order to force creating the remote frames
  // late and exercise frame target watching code.
  const tab = await addTab(
    `data:text/html,<div id="my-div" style="color: red">Foo</div><div id="second-div" style="color: blue">Foo</div>`
  );
  // Set a custom attribute on the tab's browser, in order to easily select it in the markup view
  tab.linkedBrowser.setAttribute("test-tab", "true");

  const color = await ToolboxTask.spawn(null, async () => {
    /* global gToolbox */
    const inspector = gToolbox.getPanel("inspector");
    const onSidebarSelect = inspector.sidebar.once("select");
    inspector.sidebar.select("computedview");
    await onSidebarSelect;

    await selectNodeInFrames(
      ['browser[remote="true"][test-tab]', "#my-div"],
      inspector
    );

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

  info("Check that the node picker can be used on element in the content page");
  await pickNodeInContentPage(
    ToolboxTask,
    tab,
    "browser[test-tab]",
    "#second-div"
  );
  const secondColor = await ToolboxTask.spawn(null, async () => {
    const inspector = gToolbox.getPanel("inspector");

    is(
      inspector.selection.nodeFront.id,
      "second-div",
      "The expected element is selected in the inspector"
    );

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
    secondColor,
    "rgb(0, 0, 255)",
    "The color property of the <div> within a tab isn't blue"
  );

  info(
    "Check that the node picker can be used for element in non-remote <browser>"
  );
  const nonRemoteUrl = "about:robots";
  const nonRemoteTab = await addTab(nonRemoteUrl);
  // Set a custom attribute on the tab's browser, in order to target it
  nonRemoteTab.linkedBrowser.setAttribute("test-tab-non-remote", "");

  // check that the browser element is indeed not remote. If that changes for about:robots,
  // this should be replaced with another page
  is(
    nonRemoteTab.linkedBrowser.hasAttribute("remote"),
    false,
    "The <browser> element for about:robots is not remote"
  );

  await pickNodeInContentPage(
    ToolboxTask,
    nonRemoteTab,
    "browser[test-tab-non-remote]",
    "#errorTryAgain"
  );

  await ToolboxTask.spawn(null, async () => {
    const inspector = gToolbox.getPanel("inspector");
    is(
      inspector.selection.nodeFront.id,
      "errorTryAgain",
      "The element inside a non-remote <browser> element is selected in the inspector"
    );
  });

  await ToolboxTask.destroy();
});

async function pickNodeInContentPage(
  ToolboxTask,
  tab,
  browserElementSelector,
  contentElementSelector
) {
  await ToolboxTask.spawn(contentElementSelector, async _selector => {
    const onPickerStarted = gToolbox.nodePicker.once("picker-started");

    // Wait until the inspector front was initialized in the target that
    // contains the element we want to pick.
    // Otherwise, even if the picker is "started", the corresponding WalkerActor
    // might not be listening to the correct pick events (WalkerActor::pick)
    const onPickerReady = new Promise(resolve => {
      gToolbox.nodePicker.on(
        "inspector-front-ready-for-picker",
        async function onFrontReady(walker) {
          if (await walker.querySelector(walker.rootNode, _selector)) {
            gToolbox.nodePicker.off(
              "inspector-front-ready-for-picker",
              onFrontReady
            );
            resolve();
          }
        }
      );
    });

    gToolbox.nodePicker.start();
    await onPickerStarted;
    await onPickerReady;

    const inspector = gToolbox.getPanel("inspector");

    // Save the promises for later tasks, in order to start listening
    // *before* hovering the element and wait for resolution *after* hovering.
    this.onPickerStopped = gToolbox.nodePicker.once("picker-stopped");
    this.onInspectorUpdated = inspector.once("inspector-updated");
  });

  // Retrieve the position of the element we want to pick in the content page
  const { x, y } = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [contentElementSelector],
    _selector => {
      const rect = content.document
        .querySelector(_selector)
        .getBoundingClientRect();
      return { x: rect.x, y: rect.y };
    }
  );

  // Synthesize the mouse event in the top level browsing context, but on the <browser>
  // element containing the tab we're looking at, at the position where should be the
  // content element.
  // We need to do this to mimick what's actually done in node-picker.js
  await EventUtils.synthesizeMouse(
    document.querySelector(browserElementSelector),
    x + 5,
    y + 5,
    {}
  );

  await ToolboxTask.spawn(null, async () => {
    info(" # Waiting for picker stop");
    await this.onPickerStopped;
    info(" # Waiting for inspector-updated");
    await this.onInspectorUpdated;

    delete this.onPickerStopped;
    delete this.onInspectorUpdated;
  });
}
