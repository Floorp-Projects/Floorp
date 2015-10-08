"use strict";

const SERVER_URL = "http://example.com/browser/toolkit/crashreporter/test/browser/crashreport.sjs";
const PAGE = "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";
const COMMENTS = "Here's my test comment!";
const EMAIL = "foo@privacy.com";

/**
 * Sets up the browser to send crash reports to the local crash report
 * testing server.
 */
add_task(function* setup() {
  // The test harness sets MOZ_CRASHREPORTER_NO_REPORT, which disables crash
  // reports.  This test needs them enabled.  The test also needs a mock
  // report server, and fortunately one is already set up by toolkit/
  // crashreporter/test/Makefile.in.  Assign its URL to MOZ_CRASHREPORTER_URL,
  // which CrashSubmit.jsm uses as a server override.
  let env = Cc["@mozilla.org/process/environment;1"]
              .getService(Components.interfaces.nsIEnvironment);
  let noReport = env.get("MOZ_CRASHREPORTER_NO_REPORT");
  let serverUrl = env.get("MOZ_CRASHREPORTER_URL");
  env.set("MOZ_CRASHREPORTER_NO_REPORT", "");
  env.set("MOZ_CRASHREPORTER_URL", SERVER_URL);

  // On debug builds, crashing tabs results in much thinking, which
  // slows down the test and results in intermittent test timeouts,
  // so we'll pump up the expected timeout for this test.
  requestLongerTimeout(2);

  registerCleanupFunction(function() {
    env.set("MOZ_CRASHREPORTER_NO_REPORT", noReport);
    env.set("MOZ_CRASHREPORTER_URL", serverUrl);
  });
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
  }, function*(browser) {
    let prefs = TabCrashReporter.prefs;
    let originalSendReport = prefs.getBoolPref("sendReport");
    let originalEmailMe = prefs.getBoolPref("emailMe");
    let originalIncludeURL = prefs.getBoolPref("includeURL");
    let originalEmail = prefs.getCharPref("email");

    let tab = gBrowser.getTabForBrowser(browser);
    yield BrowserTestUtils.crashBrowser(browser);
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
    yield BrowserTestUtils.waitForEvent(tab, "SSTabRestored");
    yield crashReport;

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
add_task(function* test_default() {
  yield crashTabTestHelper({}, {
    "Comments": "",
    "URL": "",
    "Email": "",
  });
});

/**
 * Test just sending a comment.
 */
add_task(function* test_just_a_comment() {
  yield crashTabTestHelper({
    comments: COMMENTS,
  }, {
    "Comments": COMMENTS,
    "URL": "",
    "Email": "",
  });
});

/**
 * Test that we don't send email if emailMe is unchecked
 */
add_task(function* test_no_email() {
  yield crashTabTestHelper({
    email: EMAIL,
    emailMe: false,
  }, {
    "Comments": "",
    "URL": "",
    "Email": "",
  });
});

/**
 * Test that we can send an email address if emailMe is checked
 */
add_task(function* test_yes_email() {
  yield crashTabTestHelper({
    email: EMAIL,
    emailMe: true,
  }, {
    "Comments": "",
    "URL": "",
    "Email": EMAIL,
  });
});

/**
 * Test that we will send the URL of the page if includeURL is checked.
 */
add_task(function* test_send_URL() {
  yield crashTabTestHelper({
    includeURL: true,
  }, {
    "Comments": "",
    "URL": PAGE,
    "Email": "",
  });
});

/**
 * Test that we can send comments, the email address, and the URL
 */
add_task(function* test_send_all() {
  yield crashTabTestHelper({
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
