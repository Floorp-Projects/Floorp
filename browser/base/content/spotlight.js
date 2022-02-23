/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {
  document: gDoc,
  ChromeUtils,
} = window.docShell.chromeEventHandler.ownerGlobal;
const { RemoteL10n } = ChromeUtils.import(
  "resource://activity-stream/lib/RemoteL10n.jsm"
);

function cloneTemplate(id) {
  return document.getElementById(id).content.cloneNode(true);
}

async function renderSpotlight(ready) {
  const [
    { template, logo = {}, body, extra = {} },
    params,
  ] = window.arguments[0];

  // Apply desired message template.
  const clone = cloneTemplate(template);
  document.body.classList.add(template);

  // Render logo element.
  let imageEl = clone.querySelector(".logo");
  // Allow backwards compatibility of previous content structure.
  imageEl.src = logo.imageURL ?? window.arguments[0][0].logoImageURL;
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
      params.primaryBtn = true;
      window.close();
    });

    // If we just call focus() at some random time, it'll cause a flush,
    // which slows things down unnecessarily, so instead we use rAF...
    requestAnimationFrame(() => {
      primaryBtn.focus({ preventFocusRing: true });
    });
  }
  if (secondaryBtn) {
    secondaryBtn.addEventListener("click", () => {
      params.secondaryBtn = true;
      window.close();
    });
  }

  // Wait for translations to load before getting sizing information.
  await document.l10n.ready;
  await document.l10n.translateElements(clone.children);
  requestAnimationFrame(() => requestAnimationFrame(ready));
}

// Indicate when we're ready to show and size (async localized) content.
document.mozSubdialogReady = new Promise(resolve =>
  document.addEventListener(
    "DOMContentLoaded",
    () => renderSpotlight(resolve),
    {
      once: true,
    }
  )
);
