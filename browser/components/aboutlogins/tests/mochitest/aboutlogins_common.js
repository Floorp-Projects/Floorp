"use strict";

/* exported asyncElementRendered, importDependencies, stubFluentL10n */

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
  let templates = templateFrame.contentDocument.querySelectorAll("template");
  isnot(templates, null, "Check some templates found");
  for (let template of templates) {
    let imported = document.importNode(template, true);
    destinationEl.appendChild(imported);
  }
}

function stubFluentL10n(argsMap) {
  document.l10n = {
    setAttributes(element, id, args) {
      element.setAttribute("data-l10n-id", id);
      for (let attrName of Object.keys(argsMap)) {
        let varName = argsMap[attrName];
        element.setAttribute(attrName, args[varName]);
      }
    },
  };
}
