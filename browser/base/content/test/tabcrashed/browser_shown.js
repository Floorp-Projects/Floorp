"use strict";

const PAGE = "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";
const COMMENTS = "Here's my test comment!";
const EMAIL = "foo@privacy.com";

add_task(async function setup() {
  await setupLocalCrashReportServer();
});

/**
 * This function returns a Promise that resolves once the following
 * actions have taken place:
 *
 * 1) A new tab is opened up at PAGE
 * 2) The tab is crashed
 * 3) The about:tabcrashed page's fields are set in accordance with
 *    fieldValues
 * 4) The tab is restored
 * 5) A crash report is received from the testing server
 * 6) Any tab crash prefs that were overwritten are reset
 *
 * @param fieldValues
 *        An Object describing how to set the about:tabcrashed
 *        fields. The following properties are accepted:
 *
 *        comments (String)
 *          The comments to put in the comment textarea
 *        email (String)
 *          The email address to put in the email address input
 *        emailMe (bool)
 *          The checked value of the "Email me" checkbox
 *        includeURL (bool)
 *          The checked value of the "Include URL" checkbox
 *
 *        If any of these fields are missing, the defaults from
 *        the user preferences are used.
 * @param expectedExtra
 *        An Object describing the expected values that the submitted
 *        crash report's extra data should contain.
 * @returns Promise
 */
function crashTabTestHelper(fieldValues, expectedExtra) {
  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, async function(browser) {
    let prefs = TabCrashHandler.prefs;
    let originalSendReport = prefs.getBoolPref("sendReport");
    let originalEmailMe = prefs.getBoolPref("emailMe");
    let originalIncludeURL = prefs.getBoolPref("includeURL");
    let originalEmail = prefs.getCharPref("email");

    let tab = gBrowser.getTabForBrowser(browser);
    await BrowserTestUtils.crashBrowser(browser);
    let doc = browser.contentDocument;

    // Since about:tabcrashed will run in the parent process, we can safely
    // manipulate its DOM nodes directly
    let comments = doc.getElementById("comments");
    let email = doc.getElementById("email");
    let emailMe = doc.getElementById("emailMe");
    let includeURL = doc.getElementById("includeURL");

    if (fieldValues.hasOwnProperty("comments")) {
      comments.value = fieldValues.comments;
    }

    if (fieldValues.hasOwnProperty("email")) {
      email.value = fieldValues.email;
    }

    if (fieldValues.hasOwnProperty("emailMe")) {
      emailMe.checked = fieldValues.emailMe;
    }

    if (fieldValues.hasOwnProperty("includeURL")) {
      includeURL.checked = fieldValues.includeURL;
    }

    let crashReport = promiseCrashReport(expectedExtra);
    let restoreTab = browser.contentDocument.getElementById("restoreTab");
    restoreTab.click();
    await BrowserTestUtils.waitForEvent(tab, "SSTabRestored");
    await crashReport;

    // Submitting the crash report may have set some prefs regarding how to
    // send tab crash reports. Let's reset them for the next test.
    prefs.setBoolPref("sendReport", originalSendReport);
    prefs.setBoolPref("emailMe", originalEmailMe);
    prefs.setBoolPref("includeURL", originalIncludeURL);
    prefs.setCharPref("email", originalEmail);
  });
}

/**
 * Tests what we send with the crash report by default. By default, we do not
 * send any comments, the URL of the crashing page, or the email address of
 * the user.
 */
add_task(async function test_default() {
  await crashTabTestHelper({}, {
    "Comments": null,
    "URL": "",
    "Email": null,
  });
});

/**
 * Test just sending a comment.
 */
add_task(async function test_just_a_comment() {
  await crashTabTestHelper({
    comments: COMMENTS,
  }, {
    "Comments": COMMENTS,
    "URL": "",
    "Email": null,
  });
});

/**
 * Test that we don't send email if emailMe is unchecked
 */
add_task(async function test_no_email() {
  await crashTabTestHelper({
    email: EMAIL,
    emailMe: false,
  }, {
    "Comments": null,
    "URL": "",
    "Email": null,
  });
});

/**
 * Test that we can send an email address if emailMe is checked
 */
add_task(async function test_yes_email() {
  await crashTabTestHelper({
    email: EMAIL,
    emailMe: true,
  }, {
    "Comments": null,
    "URL": "",
    "Email": EMAIL,
  });
});

/**
 * Test that we will send the URL of the page if includeURL is checked.
 */
add_task(async function test_send_URL() {
  await crashTabTestHelper({
    includeURL: true,
  }, {
    "Comments": null,
    "URL": PAGE,
    "Email": null,
  });
});

/**
 * Test that we can send comments, the email address, and the URL
 */
add_task(async function test_send_all() {
  await crashTabTestHelper({
    includeURL: true,
    emailMe: true,
    email: EMAIL,
    comments: COMMENTS,
  }, {
    "Comments": COMMENTS,
    "URL": PAGE,
    "Email": EMAIL,
  });
});

