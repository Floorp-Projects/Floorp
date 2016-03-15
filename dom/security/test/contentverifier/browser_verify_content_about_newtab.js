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

const URI_BAD_FILE_CACHED = BASE + "sig=good&key=good&file=bad&header=good&cached=true";

const GOOD_ABOUT_STRING = "Just a fully good testpage for Bug 1226928";
const BAD_ABOUT_STRING = "Just a bad testpage for Bug 1226928";
const ABOUT_BLANK = "<head></head><body></body>";

const TESTS = [
  // { newtab (aboutURI) or regular load (url) : url,
  //   testString : expected string in the loaded page }
  { "aboutURI" : URI_GOOD, "testString" : GOOD_ABOUT_STRING },
  { "aboutURI" : URI_ERROR_HEADER, "testString" : ABOUT_BLANK },
  { "aboutURI" : URI_KEYERROR_HEADER, "testString" : ABOUT_BLANK },
  { "aboutURI" : URI_SIGERROR_HEADER, "testString" : ABOUT_BLANK },
  { "aboutURI" : URI_NO_HEADER, "testString" : ABOUT_BLANK },
  { "aboutURI" : URI_BAD_SIG, "testString" : ABOUT_BLANK },
  { "aboutURI" : URI_BROKEN_SIG, "testString" : ABOUT_BLANK },
  { "aboutURI" : URI_BAD_KEY, "testString" : ABOUT_BLANK },
  { "aboutURI" : URI_BAD_FILE, "testString" : ABOUT_BLANK },
  { "aboutURI" : URI_BAD_ALL, "testString" : ABOUT_BLANK },
  { "url" : URI_BAD_FILE_CACHED, "testString" : BAD_ABOUT_STRING },
  { "aboutURI" : URI_BAD_FILE_CACHED, "testString" : ABOUT_BLANK },
  { "aboutURI" : URI_GOOD, "testString" : GOOD_ABOUT_STRING }
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
function doTest(aExpectedString, reload, aUrl, aNewTabPref) {
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
      yield ContentTask.spawn(
          browser, aExpectedString, function * (aExpectedString) {
            ok(content.document.documentElement.innerHTML.includes(aExpectedString),
               "Expect the following value in the result\n" + aExpectedString +
               "\nand got " + content.document.documentElement.innerHTML);
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

        aExpectedString = ABOUT_BLANK;
        yield ContentTask.spawn(browser, aExpectedString,
          function * (aExpectedString) {
            ok(content.document.documentElement.innerHTML.includes(aExpectedString),
               "Expect the following value in the result\n" + aExpectedString +
               "\nand got " + content.document.documentElement.innerHTML);
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
    let aExpectedString = testCase.testString;
    if (testCase.aboutURI) {
      url = ABOUT_NEWTAB_URI;
      aNewTabPref = testCase.aboutURI;
      if (aExpectedString == GOOD_ABOUT_STRING) {
        reload = true;
      }
    } else {
      url = testCase.url;
    }

    yield doTest(aExpectedString, reload, url, aNewTabPref);
  }
});
