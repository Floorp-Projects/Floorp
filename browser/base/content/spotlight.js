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

function renderSpotlight() {
  const [content, params] = window.arguments[0];
  const template = document.querySelector(`#${content?.template}`);
  const clone = template.content.cloneNode(true);

  document.body.classList.add(content.template);

  let imageEl = clone.querySelector(".logo");
  imageEl.src = content.logoImageURL;

  for (let textProp in content.body) {
    let el = clone.querySelector(`.${textProp}`);
    if (!content.body[textProp]?.label) {
      el.remove();
      continue;
    }
    el.appendChild(
      RemoteL10n.createElement(this.window.document, "span", {
        content: content.body[textProp].label,
      })
    );
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
}

renderSpotlight();
