/*
 * Test Content-Signature for remote about:newtab
 *  - Bug 1226928 - allow about:newtab to load remote content
 *
 * This tests content-signature verification on remote about:newtab in the
 * following cases (see TESTS, all failed loads display about:blank fallback):
 * - good case (signature should verify and correct page is displayed)
 * - reload of newtab when the siganture was invalidated after the last correct
 *   load
 * - malformed content-signature header
 * - malformed keyid directive
 * - malformed p384ecdsa directive
 * - wrong signature (this is not a siganture for the delivered document)
 * - invalid signature (this is not even a signature)
 * - loading a file that doesn't fit the key or signature
 * - cache poisoning (load a malicious remote page not in newtab, subsequent
 *   newtab load has to load the fallback)
 */

const ABOUT_NEWTAB_URI = "about:newtab";

const BASE = "https://example.com/browser/dom/security/test/contentverifier/file_contentserver.sjs?";
const URI_GOOD = BASE + "sig=good&key=good&file=good&header=good";

const INVALIDATE_FILE = BASE + "invalidateFile=yep";
const VALIDATE_FILE = BASE + "validateFile=yep";

const URI_HEADER_BASE = BASE + "sig=good&key=good&file=good&header=";
const URI_ERROR_HEADER = URI_HEADER_BASE + "error";
const URI_KEYERROR_HEADER = URI_HEADER_BASE + "errorInKeyid";
const URI_SIGERROR_HEADER = URI_HEADER_BASE + "errorInSignature";
const URI_NO_HEADER = URI_HEADER_BASE + "noHeader";

const URI_BAD_SIG = BASE + "sig=bad&key=good&file=good&header=good";
const URI_BROKEN_SIG = BASE + "sig=broken&key=good&file=good&header=good";
const URI_BAD_KEY = BASE + "sig=good&key=bad&file=good&header=good";
const URI_BAD_FILE = BASE + "sig=good&key=good&file=bad&header=good";
const URI_BAD_ALL = BASE + "sig=bad&key=bad&file=bad&header=bad";
const URI_BAD_CSP = BASE + "sig=bad-csp&key=good&file=bad-csp&header=good";

const URI_BAD_FILE_CACHED = BASE + "sig=good&key=good&file=bad&header=good&cached=true";

const GOOD_ABOUT_STRING = "Just a fully good testpage for Bug 1226928";
const BAD_ABOUT_STRING = "Just a bad testpage for Bug 1226928";
const ABOUT_BLANK = "<head></head><body></body>";

const URI_CLEANUP = BASE + "cleanup=true";
const CLEANUP_DONE = "Done";

const URI_SRI = BASE + "sig=sri&key=good&file=sri&header=good";
const STYLESHEET_WITHOUT_SRI_BLOCKED = "Stylesheet without SRI blocked";
const STYLESHEET_WITH_SRI_BLOCKED = "Stylesheet with SRI blocked";
const STYLESHEET_WITH_SRI_LOADED = "Stylesheet with SRI loaded";
const SCRIPT_WITHOUT_SRI_BLOCKED = "Script without SRI blocked";
const SCRIPT_WITH_SRI_BLOCKED = "Script with SRI blocked";
const SCRIPT_WITH_SRI_LOADED = "Script with SRI loaded";

const CSP_TEST_SUCCESS_STRING = "CSP violation test succeeded.";

// Needs to sync with pref "security.signed_content.CSP.default".
const SIGNED_CONTENT_CSP = `{"csp-policies":[{"report-only":false,"script-src":["https://example.com","'unsafe-inline'"],"style-src":["https://example.com"]}]}`;

const TESTS = [
  // { newtab (aboutURI) or regular load (url) : url,
  //   testStrings : expected strings in the loaded page }
  { "aboutURI" : URI_GOOD, "testStrings" : [GOOD_ABOUT_STRING] },
  { "aboutURI" : URI_ERROR_HEADER, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_KEYERROR_HEADER, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_SIGERROR_HEADER, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_NO_HEADER, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_BAD_SIG, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_BROKEN_SIG, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_BAD_KEY, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_BAD_FILE, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_BAD_ALL, "testStrings" : [ABOUT_BLANK] },
  { "url" : URI_BAD_FILE_CACHED, "testStrings" : [BAD_ABOUT_STRING] },
  { "aboutURI" : URI_BAD_FILE_CACHED, "testStrings" : [ABOUT_BLANK] },
  { "aboutURI" : URI_GOOD, "testStrings" : [GOOD_ABOUT_STRING] },
  { "aboutURI" : URI_SRI, "testStrings" : [
    STYLESHEET_WITHOUT_SRI_BLOCKED,
    STYLESHEET_WITH_SRI_LOADED,
    SCRIPT_WITHOUT_SRI_BLOCKED,
    SCRIPT_WITH_SRI_LOADED,
    ]},
  { "aboutURI" : URI_BAD_CSP, "testStrings" : [CSP_TEST_SUCCESS_STRING] },
  { "url" : URI_CLEANUP, "testStrings" : [CLEANUP_DONE] },
];

var browser = null;
var aboutNewTabService = Cc["@mozilla.org/browser/aboutnewtab-service;1"]
                           .getService(Ci.nsIAboutNewTabService);

function pushPrefs(...aPrefs) {
  return new Promise((resolve) => {
    SpecialPowers.pushPrefEnv({"set": aPrefs}, resolve);
  });
}

/*
 * run tests with input from TESTS
 */
function doTest(aExpectedStrings, reload, aUrl, aNewTabPref) {
  // set about:newtab location for this test if it's a newtab test
  if (aNewTabPref) {
    aboutNewTabService.newTabURL = aNewTabPref;
  }

  // set prefs
  yield pushPrefs(
      ["browser.newtabpage.remote.content-signing-test", true],
      ["browser.newtabpage.remote", true], [
        "browser.newtabpage.remote.keys",
        "RemoteNewTabNightlyv0=BO9QHuP6E2eLKybql8iuD4o4Np9YFDfW3D+k" +
        "a70EcXXTqZcikc7Am1CwyP1xBDTpEoe6gb9SWzJmaDW3dNh1av2u90VkUM" +
        "B7aHIrImjTjLNg/1oC8GRcTKM4+WzbKF00iA==;OtherKey=eKQJ2fNSId" +
        "CFzL6N326EzZ/5LCeFU5eyq3enwZ5MLmvOw+3gycr4ZVRc36/EiSPsQYHE" +
        "3JvJs1EKs0QCaguHFOZsHwqXMPicwp/gLdeYbuOmN2s1SEf/cxw8GtcxSA" +
        "kG;RemoteNewTab=MHYwEAYHKoZIzj0CAQYFK4EEACIDYgAE4k3FmG7dFo" +
        "Ot3Tuzl76abTRtK8sb/r/ibCSeVKa96RbrOX2ciscz/TT8wfqBYS/8cN4z" +
        "Me1+f7wRmkNrCUojZR1ZKmYM2BeiUOMlMoqk2O7+uwsn1DwNQSYP58TkvZt6"
      ]);

  if (aNewTabPref === URI_BAD_CSP) {
    // Use stricter CSP to test CSP violation.
    yield pushPrefs(["security.signed_content.CSP.default", "script-src 'self'; style-src 'self'"]);
  } else {
    // Use weaker CSP to test normal content.
    yield pushPrefs(["security.signed_content.CSP.default", "script-src 'self' 'unsafe-inline'; style-src 'self'"]);
  }

  // start the test
  yield BrowserTestUtils.withNewTab({
      gBrowser,
      url: aUrl,
    },
    function * (browser) {
      // check if everything's set correct for testing
      ok(Services.prefs.getBoolPref(
          "browser.newtabpage.remote.content-signing-test"),
          "sanity check: remote newtab signing test should be used");
      ok(Services.prefs.getBoolPref("browser.newtabpage.remote"),
          "sanity check: remote newtab should be used");
      // we only check this if we really do a newtab test
      if (aNewTabPref) {
        ok(aboutNewTabService.overridden,
            "sanity check: default URL for about:newtab should be overriden");
        is(aboutNewTabService.newTabURL, aNewTabPref,
            "sanity check: default URL for about:newtab should return the new URL");
      }

      // Every valid remote newtab page must have built-in CSP.
      let shouldHaveCSP = ((aUrl === ABOUT_NEWTAB_URI) &&
                          (aNewTabPref === URI_GOOD || aNewTabPref === URI_SRI));

      if (shouldHaveCSP) {
        is(browser.contentDocument.nodePrincipal.cspJSON, SIGNED_CONTENT_CSP,
           "Valid remote newtab page must have built-in CSP.");
      }

      yield ContentTask.spawn(
          browser, aExpectedStrings, function * (aExpectedStrings) {
            for (let expectedString of aExpectedStrings) {
              ok(content.document.documentElement.innerHTML.includes(expectedString),
                 "Expect the following value in the result\n" + expectedString +
                 "\nand got " + content.document.documentElement.innerHTML);
            }
          });

      // for good test cases we check if a reload fails if the remote page
      // changed from valid to invalid in the meantime
      if (reload) {
        yield BrowserTestUtils.withNewTab({
            gBrowser,
            url: INVALIDATE_FILE,
          },
          function * (browser2) {
            yield ContentTask.spawn(browser2, null, function * () {
              ok(content.document.documentElement.innerHTML.includes("Done"),
                 "Expect the following value in the result\n" + "Done" +
                 "\nand got " + content.document.documentElement.innerHTML);
            });
          }
        );

        browser.reload();
        yield BrowserTestUtils.browserLoaded(browser);

        let expectedStrings = [ABOUT_BLANK];
        if (aNewTabPref == URI_SRI) {
          expectedStrings = [
            STYLESHEET_WITHOUT_SRI_BLOCKED,
            STYLESHEET_WITH_SRI_BLOCKED,
            SCRIPT_WITHOUT_SRI_BLOCKED,
            SCRIPT_WITH_SRI_BLOCKED
          ];
        }
        yield ContentTask.spawn(browser, expectedStrings,
          function * (expectedStrings) {
            for (let expectedString of expectedStrings) {
              ok(content.document.documentElement.innerHTML.includes(expectedString),
                 "Expect the following value in the result\n" + expectedString +
                 "\nand got " + content.document.documentElement.innerHTML);
            }
          }
        );

        yield BrowserTestUtils.withNewTab({
            gBrowser,
            url: VALIDATE_FILE,
          },
          function * (browser2) {
            yield ContentTask.spawn(browser2, null, function * () {
              ok(content.document.documentElement.innerHTML.includes("Done"),
                 "Expect the following value in the result\n" + "Done" +
                 "\nand got " + content.document.documentElement.innerHTML);
              });
          }
        );
      }
    }
  );
}

add_task(function * test() {
  // run tests from TESTS
  for (let i = 0; i < TESTS.length; i++) {
    let testCase = TESTS[i];
    let url = "", aNewTabPref = "";
    let reload = false;
    var aExpectedStrings = testCase.testStrings;
    if (testCase.aboutURI) {
      url = ABOUT_NEWTAB_URI;
      aNewTabPref = testCase.aboutURI;
      if (aNewTabPref == URI_GOOD || aNewTabPref == URI_SRI) {
        reload = true;
      }
    } else {
      url = testCase.url;
    }

    yield doTest(aExpectedStrings, reload, url, aNewTabPref);
  }
});
