/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillUtils"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.FormAutofillUtils = {
  defineLazyLogGetter(scope, logPrefix) {
    XPCOMUtils.defineLazyGetter(scope, "log", () => {
      let ConsoleAPI = Cu.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
      return new ConsoleAPI({
        maxLogLevelPref: "browser.formautofill.loglevel",
        prefix: logPrefix,
      });
    });
  },

  findLabelElements(element) {
    let document = element.ownerDocument;
    let labels = [];
    // TODO: querySelectorAll is inefficient here. However, bug 1339726 is for
    // a more efficient implementation from DOM API perspective. This function
    // should be refined after input.labels API landed.
    for (let label of document.querySelectorAll("label[for]")) {
      if (element.id == label.htmlFor) {
        labels.push(label);
      }
    }

    if (labels.length > 0) {
      log.debug("Label found by ID", element.id);
      return labels;
    }

    let parent = element.parentNode;
    if (!parent) {
      return [];
    }
    do {
      if (parent.tagName == "LABEL" &&
          parent.control == element &&
          !parent.hasAttribute("for")) {
        log.debug("Label found in input's parent or ancestor.");
        return [parent];
      }
      parent = parent.parentNode;
    } while (parent);

    return [];
  },
};

this.log = null;
this.FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

