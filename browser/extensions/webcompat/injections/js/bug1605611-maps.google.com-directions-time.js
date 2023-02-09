"use strict";

/**
 * Bug 1605611 - Cannot change Departure/arrival dates in Google Maps on Android
 *
 * This patch re-enables the disabled "Leave now" button.
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1800498 and
 *     https://bugzilla.mozilla.org/show_bug.cgi?id=1605611 for details.
 */

const selector =
  ".ml-directions-searchbox-parent [aria-haspopup=dialog][disabled=true]";

document.addEventListener("DOMContentLoaded", () => {
  // In case the element appeared before the MutationObserver was activated.
  for (const elem of document.querySelectorAll(selector)) {
    elem.disabled = false;
  }
  // Start watching for the insertion of the "Leave now" button.
  const moOptions = {
    attributeFilter: ["disabled"],
    attributes: true,
    subtree: true,
  };
  const mo = new MutationObserver(function(records) {
    for (const { target } of records) {
      if (target.matches(selector)) {
        target.disabled = false;
      }
    }
  });
  mo.observe(document.body, moOptions);
});
