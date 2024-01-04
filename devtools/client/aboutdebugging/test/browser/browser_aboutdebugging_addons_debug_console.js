/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/* import-globals-from helper-addons.js */
Services.scriptloader.loadSubScript(CHROME_URL_ROOT + "helper-addons.js", this);

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

// Avoid test timeouts that can occur while waiting for the "addon-console-works" message.
requestLongerTimeout(2);

const ADDON_ID = "test-devtools-webextension@mozilla.org";
const ADDON_NAME = "base-test-devtools-webextension";

const OTHER_ADDON_ID = "other-test-devtools-webextension@mozilla.org";
const OTHER_ADDON_NAME = "other-test-devtools-webextension";

const POPUPONLY_ADDON_ID = "popuponly-test-devtools-webextension@mozilla.org";
const POPUPONLY_ADDON_NAME = "popuponly-test-devtools-webextension";

const BACKGROUND_ADDON_ID = "background-test-devtools-webextension@mozilla.org";
const BACKGROUND_ADDON_NAME = "background-test-devtools-webextension";

/**
 * This test file ensures that the webextension addon developer toolbox:
 * - when the debug button is clicked on a webextension, the opened toolbox
 *   has a working webconsole with the background page as default target;
 */
add_task(async function testWebExtensionsToolboxWebConsole() {
  await pushPref("devtools.webconsole.filter.css", true);
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI(
    {
      background() {
        window.myWebExtensionAddonFunction = function () {
          console.log(
            "Background page function called",
            this.browser.runtime.getManifest()
          );
        };

        const style = document.createElement("style");
        style.textContent = "* { color: error; }";
        document.documentElement.appendChild(style);

        throw new Error("Background page exception");
      },
      extraProperties: {
        browser_action: {
          default_title: "WebExtension Popup Debugging",
          default_popup: "popup.html",
          default_area: "navbar",
        },
      },
      files: {
        "popup.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="popup.js"></script>
          </head>
          <body>
            Popup
          </body>
        </html>
      `,
        "popup.js": function () {
          console.log("Popup log");

          const style = document.createElement("style");
          style.textContent = "* { color: popup-error; }";
          document.documentElement.appendChild(style);

          throw new Error("Popup exception");
        },
      },
      id: ADDON_ID,
      name: ADDON_NAME,
    },
    document
  );

  // Install another addon in order to ensure we don't get its logs
  await installTemporaryExtensionFromXPI(
    {
      background() {
        console.log("Other addon log");

        const style = document.createElement("style");
        style.textContent = "* { background-color: error; }";
        document.documentElement.appendChild(style);

        throw new Error("Other addon exception");
      },
      extraProperties: {
        browser_action: {
          default_title: "Other addon popup",
          default_popup: "other-popup.html",
          default_area: "navbar",
        },
      },
      files: {
        "other-popup.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="other-popup.js"></script>
          </head>
          <body>
            Other popup
          </body>
        </html>
      `,
        "other-popup.js": function () {
          console.log("Other popup log");

          const style = document.createElement("style");
          style.textContent = "* { background-color: popup-error; }";
          document.documentElement.appendChild(style);

          throw new Error("Other popup exception");
        },
      },
      id: OTHER_ADDON_ID,
      name: OTHER_ADDON_NAME,
    },
    document
  );

  const { devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    ADDON_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);
  const webconsole = await toolbox.selectTool("webconsole");
  const { hud } = webconsole;

  info("Trigger some code in the background page logging some stuff");
  const onMessage = waitUntil(() => {
    return !!findMessagesByType(hud, "Background page exception", ".error")
      .length;
  });
  hud.ui.wrapper.dispatchEvaluateExpression("myWebExtensionAddonFunction()");
  await onMessage;

  info("Open the two add-ons popups to cover popups messages");
  const onPopupMessage = waitUntil(() => {
    return !!findMessagesByType(hud, "Popup exception", ".error").length;
  });
  clickOnAddonWidget(OTHER_ADDON_ID);
  clickOnAddonWidget(ADDON_ID);
  await onPopupMessage;

  info("Wait a bit to catch unexpected duplicates or mixed up messages");
  await wait(1000);

  is(
    findMessagesByType(hud, "Background page exception", ".error").length,
    1,
    "We get the background page exception"
  );
  is(
    findMessagesByType(hud, "Popup exception", ".error").length,
    1,
    "We get the popup exception"
  );
  is(
    findMessagesByType(
      hud,
      "Expected color but found ‘error’.  Error in parsing value for ‘color’.  Declaration dropped.",
      ".warn"
    ).length,
    1,
    "We get the addon's background page CSS error message"
  );
  is(
    findMessagesByType(
      hud,
      "Expected color but found ‘popup-error’.  Error in parsing value for ‘color’.  Declaration dropped.",
      ".warn"
    ).length,
    1,
    "We get the addon's popup CSS error message"
  );

  // Verify that we don't get the other addon log and errors
  ok(
    !findMessageByType(hud, "Other addon log", ".console-api"),
    "We don't get the other addon log"
  );
  ok(
    !findMessageByType(hud, "Other addon exception", ".console-api"),
    "We don't get the other addon exception"
  );
  ok(
    !findMessageByType(hud, "Other popup log", ".console-api"),
    "We don't get the other addon popup log"
  );
  ok(
    !findMessageByType(hud, "Other popup exception", ".error"),
    "We don't get the other addon popup exception"
  );
  ok(
    !findMessageByType(
      hud,
      "Expected color but found ‘error’.  Error in parsing value for ‘background-color’.  Declaration dropped.",
      ".warn"
    ),
    "We don't get the other addon's background page CSS error message"
  );
  ok(
    !findMessageByType(
      hud,
      "Expected color but found ‘popup-error’.  Error in parsing value for ‘background-color’.  Declaration dropped.",
      ".warn"
    ),
    "We don't get the other addon's popup CSS error message"
  );

  // Verify that console evaluations still work after reloading the page
  info("Reload the webextension document");
  const { onDomCompleteResource } =
    await waitForNextTopLevelDomCompleteResource(toolbox.commands);
  hud.ui.wrapper.dispatchEvaluateExpression("location.reload()");
  await onDomCompleteResource;

  info("Try to evaluate something after reload");

  const onEvaluationResultAfterReload = waitUntil(() =>
    findMessageByType(hud, "result:2", ".result")
  );
  const onMessageAfterReload = waitUntil(() =>
    findMessageByType(hud, "message after reload", ".console-api")
  );
  hud.ui.wrapper.dispatchEvaluateExpression(
    "console.log('message after reload'); 'result:' + (1 + 1)"
  );
  // Both cover that the console.log worked
  await onMessageAfterReload;
  // And we received the evaluation result
  await onEvaluationResultAfterReload;

  await closeWebExtAboutDevtoolsToolbox(devtoolsWindow, window);

  // Note that it seems to be important to remove the addons in the reverse order
  // from which they were installed...
  await removeTemporaryExtension(OTHER_ADDON_NAME, document);
  await removeTemporaryExtension(ADDON_NAME, document);
  await removeTab(tab);
});

add_task(async function testWebExtensionNoBgScript() {
  await pushPref("devtools.webconsole.filter.css", true);
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI(
    {
      extraProperties: {
        browser_action: {
          default_title: "WebExtension Popup Only",
          default_popup: "popup.html",
          default_area: "navbar",
        },
      },
      files: {
        "popup.html": `<!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
            <script src="popup.js"></script>
          </head>
          <body>
            Popup
          </body>
        </html>
      `,
        "popup.js": function () {
          console.log("Popup-only log");

          const style = document.createElement("style");
          style.textContent = "* { color: popup-only-error; }";
          document.documentElement.appendChild(style);

          throw new Error("Popup-only exception");
        },
      },
      id: POPUPONLY_ADDON_ID,
      name: POPUPONLY_ADDON_NAME,
    },
    document
  );

  const { devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    POPUPONLY_ADDON_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);
  const webconsole = await toolbox.selectTool("webconsole");
  const { hud } = webconsole;

  info("Open the add-on popup");
  const onPopupMessage = waitUntil(() => {
    return !!findMessagesByType(hud, "Popup-only exception", ".error").length;
  });
  clickOnAddonWidget(POPUPONLY_ADDON_ID);
  await onPopupMessage;

  info("Wait a bit to catch unexpected duplicates or mixed up messages");
  await wait(1000);
  is(
    findMessagesByType(hud, "Popup-only exception", ".error").length,
    1,
    "We get the popup exception"
  );
  is(
    findMessagesByType(hud, "Popup-only log", ".console-api").length,
    1,
    "We get the addon's popup log"
  );
  is(
    findMessagesByType(
      hud,
      "Expected color but found ‘popup-only-error’.  Error in parsing value for ‘color’.  Declaration dropped.",
      ".warn"
    ).length,
    1,
    "We get the addon's popup CSS error message"
  );

  await closeWebExtAboutDevtoolsToolbox(devtoolsWindow, window);
  await removeTemporaryExtension(POPUPONLY_ADDON_NAME, document);
  await removeTab(tab);
});

// Check that reloading the addon several times does not break the console,
// see Bug 1778951.
add_task(async function testWebExtensionTwoReloads() {
  await enableExtensionDebugging();
  const { document, tab, window } = await openAboutDebugging();
  await selectThisFirefoxPage(document, window.AboutDebugging.store);

  await installTemporaryExtensionFromXPI(
    {
      background() {
        console.log("Background page log");
      },
      extraProperties: {
        browser_action: {
          default_title: "WebExtension with background script",
          default_popup: "popup.html",
          default_area: "navbar",
        },
      },
      files: {
        "popup.html": `<!DOCTYPE html>
        <html>
          <body>
            Popup
          </body>
        </html>
      `,
      },
      id: BACKGROUND_ADDON_ID,
      name: BACKGROUND_ADDON_NAME,
    },
    document
  );

  // Retrieve the addonTarget element before calling `openAboutDevtoolsToolbox`,
  // otherwise it will pick the about:devtools-toolbox tab with the same name
  // instead.
  const addonTarget = findDebugTargetByText(BACKGROUND_ADDON_NAME, document);

  const { devtoolsWindow } = await openAboutDevtoolsToolbox(
    document,
    tab,
    window,
    BACKGROUND_ADDON_NAME
  );
  const toolbox = getToolbox(devtoolsWindow);
  const webconsole = await toolbox.selectTool("webconsole");
  const { hud } = webconsole;

  // Verify that console evaluations still work after reloading the addon
  info("Reload the webextension itself");
  let { onDomCompleteResource } = await waitForNextTopLevelDomCompleteResource(
    toolbox.commands
  );
  const reloadButton = addonTarget.querySelector(
    ".qa-temporary-extension-reload-button"
  );
  reloadButton.click();
  await onDomCompleteResource;

  info("Try to evaluate something after 1st addon reload");
  // Wait before evaluating the message, otherwise they might be cleaned up by
  // the console UI.
  info("Wait until the background script log is visible");
  await waitUntil(() =>
    findMessageByType(hud, "Background page log", ".message")
  );

  hud.ui.wrapper.dispatchEvaluateExpression("40+1");
  await waitUntil(() => findMessageByType(hud, "41", ".result"));

  info("Reload the extension a second time");
  ({ onDomCompleteResource } = await waitForNextTopLevelDomCompleteResource(
    toolbox.commands
  ));
  reloadButton.click();
  await onDomCompleteResource;

  info("Wait until the background script log is visible - after reload");
  await waitUntil(() =>
    findMessageByType(hud, "Background page log", ".message")
  );

  info("Try to evaluate something after 2nd addon reload");
  hud.ui.wrapper.dispatchEvaluateExpression("40+2");
  await waitUntil(() => findMessageByType(hud, "42", ".result"));

  await closeWebExtAboutDevtoolsToolbox(devtoolsWindow, window);
  await removeTemporaryExtension(BACKGROUND_ADDON_NAME, document);
  await removeTab(tab);
});
