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

const OTHER_ADDON_ID = "other-test-devtools-webextension@mozilla.org";
const OTHER_ADDON_NAME = "other-test-devtools-webextension";

const POPUPONLY_ADDON_ID = "popuponly-test-devtools-webextension@mozilla.org";
const POPUPONLY_ADDON_NAME = "popuponly-test-devtools-webextension";

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
      background: function() {
        window.myWebExtensionAddonFunction = function() {
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
        "popup.js": function() {
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
      background: function() {
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
        "other-popup.js": function() {
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

  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
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
    return (
      findMessagesByType(hud, "Background page exception", ".error").length > 0
    );
  });
  hud.ui.wrapper.dispatchEvaluateExpression("myWebExtensionAddonFunction()");
  await onMessage;

  info("Open the two add-ons popups to cover popups messages");
  const onPopupMessage = waitUntil(() => {
    return findMessagesByType(hud, "Popup exception", ".error").length > 0;
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
  is(
    findMessagesByType(hud, "Other addon log", ".console-api").length,
    0,
    "We don't get the other addon log"
  );
  is(
    findMessagesByType(hud, "Other addon exception", ".console-api").length,
    0,
    "We don't get the other addon exception"
  );
  is(
    findMessagesByType(hud, "Other popup log", ".console-api").length,
    0,
    "We don't get the other addon popup log"
  );
  is(
    findMessagesByType(hud, "Other popup exception", ".error").length,
    0,
    "We don't get the other addon popup exception"
  );
  is(
    findMessagesByType(
      hud,
      "Expected color but found ‘error’.  Error in parsing value for ‘background-color’.  Declaration dropped.",
      ".warn"
    ).length,
    0,
    "We don't get the other addon's background page CSS error message"
  );
  is(
    findMessagesByType(
      hud,
      "Expected color but found ‘popup-error’.  Error in parsing value for ‘background-color’.  Declaration dropped.",
      ".warn"
    ).length,
    0,
    "We don't get the other addon's popup CSS error message"
  );

  // Verify that console evaluations still work after reloading the page
  info("Reload the webextension document");
  const {
    onDomCompleteResource,
  } = await waitForNextTopLevelDomCompleteResource(toolbox.commands);
  hud.ui.wrapper.dispatchEvaluateExpression("location.reload()");
  await onDomCompleteResource;

  info("Try to evaluate something after reload");
  const onEvaluationResultAfterReload = waitUntil(() => {
    return findMessagesByType(hud, "result:2", ".result").length > 0;
  });
  const onMessageAfterReload = waitUntil(() => {
    return (
      findMessagesByType(hud, "message after reload", ".console-api").length > 0
    );
  });
  hud.ui.wrapper.dispatchEvaluateExpression(
    "console.log('message after reload'); 'result:' + (1 + 1)"
  );
  // Both cover that the console.log worked
  await onMessageAfterReload;
  // And we received the evaluation result
  await onEvaluationResultAfterReload;

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);

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
        "popup.js": function() {
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

  const { devtoolsTab, devtoolsWindow } = await openAboutDevtoolsToolbox(
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
    return findMessagesByType(hud, "Popup-only exception", ".error").length > 0;
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

  await closeAboutDevtoolsToolbox(document, devtoolsTab, window);
  await removeTemporaryExtension(POPUPONLY_ADDON_NAME, document);
  await removeTab(tab);
});
