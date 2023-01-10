/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const browser = window.docShell.chromeEventHandler;
const { document: gDoc, XPCOMUtils } = browser.ownerGlobal;

XPCOMUtils.defineLazyModuleGetters(this, {
  AboutWelcomeParent: "resource:///actors/AboutWelcomeParent.jsm",
  RemoteL10n: "resource://activity-stream/lib/RemoteL10n.jsm",
});

const [CONFIG, PARAMS] = window.arguments[0];

function cloneTemplate(id) {
  return document.getElementById(id).content.cloneNode(true);
}

function addStylesheet(href) {
  const link = document.head.appendChild(document.createElement("link"));
  link.rel = "stylesheet";
  link.href = href;
}

/**
 * Render content based on Spotlight-specific templates.
 */
async function renderSpotlight(ready) {
  const { template, logo = {}, body, extra = {} } = CONFIG;

  // Add Spotlight styles
  addStylesheet("chrome://browser/skin/spotlight.css");

  // Apply desired message template.
  const clone = cloneTemplate(template);
  document.body.classList.add(template);

  // Render logo element.
  let imageEl = clone.querySelector(".logo");
  if (logo.imageURL) {
    imageEl.src = logo.imageURL;
  } else {
    imageEl.style.visibility = "hidden";
  }
  imageEl.style.height = imageEl.style.width = logo.size;

  // Set text data of an element by class name with local/remote as configured.
  const setText = (className, config) => {
    const el = clone.querySelector(`.${className}`);
    if (!config.label) {
      el.remove();
      return;
    }

    el.appendChild(
      RemoteL10n.createElement(document, "span", { content: config.label })
    );
    el.style.fontSize = config.size;
  };

  // Render main body text elements.
  Object.entries(body).forEach(entry => setText(...entry));

  // Optionally apply and render extra behaviors.
  const { expanded } = extra;
  if (expanded) {
    // Add the expanded behavior to the main text content.
    clone
      .querySelector("#content")
      .append(cloneTemplate("extra-content-expanded"));
    setText("expanded", expanded);

    // Initialize state and handle toggle events.
    const toggleBtn = clone.querySelector("#learn-more-toggle");
    const toggle = () => {
      const toExpand = !!toggleBtn.dataset.l10nId?.includes("collapsed");
      document.l10n.setAttributes(
        toggleBtn,
        toExpand
          ? "spotlight-learn-more-expanded"
          : "spotlight-learn-more-collapsed"
      );
      toggleBtn.setAttribute("aria-expanded", toExpand);
    };
    toggleBtn.addEventListener("click", toggle);
    toggle();
  }

  document.body.appendChild(clone);

  let primaryBtn = document.getElementById("primary");
  let secondaryBtn = document.getElementById("secondary");
  if (primaryBtn) {
    primaryBtn.addEventListener("click", () => {
      PARAMS.primaryBtn = true;
      window.close();
    });

    // If we just call focus() at some random time, it'll cause a flush,
    // which slows things down unnecessarily, so instead we use rAF...
    requestAnimationFrame(() => {
      primaryBtn.focus({ focusVisible: false });
    });
  }
  if (secondaryBtn) {
    secondaryBtn.addEventListener("click", () => {
      PARAMS.secondaryBtn = true;
      window.close();
    });
  }

  // Wait for translations to load before getting sizing information.
  await document.l10n.ready;
  await document.l10n.translateElements(clone.children);
  requestAnimationFrame(() => requestAnimationFrame(ready));
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
  window.AWGetRegion = receive("GET_REGION");
  window.AWGetSelectedTheme = receive("GET_SELECTED_THEME");
  window.AWSelectTheme = data => receive("SELECT_THEME")(data?.toUpperCase());
  // Do not send telemetry if message (e.g. spotlight in PBM) config sets metrics as 'block'.
  if (CONFIG?.metrics !== "block") {
    window.AWSendEventTelemetry = receive("TELEMETRY_EVENT");
  }
  window.AWSendToDeviceEmailsSupported = receive(
    "SEND_TO_DEVICE_EMAILS_SUPPORTED"
  );
  window.AWSendToParent = (name, data) => receive(name)(data);
  window.AWFinish = () => {
    window.close();
  };
  window.AWWaitForMigrationClose = receive("WAIT_FOR_MIGRATION_CLOSE");

  // Update styling to be compatible with about:welcome.
  addStylesheet(
    "chrome://activity-stream/content/aboutwelcome/aboutwelcome.css"
  );

  document.body.classList.add("onboardingContainer");
  document.body.id = "root";

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
    "resource://activity-stream/aboutwelcome/aboutwelcome.bundle.js";
  ready();
}

// Indicate when we're ready to show and size (async localized) content.
document.mozSubdialogReady = new Promise(resolve =>
  document.addEventListener(
    "DOMContentLoaded",
    () =>
      (CONFIG.template === "multistage" ? renderMultistage : renderSpotlight)(
        resolve
      ),
    {
      once: true,
    }
  )
);
