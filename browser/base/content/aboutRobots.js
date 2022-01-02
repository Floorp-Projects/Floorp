/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var buttonClicked = false;
var button = document.getElementById("errorTryAgain");
button.onclick = function() {
  if (buttonClicked) {
    button.style.visibility = "hidden";
  } else {
    var newLabel = button.getAttribute("label2");
    button.textContent = newLabel;
    buttonClicked = true;
  }
};
