/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// change the text of the install button on addons.mozilla.org


function changeInstallButtonText() {
  let targetElem = document.querySelector(".AMInstallButton-button")
  if (targetElem) {
    targetElem.textContent = targetElem.textContent.replace("Firefox", "Floorp")
  }
}
