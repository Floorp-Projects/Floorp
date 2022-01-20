/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

// Avoid test timeouts that can occur while waiting for the "addon-console-works" message.
requestLongerTimeout(2);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "test-devtools-webextension";

/**
 * This test file ensures that the webextension addon developer toolbox:
 * - when the debug button is clicked on a webextension, the opened toolbox
 *   has a working webconsole with the background page as default target;
 */
add_task(async function testWebExtensionsToolboxWebConsole() {
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI(
    {
      background: function() {
        window.myWebExtensionAddonFunction = function() {
          console.log(
            "Background page function called",
            this.browser.runtime.getManifest()
          );
        };
      },
      id: ADDON_ID,
      name: ADDON_NAME,
    },
    document
  );

  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    ADDON_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);

  const webconsole = await toolbox.selectTool("webconsole");
  const { hud } = webconsole;
  const onMessage = waitUntil(() => {
    return findMessages(hud, "Background page function called").length > 0;
  });
  hud.ui.wrapper.dispatchEvaluateExpression("myWebExtensionAddonFunction()");
  await onMessage;

  // Verify that console evaluations still work after reloading the page
  info("Reload the webextension document");
  const {
    onDomCompleteResource,
  } = await waitForNextTopLevelDomCompleteResource(toolbox.commands);
  hud.ui.wrapper.dispatchEvaluateExpression("location.reload()");
  await onDomCompleteResource;

  info("Try to evaluate something after reload");
  const onEvaluationResultAfterReload = waitUntil(() => {
    return findMessages(hud, "result:2").length > 0;
  });
  const onMessageAfterReload = waitUntil(() => {
    return findMessages(hud, "message after reload", ".console-api").length > 0;
  });
  hud.ui.wrapper.dispatchEvaluateExpression(
    "console.log('message after reload'); 'result:' + (1 + 1)"
  );
  // Both cover that the console.log worked
  await onMessageAfterReload;
  // And we received the evaluation result
  await onEvaluationResultAfterReload;

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTemporaryExtension(ADDON_NAME, document);
  await removeTab(tab);
});

function findMessages(hud, text, selector = ".message") {
  const messages = hud.ui.outputNode.querySelectorAll(selector);
  const elements = Array.prototype.filter.call(messages, el =>
    el.textContent.includes(text)
  );
  return elements;
}
