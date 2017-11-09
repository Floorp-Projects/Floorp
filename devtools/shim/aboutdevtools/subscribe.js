/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This file handles the newsletter subscription form on about:devtools.
 * It is largely inspired from https://mozilla.github.io/basket-example/
 */

window.addEventListener("load", function () {
  const { utils: Cu } = Components;
  const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});

  // Timeout for the subscribe XHR.
  const REQUEST_TIMEOUT = 5000;

  const ABOUTDEVTOOLS_STRINGS = "chrome://devtools-shim/locale/aboutdevtools.properties";
  const aboutDevtoolsBundle = Services.strings.createBundle(ABOUTDEVTOOLS_STRINGS);

  let emailInput = document.getElementById("email");
  let newsletterErrors = document.getElementById("newsletter-errors");
  let newsletterForm = document.getElementById("newsletter-form");
  let newsletterPrivacySection = document.getElementById("newsletter-privacy");
  let newsletterThanks = document.getElementById("newsletter-thanks");

  /**
   * Update the error panel to display the provided errors. If the argument is null or
   * empty, a default error message will be displayed.
   *
   * @param {Array} errors
   *        Array of strings, each item being an error message to display.
   */
  function updateErrorPanel(errors) {
    clearErrorPanel();

    if (!errors || errors.length == 0) {
      errors = [aboutDevtoolsBundle.GetStringFromName("newsletter.error.unknown")];
    }

    // Create errors markup.
    let fragment = document.createDocumentFragment();
    for (let error of errors) {
      let item = document.createElement("p");
      item.classList.add("error");
      item.appendChild(document.createTextNode(error));
      fragment.appendChild(item);
    }

    newsletterErrors.appendChild(fragment);
    newsletterErrors.classList.add("show");
  }

  /**
   * Hide the error panel and remove all errors.
   */
  function clearErrorPanel() {
    newsletterErrors.classList.remove("show");
    newsletterErrors.innerHTML = "";
  }

  // Show the additional form fields on focus of the email input.
  function onEmailInputFocus() {
    // Create a hidden measuring container, append it to the parent of the privacy section
    let container = document.createElement("div");
    container.style.cssText = "visibility: hidden; overflow: hidden; position: absolute";
    newsletterPrivacySection.parentNode.appendChild(container);

    // Clone the privacy section, append the clone to the measuring container.
    let clone = newsletterPrivacySection.cloneNode(true);
    container.appendChild(clone);

    // Measure the target height of the privacy section.
    clone.style.height = "auto";
    let height = clone.offsetHeight;

    // Cleanup the measuring container.
    container.remove();

    // Set the animate class and set the height to the measured height.
    newsletterPrivacySection.classList.add("animate");
    newsletterPrivacySection.style.cssText = `height: ${height}px; margin-bottom: 0;`;
  }

  // XHR subscribe; handle errors; display thanks message on success.
  function onFormSubmit(evt) {
    evt.preventDefault();
    evt.stopPropagation();

    // New submission, clear old errors
    clearErrorPanel();

    let xhr = new XMLHttpRequest();

    xhr.onload = function (r) {
      if (r.target.status >= 200 && r.target.status < 300) {
        let {response} = r.target;

        if (response.success === true) {
          // Hide form and show success message.
          newsletterForm.style.display = "none";
          newsletterThanks.classList.add("show");
        } else {
          // We trust the error messages from the service to be meaningful for the user.
          updateErrorPanel(response.errors);
        }
      } else {
        let {status, statusText} = r.target;
        let statusInfo = `${status} - ${statusText}`;
        let error = aboutDevtoolsBundle
          .formatStringFromName("newsletter.error.common", [statusInfo], 1);
        updateErrorPanel([error]);
      }
    };

    xhr.onerror = () => {
      updateErrorPanel();
    };

    xhr.ontimeout = () => {
      let error = aboutDevtoolsBundle.GetStringFromName("newsletter.error.timeout");
      updateErrorPanel([error]);
    };

    let url = newsletterForm.getAttribute("action");

    xhr.open("POST", url, true);
    xhr.setRequestHeader("Content-type", "application/x-www-form-urlencoded");
    xhr.setRequestHeader("X-Requested-With", "XMLHttpRequest");
    xhr.timeout = REQUEST_TIMEOUT;
    xhr.responseType = "json";

    // Create form data.
    let formData = new FormData(newsletterForm);
    formData.append("source_url", document.location.href);

    let params = new URLSearchParams(formData);

    // Send the request.
    xhr.send(params.toString());
  }

  // Attach event listeners.
  newsletterForm.addEventListener("submit", onFormSubmit);
  emailInput.addEventListener("focus", onEmailInputFocus);
}, { once: true });
