"use strict";

/**
 * Bug 1800131 - Cannot use date fields on Almosafer mobile page
 *
 * This patch ensures that the search input never has the [disabled]
 * attribute, so that users may tap/click on it to search.
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1800131 for details.
 */

const SELECTOR = `
  input[data-testid="FlightHome__DatePicker__DepartureDate"][disabled], 
  input[data-testid="FlightHome__DatePicker__ReturnDate"][disabled]
`;

function check(target) {
  if (target.nodeName === "INPUT" && target.matches(SELECTOR)) {
    target.removeAttribute("disabled");
    return true;
  }
  return false;
}

new MutationObserver(mutations => {
  for (const { addedNodes, target, attributeName } of mutations) {
    if (attributeName === "disabled") {
      check(target);
    } else {
      addedNodes?.forEach(node => {
        if (!check(node)) {
          node
            .querySelectorAll?.(SELECTOR)
            ?.forEach(n => n.removeAttribute("disabled"));
        }
      });
    }
  }
}).observe(document, { attributes: true, childList: true, subtree: true });
