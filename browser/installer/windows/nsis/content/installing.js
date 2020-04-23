// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// Length of time (milliseconds) that one blurb stays up before we switch to
// displaying the next one.
var BLURB_CYCLE_MS = 20000;

// How frequently we should update the progress bar state, in milliseconds.
var PROGRESS_BAR_INTERVAL_MS = 250;

window.attachEvent("onload", function() {
  // Set direction on the two components of the layout.
  var direction = external.getTextDirection();
  document.getElementById("text_column").style.direction = direction;
  document.getElementById("installing").style.direction = direction;

  // Get this page's static strings.
  var label = document.getElementById("label");
  label.innerText = external.getUIString("installing_label");
  document.getElementById("header").innerText = external.getUIString(
    "installing_header"
  );
  document.getElementById("content").innerText = external.getUIString(
    "installing_content"
  );

  // Poll and update the progress bar percentage.
  setInterval(function() {
    var percent = external.getProgressBarPercent();
    var progressBar = document.getElementById("progress_bar");
    progressBar.setAttribute("aria-valuenow", percent);
    progressBar.style.width = percent + "%";
  }, PROGRESS_BAR_INTERVAL_MS);

  // Get the blurb strings and initialize the blurb rotation.
  var currentBlurb = 0;
  // IE8 adds undefined to the array if there is a trailing comma in an
  // array literal, so don't allow prettier to add one here.
  // prettier-ignore
  var blurbStrings = [
    external.getUIString("installing_blurb_0"),
    external.getUIString("installing_blurb_1"),
    external.getUIString("installing_blurb_2")
  ];
  function rotateBlurb() {
    document.getElementById("blurb").innerText = blurbStrings[currentBlurb];
    currentBlurb = (currentBlurb + 1) % blurbStrings.length;
  }
  rotateBlurb();
  setInterval(rotateBlurb, BLURB_CYCLE_MS);

  label.focus();
});
