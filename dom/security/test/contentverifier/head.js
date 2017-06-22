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
const URI_GOOD = BASE + "sig=good&x5u=good&file=good&header=good";

const INVALIDATE_FILE = BASE + "invalidateFile=yep";
const VALIDATE_FILE = BASE + "validateFile=yep";

const URI_HEADER_BASE = BASE + "sig=good&x5u=good&file=good&header=";
const URI_ERROR_HEADER = URI_HEADER_BASE + "error";
const URI_KEYERROR_HEADER = URI_HEADER_BASE + "errorInX5U";
const URI_SIGERROR_HEADER = URI_HEADER_BASE + "errorInSignature";
const URI_NO_HEADER = URI_HEADER_BASE + "noHeader";

const URI_BAD_SIG = BASE + "sig=bad&x5u=good&file=good&header=good";
const URI_BROKEN_SIG = BASE + "sig=broken&x5u=good&file=good&header=good";
const URI_BAD_X5U = BASE + "sig=good&x5u=bad&file=good&header=good";
const URI_HTTP_X5U = BASE + "sig=good&x5u=http&file=good&header=good";
const URI_BAD_FILE = BASE + "sig=good&x5u=good&file=bad&header=good";
const URI_BAD_ALL = BASE + "sig=bad&x5u=bad&file=bad&header=bad";
const URI_BAD_CSP = BASE + "sig=bad-csp&x5u=good&file=bad-csp&header=good";

const URI_BAD_FILE_CACHED = BASE + "sig=good&x5u=good&file=bad&header=good&cached=true";

const GOOD_ABOUT_STRING = "Just a fully good testpage for Bug 1226928";
const BAD_ABOUT_STRING = "Just a bad testpage for Bug 1226928";
const ABOUT_BLANK = "<head></head><body></body>";

const URI_CLEANUP = BASE + "cleanup=true";
const CLEANUP_DONE = "Done";

const URI_SRI = BASE + "sig=sri&x5u=good&file=sri&header=good";
const STYLESHEET_WITHOUT_SRI_BLOCKED = "Stylesheet without SRI blocked";
const STYLESHEET_WITH_SRI_BLOCKED = "Stylesheet with SRI blocked";
const STYLESHEET_WITH_SRI_LOADED = "Stylesheet with SRI loaded";
const SCRIPT_WITHOUT_SRI_BLOCKED = "Script without SRI blocked";
const SCRIPT_WITH_SRI_BLOCKED = "Script with SRI blocked";
const SCRIPT_WITH_SRI_LOADED = "Script with SRI loaded";

const CSP_TEST_SUCCESS_STRING = "CSP violation test succeeded.";

// Needs to sync with pref "security.signed_content.CSP.default".
const SIGNED_CONTENT_CSP = `{"csp-policies":[{"report-only":false,"script-src":["https://example.com","'unsafe-inline'"],"style-src":["https://example.com"]}]}`;

var browser = null;
var aboutNewTabService = Cc["@mozilla.org/browser/aboutnewtab-service;1"]
                           .getService(Ci.nsIAboutNewTabService);

function pushPrefs(...aPrefs) {
  return SpecialPowers.pushPrefEnv({"set": aPrefs});
}

/*
 * run tests with input from TESTS
 */
async function doTest(aExpectedStrings, reload, aUrl, aNewTabPref) {
  // set about:newtab location for this test if it's a newtab test
  if (aNewTabPref) {
    aboutNewTabService.newTabURL = aNewTabPref;
  }

  // set prefs
  await pushPrefs(
      ["browser.newtabpage.remote.content-signing-test", true],
      ["browser.newtabpage.remote", true],
      ["security.content.signature.root_hash",
       "CC:BE:04:87:74:B2:98:24:4A:C6:7A:71:BC:6F:DB:D6:C0:48:17:29:57:51:96:47:38:CC:24:C8:E4:F9:DD:CB"]);

  if (aNewTabPref === URI_BAD_CSP) {
    // Use stricter CSP to test CSP violation.
    await pushPrefs(["security.signed_content.CSP.default", "script-src 'self'; style-src 'self'"]);
  } else {
    // Use weaker CSP to test normal content.
    await pushPrefs(["security.signed_content.CSP.default", "script-src 'self' 'unsafe-inline'; style-src 'self'"]);
  }

  // start the test
  await BrowserTestUtils.withNewTab({
      gBrowser,
      url: aUrl,
    },
    async function(browser) {
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

      await ContentTask.spawn(
          browser, aExpectedStrings, async function(aExpectedStrings) {
            for (let expectedString of aExpectedStrings) {
              ok(content.document.documentElement.innerHTML.includes(expectedString),
                 "Expect the following value in the result\n" + expectedString +
                 "\nand got " + content.document.documentElement.innerHTML);
            }
          });

      // for good test cases we check if a reload fails if the remote page
      // changed from valid to invalid in the meantime
      if (reload) {
        await BrowserTestUtils.withNewTab({
            gBrowser,
            url: INVALIDATE_FILE,
          },
          async function(browser2) {
            await ContentTask.spawn(browser2, null, async function() {
              ok(content.document.documentElement.innerHTML.includes("Done"),
                 "Expect the following value in the result\n" + "Done" +
                 "\nand got " + content.document.documentElement.innerHTML);
            });
          }
        );

        browser.reload();
        await BrowserTestUtils.browserLoaded(browser);

        let expectedStrings = [ABOUT_BLANK];
        if (aNewTabPref == URI_SRI) {
          expectedStrings = [
            STYLESHEET_WITHOUT_SRI_BLOCKED,
            STYLESHEET_WITH_SRI_BLOCKED,
            SCRIPT_WITHOUT_SRI_BLOCKED,
            SCRIPT_WITH_SRI_BLOCKED
          ];
        }
        await ContentTask.spawn(browser, expectedStrings,
          async function(expectedStrings) {
            for (let expectedString of expectedStrings) {
              ok(content.document.documentElement.innerHTML.includes(expectedString),
                 "Expect the following value in the result\n" + expectedString +
                 "\nand got " + content.document.documentElement.innerHTML);
            }
          }
        );

        await BrowserTestUtils.withNewTab({
            gBrowser,
            url: VALIDATE_FILE,
          },
          async function(browser2) {
            await ContentTask.spawn(browser2, null, async function() {
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

async function runTests() {
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

    await doTest(aExpectedStrings, reload, url, aNewTabPref);
  }
}
