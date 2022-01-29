"use strict";

/* globals exportFunction */

/**
 * Bug 1605611 - Cannot change Departure/arrival dates in Google Maps on Android
 *
 * This patch re-enables the disabled "Leave now" button.
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1605611 for details.
 */

document.addEventListener("DOMContentLoaded", () => {
  // In case the element appeared before the MutationObserver was activated.
  for (const elem of document.querySelectorAll(".ml-icon-access-time")) {
    elem.parentNode.disabled = false;
  }
  // Start watching for the insertion of the "Leave now" button.
  const moOptions = {
    attributeFilter: ["disabled"],
    attributes: true,
    subtree: true,
  };
  const mo = new MutationObserver(function(records) {
    let restore = false;
    for (const { target } of records) {
      if (target.querySelector(".ml-icon-access-time")) {
        if (!restore) {
          restore = true;
          mo.disconnect();
        }

        target.disabled = false;
      }
    }
    if (restore) {
      mo.observe(document.body, moOptions);
    }
  });
  mo.observe(document.body, moOptions);
});
