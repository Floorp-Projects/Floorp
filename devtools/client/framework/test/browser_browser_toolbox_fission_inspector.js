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
  await setupPreferencesForBrowserToolbox();

  const tab = await addTab(
    `data:text/html,<div id="my-div" style="color: red">Foo</div>`
  );

  // Set a custom attribute on the tab's browser, in order to easily select it in the markup view
  tab.linkedBrowser.setAttribute("test-tab", "true");

  // Be careful, this JS function is going to be executed in the browser toolbox,
  // which lives in another process. So do not try to use any scope variable!
  const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  /* global toolbox */
  const testScript = function() {
    // Force the fission pref in order to be able to pierce through the remote browser element
    // Set the pref from the toolbox process as previous test may already have created
    // the browser toolbox profile folder. Then setting the pref in Firefox process
    // won't be taken into account for the browser toolbox.
    const { Services } = ChromeUtils.import(
      "resource://gre/modules/Services.jsm"
    );
    Services.prefs.setBoolPref("devtools.browsertoolbox.fission", true);

    toolbox
      .selectTool("inspector")
      .then(async inspector => {
        const onSidebarSelect = inspector.sidebar.once("select");
        inspector.sidebar.select("computedview");
        await onSidebarSelect;

        async function select(walker, selector) {
          const nodeFront = await walker.querySelector(
            walker.rootNode,
            selector
          );
          const updated = inspector.once("inspector-updated");
          inspector.selection.setNodeFront(nodeFront);
          await updated;
          return nodeFront;
        }
        const browser = await select(
          inspector.walker,
          'browser[remote="true"][test-tab]'
        );
        const browserTarget = await browser.connectToRemoteFrame();
        const walker = (await browserTarget.getFront("inspector")).walker;
        await select(walker, "#my-div");

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
        const color = prop.valueNode.textContent;
        if (color != "rgb(255, 0, 0)") {
          throw new Error(
            "The color property of the <div> within a tab isn't red, got: " +
              color
          );
        }

        Services.prefs.setBoolPref("devtools.browsertoolbox.fission", false);
      })
      .then(() => toolbox.destroy());
  };
  env.set("MOZ_TOOLBOX_TEST_SCRIPT", "new " + testScript);
  registerCleanupFunction(() => {
    env.set("MOZ_TOOLBOX_TEST_SCRIPT", "");
  });

  const { BrowserToolboxProcess } = ChromeUtils.import(
    "resource://devtools/client/framework/ToolboxProcess.jsm"
  );
  is(
    BrowserToolboxProcess.getBrowserToolboxSessionState(),
    false,
    "No session state initially"
  );

  let closePromise;
  await new Promise(onRun => {
    closePromise = new Promise(onClose => {
      info("Opening the browser toolbox\n");
      BrowserToolboxProcess.init(onClose, onRun);
    });
  });
  ok(true, "Browser toolbox started\n");
  is(
    BrowserToolboxProcess.getBrowserToolboxSessionState(),
    true,
    "Has session state"
  );

  await closePromise;
  ok(true, "Browser toolbox process just closed");
  is(
    BrowserToolboxProcess.getBrowserToolboxSessionState(),
    false,
    "No session state after closing"
  );
});
