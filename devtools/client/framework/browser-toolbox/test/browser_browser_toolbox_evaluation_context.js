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
// - computed view is correct when selecting an element in a remote frame

add_task(async function() {
  // Forces the Browser Toolbox to open on the console by default
  await pushPref("devtools.browsertoolbox.panel", "webconsole");
  await pushPref("devtools.webconsole.input.context", true);

  // Open the test *before* opening the Browser toolbox in order to have the right target title.
  // Once created, the target won't update its title, and so would be "New Tab", instead of "Test tab"
  const tab = await addTab(
    "https://example.com/document-builder.sjs?html=<html><title>Test tab</title></html>"
  );

  const ToolboxTask = await initBrowserToolboxTask({
    enableBrowserToolboxFission: true,
  });

  const tabProcessID =
    tab.linkedBrowser.browsingContext.currentWindowGlobal.osPid;

  await ToolboxTask.spawn(
    [tabProcessID, isFissionEnabled()],
    async (processID, _isFissionEnabled) => {
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

      // Note that the context menu is in the top level chrome document (browser-toolbox.xhtml)
      // instead of webconsole.xhtml.
      const labels = hud.chromeWindow.document.querySelectorAll(
        "#webconsole-console-evaluation-context-selector-menu-list li .label"
      );
      const labelTexts = Array.from(labels).map(item => item.textContent);

      const expectedTitle = _isFissionEnabled
        ? `(pid ${processID}) https://example.com`
        : `(pid ${processID}) web`;
      is(
        labelTexts.includes(expectedTitle),
        true,
        `${processID} content process visible in the execution context (${labelTexts})`
      );

      is(
        labelTexts.includes(`Test tab`),
        true,
        `Test tab is visible in the execution context (${labelTexts})`
      );
    }
  );

  await ToolboxTask.destroy();
});
