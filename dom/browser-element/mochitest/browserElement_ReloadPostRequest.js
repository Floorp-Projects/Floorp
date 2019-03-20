/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 793644, fire an event when attempting to reloads browser element after
// POST respest.

"use strict";

/* global browserElementTestHelpers */

SimpleTest.waitForExplicitFinish();
SimpleTest.requestFlakyTimeout("untriaged");
browserElementTestHelpers.setEnabledPref(true);

var iframe;
var gotConfirmRepost = false;
var doRepost = true;
var timer;
var isPostRequestSubmitted;

function getExpectedStrings() {
  let result = {};
  let appBundle = Services.strings.createBundle("chrome://global/locale/appstrings.properties");
  let brandBundle = Services.strings.createBundle("chrome://branding/locale/brand.properties");
  try {
    let brandName = brandBundle.GetStringFromName("brandShortName");
    result.message = appBundle.formatStringFromName("confirmRepostPrompt",
                                                    [brandName], 1);
  } catch (e) {
    // for the case that we don't have brandShortName
    result.message = appBundle.GetStringFromName("confirmRepostPrompt");
  }
  result.resend = appBundle.GetStringFromName("resendButton.label");

  return result;
}

function failBecauseReloaded() {
  window.clearTimeout(timer);
  timer = null;
  iframe.removeEventListener("mozbrowserloadend", failBecauseReloaded);
  ok(false, "We don't expect browser element to reload, but it did");
  SimpleTest.finish();
}

function reloadDone() {
  iframe.removeEventListener("mozbrowserloadend", reloadDone);
  ok(gotConfirmRepost, "Didn't get confirmEx prompt before reload");

  // Run again, with repost disallowed.
  doRepost = false;
  isPostRequestSubmitted = false;
  iframe.src = "file_post_request.html";
  iframe.addEventListener("mozbrowserloadend", pageLoadDone);
}

function pageLoadDone() {
  if (!isPostRequestSubmitted) {
    // This loadend is done by setting url in address bar, so we don't need to
    // do anything. The test page will submit a POST request.
    isPostRequestSubmitted = true;
    return;
  }

  gotConfirmRepost = false;
  iframe.removeEventListener("mozbrowserloadend", pageLoadDone);
  if (doRepost) {
    iframe.addEventListener("mozbrowserloadend", reloadDone);
  } else {
    // We don't expect browserelement to reload; use a timer to make sure
    // it is not reloaded.
    iframe.addEventListener("mozbrowserloadend", failBecauseReloaded);
  }
  iframe.reload();
}

function runTest() {
  iframe = document.createElement("iframe");
  iframe.setAttribute("mozbrowser", "true");

  isPostRequestSubmitted = false;
  iframe.src = "file_post_request.html";
  document.body.appendChild(iframe);

  iframe.addEventListener("mozbrowserloadend", pageLoadDone);

  let expectedMessage = getExpectedStrings();
  iframe.addEventListener("mozbrowsershowmodalprompt", function(e) {
    if (e.detail.promptType == "custom-prompt") {
      gotConfirmRepost = true;
      e.preventDefault();
      e.detail.returnValue = {
        selectedButton: doRepost ? 0 : 1,
      };
      is(e.detail.returnValue.checked, undefined);
      is(e.detail.buttons[0].messageType, "custom");
      is(e.detail.buttons[0].message, expectedMessage.resend);
      is(e.detail.buttons[1].messageType, "builtin");
      is(e.detail.buttons[1].message, "cancel");
      is(e.detail.message, expectedMessage.message);
      is(e.detail.buttons.length, 2);
      is(e.detail.showCheckbox, false);
      is(e.detail.checkboxMessage, null);
      e.detail.unblock();

      if (!doRepost) {
        // To make sure the page doesn't reload in 1 sec.
        timer = window.setTimeout(function() {
          iframe.removeEventListener("mozbrowserloadend", failBecauseReloaded);
          SimpleTest.finish();
        }, 1000);
      }
    }
  });
}

addEventListener("testready", runTest);
