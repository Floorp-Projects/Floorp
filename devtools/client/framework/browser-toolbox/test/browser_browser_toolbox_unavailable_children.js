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

// This test is used to test a badge displayed in the markup view under content
// browser elements when switching from Multi Process mode to Parent Process
// mode.

add_task(async function() {
  // Forces the Browser Toolbox to open on the inspector by default
  await pushPref("devtools.browsertoolbox.panel", "inspector");
  await pushPref("devtools.browsertoolbox.scope", "everything");

  const tab = await addTab(
    "https://example.com/document-builder.sjs?html=<div id=pick-me>Pickme"
  );
  tab.linkedBrowser.setAttribute("test-tab", "true");

  const ToolboxTask = await initBrowserToolboxTask({
    enableBrowserToolboxFission: true,
  });

  await ToolboxTask.importFunctions({
    waitUntil,
    getNodeFront,
    selectNode,
  });

  const tabProcessID =
    tab.linkedBrowser.browsingContext.currentWindowGlobal.osPid;

  const decodedTabURI = decodeURI(tab.linkedBrowser.currentURI.spec);

  await ToolboxTask.spawn(
    [tabProcessID, isFissionEnabled(), decodedTabURI],
    async (processID, _isFissionEnabled, tabURI) => {
      /* global gToolbox */
      const inspector = gToolbox.getPanel("inspector");

      info("Select the test browser element.");
      await selectNode('browser[remote="true"][test-tab]', inspector);

      info("Retrieve the node front for selected node.");
      const browserNodeFront = inspector.selection.nodeFront;
      ok(!!browserNodeFront, "Retrieved a node front for the browser");
      is(browserNodeFront.displayName, "browser");

      // Small helper to expand containers and return the child container
      // matching the provided display name.
      async function expandContainer(container, expectedChildName) {
        info(`Expand the node expected to contain a ${expectedChildName}`);
        await inspector.markup.expandNode(container.node);
        await waitUntil(() => !!container.getChildContainers().length);

        const children = container
          .getChildContainers()
          .filter(child => child.node.displayName === expectedChildName);
        is(children.length, 1);
        return children[0];
      }

      info("Check that the corresponding markup view container has children");
      const browserContainer = inspector.markup.getContainer(browserNodeFront);
      ok(browserContainer.hasChildren);
      ok(
        !browserContainer.node.childrenUnavailable,
        "childrenUnavailable un-set"
      );
      ok(
        !browserContainer.elt.querySelector(".unavailable-children"),
        "The unavailable badge is not displayed"
      );

      // Store the asserts as a helper to reuse it later in the test.
      async function assertMarkupView() {
        info("Check that the children are #document > html > body > div");
        let container = await expandContainer(browserContainer, "#document");
        container = await expandContainer(container, "html");
        container = await expandContainer(container, "body");
        container = await expandContainer(container, "div");

        info("Select the #pick-me div");
        await selectNode(container.node, inspector);
        is(inspector.selection.nodeFront.id, "pick-me");
      }
      await assertMarkupView();

      const parentProcessScope = gToolbox.doc.querySelector(
        'input[name="chrome-debug-mode"][value="parent-process"]'
      );

      info("Switch to parent process only scope");
      const onInspectorUpdated = inspector.once("inspector-updated");
      parentProcessScope.click();
      await onInspectorUpdated;

      // Note: `getChildContainers` returns null when the container has no
      // children, instead of an empty array.
      await waitUntil(() => browserContainer.getChildContainers() === null);

      ok(!browserContainer.hasChildren, "browser container has no children");
      ok(browserContainer.node.childrenUnavailable, "childrenUnavailable set");
      ok(
        !!browserContainer.elt.querySelector(".unavailable-children"),
        "The unavailable badge is displayed"
      );

      const everythingScope = gToolbox.doc.querySelector(
        'input[name="chrome-debug-mode"][value="everything"]'
      );

      info("Switch to multi process scope");
      everythingScope.click();

      info("Wait until browserContainer has children");
      await waitUntil(() => browserContainer.hasChildren);
      ok(browserContainer.hasChildren, "browser container has children");
      ok(
        !browserContainer.node.childrenUnavailable,
        "childrenUnavailable un-set"
      );
      ok(
        !browserContainer.elt.querySelector(".unavailable-children"),
        "The unavailable badge is no longer displayed"
      );

      await assertMarkupView();
    }
  );

  await ToolboxTask.destroy();
});
