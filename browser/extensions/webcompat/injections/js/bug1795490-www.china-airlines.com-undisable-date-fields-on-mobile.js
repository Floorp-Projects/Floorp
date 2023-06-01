/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1795490 - Cannot use date fields on China Airlines mobile page
 *
 * This patch ensures that the search input never has the [disabled]
 * attribute, so that users may tap/click on it to search.
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1795490 for details.
 */

const SELECTOR = `#departureDateMobile[disabled], #returnDateMobile[disabled]`;

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
