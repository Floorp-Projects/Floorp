"use strict";
// Form submissions triggered by the Javascript 'submit' event listener are
// deferred until the event listener finishes. However, changes to specific
// attributes ("action" and "target" attributes) need to cause an immediate
// flush of any pending submission to prevent the form submission from using the
// wrong action or target. This test ensures that such flushes happen properly.

const kTestPage = "http://example.org/browser/dom/html/test/submission_flush.html";
// This is the page pointed to by the form action in the test HTML page.
const kPostActionPage = "http://example.org/browser/dom/html/test/post_action_page.html";

const kFormId = "test_form"
const kFrameId = "test_frame"
const kSubmitButtonId = "submit_button"

// Take in a variety of actions (in the form of setting and unsetting form
// attributes). Then submit the form in the submit event listener to cause a
// deferred form submission. Then perform the test actions and ensure that the
// form used the correct attribute values rather than the changed ones.
async function runTest(aTestActions) {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, kTestPage);
  registerCleanupFunction(() => BrowserTestUtils.removeTab(tab));

  /* eslint-disable no-shadow */
  let frame_url = await ContentTask.spawn(gBrowser.selectedBrowser,
                          {kFormId, kFrameId, kSubmitButtonId, aTestActions},
                          async function({kFormId, kFrameId, kSubmitButtonId,
                                          aTestActions}) {
    let form = content.document.getElementById(kFormId);

    form.addEventListener("submit", (event) => {
      // Need to trigger the deferred submission by submitting in the submit
      // event handler. To prevent the form from being submitted twice, the
      // <form> tag contains the attribute |onsubmit="return false;"| to cancel
      // the original submission.
      form.submit();

      if (aTestActions.setattr) {
        for (let {attr, value} of aTestActions.setattr) {
          form.setAttribute(attr, value);
        }
      }
      if (aTestActions.unsetattr) {
        for (let attr of aTestActions.unsetattr) {
          form.removeAttribute(attr);
        }
      }
    }, {capture: true, once: true});

    // Trigger the above event listener
    content.document.getElementById(kSubmitButtonId).click();

    // Test that the form was submitted to the correct target (the frame) with
    // the correct action (kPostActionPage).
    let frame = content.document.getElementById(kFrameId);
    await new Promise(resolve => {
      frame.addEventListener("load", resolve, {once: true});
    });
    return frame.contentWindow.location.href;
  });
  /* eslint-enable no-shadow */
  is(frame_url, kPostActionPage,
     "Form should have submitted with correct target and action");
}

add_task(async function() {
  info("Changing action should flush pending submissions");
  await runTest({setattr: [{attr: "action", value: "about:blank"}]});
});

add_task(async function() {
  info("Changing target should flush pending submissions");
  await runTest({setattr: [{attr: "target", value: "_blank"}]});
});

add_task(async function() {
  info("Unsetting action should flush pending submissions");
  await runTest({unsetattr: ["action"]});
});

// On failure, this test will time out rather than failing an assert. When the
// target attribute is not set, the form will submit the active page, navigating
// it away and preventing the wait for iframe load from ever finishing.
add_task(async function() {
  info("Unsetting target should flush pending submissions");
  await runTest({unsetattr: ["target"]});
});
