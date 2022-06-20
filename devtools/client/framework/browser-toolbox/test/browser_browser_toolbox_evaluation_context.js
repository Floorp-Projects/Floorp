/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

// On debug test machine, it takes about 50s to run the test.
requestLongerTimeout(4);

// This test is used to test fission-like features via the Browser Toolbox:
// - the evaluation context selector in the console show the right targets
// - the iframe dropdown also show the right targets
// - both are updated accordingly when toggle to parent-process only scope

add_task(async function() {
  // Forces the Browser Toolbox to open on the console by default
  await pushPref("devtools.browsertoolbox.panel", "webconsole");
  await pushPref("devtools.webconsole.input.context", true);
  // Force EFT to have targets for all WindowGlobals
  await pushPref("devtools.every-frame-target.enabled", true);

  // Open the test *before* opening the Browser toolbox in order to have the right target title.
  // Once created, the target won't update its title, and so would be "New Tab", instead of "Test tab"
  const tab = await addTab(
    "https://example.com/document-builder.sjs?html=<html><title>Test tab</title></html>"
  );

  const ToolboxTask = await initBrowserToolboxTask({
    enableBrowserToolboxFission: true,
  });

  await ToolboxTask.importFunctions({
    waitUntil,
    getContextLabels,
    getFramesLabels,
  });

  const tabProcessID =
    tab.linkedBrowser.browsingContext.currentWindowGlobal.osPid;

  const decodedTabURI = decodeURI(tab.linkedBrowser.currentURI.spec);

  await ToolboxTask.spawn(
    [tabProcessID, isFissionEnabled(), decodedTabURI],
    async (processID, _isFissionEnabled, tabURI) => {
      /* global gToolbox */
      const { hud } = await gToolbox.getPanel("webconsole");

      const evaluationContextSelectorButton = hud.ui.outputNode.querySelector(
        ".webconsole-evaluation-selector-button"
      );

      is(
        !!evaluationContextSelectorButton,
        true,
        "The evaluation context selector is visible"
      );
      is(
        evaluationContextSelectorButton.innerText,
        "Top",
        "The button has the expected 'Top' text"
      );

      const labelTexts = getContextLabels(gToolbox);

      const expectedTitle = _isFissionEnabled
        ? `(pid ${processID}) https://example.com`
        : `(pid ${processID}) web`;
      ok(
        labelTexts.includes(expectedTitle),
        `${processID} content process visible in the execution context (${labelTexts})`
      );

      ok(
        labelTexts.includes(`Test tab`),
        `Test tab is visible in the execution context (${labelTexts})`
      );

      // Also assert the behavior of the iframe dropdown
      info("Check the iframe dropdown, start by opening it");
      const btn = gToolbox.doc.getElementById("command-button-frames");
      btn.click();

      const panel = gToolbox.doc.getElementById("command-button-frames-panel");
      ok(panel, "popup panel has created.");
      await waitUntil(
        () => panel.classList.contains("tooltip-visible"),
        "Wait for the menu to be displayed"
      );

      is(
        getFramesLabels(gToolbox)[0],
        "chrome://browser/content/browser.xhtml",
        "The iframe dropdown lists first browser.xhtml, running in the parent process"
      );
      ok(
        getFramesLabels(gToolbox).includes(tabURI),
        "The iframe dropdown lists the tab document, running in the content process"
      );

      const everythingScope = gToolbox.doc.querySelector(
        'input[name="scope"][value="everything"]'
      );
      const parentProcessScope = gToolbox.doc.querySelector(
        'input[name="scope"][value="parent-process"]'
      );
      ok(
        everythingScope.checked,
        "By default, we are in the multiprocess scope"
      );
      ok(
        !parentProcessScope.checked,
        "By default, the parent process scope is disabled"
      );

      info("Switch to parent process only scope");
      parentProcessScope.click();
      ok(
        panel.classList.contains("tooltip-visible"),
        "The menu stays visible when selecting another scope"
      );

      info("Wait for the iframe dropdown to hide the tab target");
      await waitUntil(() => {
        return !getFramesLabels(gToolbox).includes(tabURI);
      });

      info("Wait for the context selector to hide the tab context");
      await waitUntil(() => {
        return !getContextLabels(gToolbox).includes(`Test tab`);
      });

      ok(!everythingScope.checked, "Now, the multiprocess mode is disabled...");
      ok(
        parentProcessScope.checked,
        "... and the parent process scope is enabled"
      );

      info("Switch back to multiprocess scope");
      everythingScope.click();
      ok(
        panel.classList.contains("tooltip-visible"),
        "The menu stays visible when selecting another scope"
      );

      info("Wait for the iframe dropdown to show again the tab target");
      await waitUntil(() => {
        return getFramesLabels(gToolbox).includes(tabURI);
      });

      info("Wait for the context selector to show again the tab context");
      await waitUntil(() => {
        return getContextLabels(gToolbox).includes(`Test tab`);
      });
    }
  );

  await ToolboxTask.destroy();
});

function getContextLabels(toolbox) {
  // Note that the context menu is in the top level chrome document (toolbox.xhtml)
  // instead of webconsole.xhtml.
  const labels = toolbox.doc.querySelectorAll(
    "#webconsole-console-evaluation-context-selector-menu-list li .label"
  );
  return Array.from(labels).map(item => item.textContent);
}

function getFramesLabels(toolbox) {
  return Array.from(
    toolbox.doc.querySelectorAll("#toolbox-frame-menu .command .label")
  ).map(el => el.textContent);
}
