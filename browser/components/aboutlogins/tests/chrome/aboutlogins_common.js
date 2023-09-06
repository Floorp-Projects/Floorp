"use strict";

/* exported asyncElementRendered, importDependencies */

/**
 * A helper to await on while waiting for an asynchronous rendering of a Custom
 * Element.
 * @returns {Promise}
 */
function asyncElementRendered() {
  return Promise.resolve();
}

/**
 * Import the templates from the real page to avoid duplication in the tests.
 * @param {HTMLIFrameElement} templateFrame - Frame to copy the resources from
 * @param {HTMLElement} destinationEl - Where to append the copied resources
 */
function importDependencies(templateFrame, destinationEl) {
  let promises = [];
  for (let template of templateFrame.contentDocument.querySelectorAll(
    "template"
  )) {
    let imported = document.importNode(template, true);
    destinationEl.appendChild(imported);
    // Preload the styles in the actual page, to ensure they're loaded on time.
    for (let element of imported.content.querySelectorAll(
      "link[rel='stylesheet']"
    )) {
      let clone = element.cloneNode(true);
      promises.push(
        new Promise(resolve => {
          clone.onload = function () {
            resolve();
            clone.remove();
          };
        })
      );
      destinationEl.appendChild(clone);
    }
  }
  return Promise.all(promises);
}

Object.defineProperty(document, "l10n", {
  configurable: true,
  writable: true,
  value: {
    connectRoot() {},
    disconnectRoot() {},
    translateElements() {
      return Promise.resolve();
    },
    getAttributes(element) {
      return {
        id: element.getAttribute("data-l10n-id"),
        args: element.getAttribute("data-l10n-args")
          ? JSON.parse(element.getAttribute("data-l10n-args"))
          : {},
      };
    },
    setAttributes(element, id, args) {
      element.setAttribute("data-l10n-id", id);
      if (args) {
        element.setAttribute("data-l10n-args", JSON.stringify(args));
      } else {
        element.removeAttribute("data-l10n-args");
      }
    },
  },
});

Object.defineProperty(window, "AboutLoginsUtils", {
  configurable: true,
  writable: true,
  value: {
    getLoginOrigin(uriString) {
      return uriString;
    },
    setFocus(element) {
      return element.focus();
    },
    async promptForPrimaryPassword(resolve, messageId) {
      resolve(true);
    },
    doLoginsMatch(login1, login2) {
      return (
        login1.origin == login2.origin &&
        login1.username == login2.username &&
        login1.password == login2.password
      );
    },
    fileImportEnabled: SpecialPowers.getBoolPref(
      "signon.management.page.fileImport.enabled"
    ),
    primaryPasswordEnabled: false,
  },
});
