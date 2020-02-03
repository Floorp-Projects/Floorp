"use strict";

/* globals exportFunction */

/**
 * Bug 1605611 - Cannot change Departure/arrival dates in Google Maps on Android
 *
 * This patch does the following:
 * 1. Re-enable the disabled "Leave now" button.
 * 2. Fix the precision of datetime-local inputs (to minutes).
 * 3. Fixup side effect from enabling the date picker UI via
 *    injections/css/bug1605611-maps.google.com-directions-time.css
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1605611#c0 for details.
 */

// Step 1.
document.addEventListener("DOMContentLoaded", () => {
  // In case the element appeared before the MutationObserver was activated.
  for (const elem of document.querySelectorAll(
    ".ml-directions-time[disabled]"
  )) {
    elem.disabled = false;
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
      if (target.classList.contains("ml-directions-time")) {
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

// Step 2.
const originalValueAsNumberGetter = Object.getOwnPropertyDescriptor(
  HTMLInputElement.prototype.wrappedJSObject,
  "valueAsNumber"
).get;
Object.defineProperty(
  HTMLInputElement.prototype.wrappedJSObject,
  "valueAsNumber",
  {
    configurable: true,
    enumerable: true,
    get: originalValueAsNumberGetter,
    set: exportFunction(function(v) {
      if (this.type === "datetime-local" && v) {
        const d = new Date(v);
        d.setSeconds(0);
        d.setMilliseconds(0);
        v = d.getTime();
      }
      this.valueAsNumber = v;
    }, window),
  }
);

// Step 3.
// injections/css/bug1605611-maps.google.com-directions-time.css fixes the bug,
// but a side effect of allowing the user to click on the datetime-local input
// is that the keyboard appears when the native date picker is closed.
// Fix this by unfocusing the datetime-local input upon focus.
document.addEventListener("focusin", ({ target }) => {
  if (target.id === "ml-route-options-time-selector-time-input") {
    target.blur();
  }
});
