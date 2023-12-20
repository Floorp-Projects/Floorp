/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const browser = window.docShell.chromeEventHandler;
const { document: gDoc, XPCOMUtils } = browser.ownerGlobal;

ChromeUtils.defineESModuleGetters(this, {
  AboutWelcomeParent: "resource:///actors/AboutWelcomeParent.sys.mjs",
});

const CONFIG = window.arguments[0];

function addStylesheet(href) {
  const link = document.head.appendChild(document.createElement("link"));
  link.rel = "stylesheet";
  link.href = href;
}

/**
 * Render content based on about:welcome multistage template.
 */
function renderMultistage(ready) {
  const AWParent = new AboutWelcomeParent();
  const receive = name => data =>
    AWParent.onContentMessage(`AWPage:${name}`, data, browser);

  // Expose top level functions expected by the bundle.
  window.AWGetFeatureConfig = () => CONFIG;
  window.AWGetSelectedTheme = receive("GET_SELECTED_THEME");
  window.AWSelectTheme = data => receive("SELECT_THEME")(data?.toUpperCase());
  // Do not send telemetry if message (e.g. spotlight in PBM) config sets metrics as 'block'.
  if (CONFIG?.metrics !== "block") {
    window.AWSendEventTelemetry = receive("TELEMETRY_EVENT");
  }
  window.AWSendToDeviceEmailsSupported = receive(
    "SEND_TO_DEVICE_EMAILS_SUPPORTED"
  );
  window.AWAddScreenImpression = receive("ADD_SCREEN_IMPRESSION");
  window.AWSendToParent = (name, data) => receive(name)(data);
  window.AWFinish = () => {
    window.close();
  };
  window.AWWaitForMigrationClose = receive("WAIT_FOR_MIGRATION_CLOSE");
  window.AWEvaluateScreenTargeting = receive("EVALUATE_SCREEN_TARGETING");

  // Update styling to be compatible with about:welcome.
  addStylesheet("chrome://browser/content/aboutwelcome/aboutwelcome.css");

  document.body.classList.add("onboardingContainer");
  document.body.id = "multi-stage-message-root";
  // This value is reported as the "page" in telemetry
  document.body.dataset.page = "spotlight";

  // Prevent applying the default modal shadow and margins because the content
  // handles styling, including its own modal shadowing.
  const box = browser.closest(".dialogBox");
  const dialog = box.closest("dialog");
  box.classList.add("spotlightBox");
  dialog?.classList.add("spotlight");
  // Prevent SubDialog methods from manually setting dialog size.
  box.setAttribute("sizeto", "available");
  addEventListener("pagehide", () => {
    box.classList.remove("spotlightBox");
    dialog?.classList.remove("spotlight");
    box.removeAttribute("sizeto");
  });

  // Load the bundle to render the content as configured.
  document.head.appendChild(document.createElement("script")).src =
    "chrome://browser/content/aboutwelcome/aboutwelcome.bundle.js";
  ready();
}

// Indicate when we're ready to show and size (async localized) content.
document.mozSubdialogReady = new Promise(resolve =>
  document.addEventListener(
    "DOMContentLoaded",
    () => renderMultistage(resolve),
    {
      once: true,
    }
  )
);
