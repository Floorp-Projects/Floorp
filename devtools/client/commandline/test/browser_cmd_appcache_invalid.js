/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the appcache validate works as they should with an invalid
// manifest.

const TEST_URI = "http://sub1.test1.example.com/browser/devtools/client/commandline/" +
                 "test/browser_cmd_appcache_invalid_index.html";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  let lines = [
    "Manifest has a character encoding of ISO-8859-1. Manifests must have the " +
      "utf-8 character encoding.",
    "The first line of the manifest must be \u201cCACHE MANIFEST\u201d at line 1.",
    "\u201cCACHE MANIFEST\u201d is only valid on the first line but was found at line 3.",
    "images/sound-icon.png points to a resource that is not available at line 9.",
    "images/background.png points to a resource that is not available at line 10.",
    "/checking.cgi points to a resource that is not available at line 13.",
    "Asterisk (*) incorrectly used in the NETWORK section at line 14. If a line " +
      "in the NETWORK section contains only a single asterisk character, then any " +
      "URI not listed in the manifest will be treated as if the URI was listed in " +
      "the NETWORK section. Otherwise such URIs will be treated as unavailable. " +
    "Other uses of the * character are prohibited",
    "../rel.html points to a resource that is not available at line 17.",
    "../../rel.html points to a resource that is not available at line 18.",
    "../../../rel.html points to a resource that is not available at line 19.",
    "../../../../rel.html points to a resource that is not available at line 20.",
    "../../../../../rel.html points to a resource that is not available at line 21.",
    "/../ is not a valid URI prefix at line 22.",
    "/test.css points to a resource that is not available at line 23.",
    "/test.js points to a resource that is not available at line 24.",
    "test.png points to a resource that is not available at line 25.",
    "/main/features.js points to a resource that is not available at line 27.",
    "/main/settings/index.css points to a resource that is not available at line 28.",
    "http://example.com/scene.jpg points to a resource that is not available at line 29.",
    "/section1/blockedbyfallback.html points to a resource that is not available at line 30.",
    "http://example.com/images/world.jpg points to a resource that is not available at line 31.",
    "/section2/blockedbyfallback.html points to a resource that is not available at line 32.",
    "/main/home points to a resource that is not available at line 34.",
    "main/app.js points to a resource that is not available at line 35.",
    "/settings/home points to a resource that is not available at line 37.",
    "/settings/app.js points to a resource that is not available at line 38.",
    "The file http://sub1.test1.example.com/browser/devtools/client/" +
      "commandline/test/browser_cmd_appcache_invalid_page3.html was modified " +
      "after http://sub1.test1.example.com/browser/devtools/client/" +
      "commandline/test/browser_cmd_appcache_invalid_appcache.appcache. Unless " +
      "the text in the manifest file is changed the cached version will be used " +
      "instead at line 39.",
    "browser_cmd_appcache_invalid_page3.html has cache-control set to no-store. " +
      "This will prevent the application cache from storing the file at line 39.",
    "http://example.com/logo.png points to a resource that is not available at line 40.",
    "http://example.com/check.png points to a resource that is not available at line 41.",
    "Spaces in URIs need to be replaced with % at line 42.",
    "http://example.com/cr oss.png points to a resource that is not available at line 42.",
    "Asterisk (*) incorrectly used in the CACHE section at line 43. If a line " +
      "in the NETWORK section contains only a single asterisk character, then " +
      "any URI not listed in the manifest will be treated as if the URI was " +
      "listed in the NETWORK section. Otherwise such URIs will be treated as " +
      "unavailable. Other uses of the * character are prohibited",
    "The SETTINGS section may only contain a single value, \u201cprefer-online\u201d or \u201cfast\u201d at line 47.",
    "FALLBACK section line 50 (/section1/ /offline1.html) prevents caching of " +
      "line 30 (/section1/blockedbyfallback.html) in the CACHE section.",
    "/offline1.html points to a resource that is not available at line 50.",
    "FALLBACK section line 51 (/section2/ offline2.html) prevents caching of " +
      "line 32 (/section2/blockedbyfallback.html) in the CACHE section.",
    "offline2.html points to a resource that is not available at line 51.",
    "Only two URIs separated by spaces are allowed in the FALLBACK section at line 52.",
    "Asterisk (*) incorrectly used in the FALLBACK section at line 53. URIs " +
      "in the FALLBACK section simply need to match a prefix of the request URI.",
    "offline3.html points to a resource that is not available at line 53.",
    "Invalid section name (BLAH) at line 55.",
    "Only two URIs separated by spaces are allowed in the FALLBACK section at line 55."
  ];

  let options = yield helpers.openTab(TEST_URI);
  info("window open");

  // Wait for site to be cached.
  yield helpers.listenOnce(gBrowser.contentWindow.applicationCache, "error");
  info("applicationCache error happened");

  yield helpers.openToolbar(options);
  info("toolbar open");

  // Pages containing an appcache the notification bar gives options to allow
  // or deny permission for the app to save data offline. Let's click Allow.
  let notificationID = "offline-app-requested-sub1.test1.example.com";
  let notification =
    PopupNotifications.getNotification(notificationID, gBrowser.selectedBrowser);

  if (notification) {
    info("Authorizing offline storage.");
    notification.mainAction.callback();
  } else {
    info("No notification box is available.");
  }

  info("Site now cached, running tests.");
  yield helpers.audit(options, [
    {
      setup: "appcache validate",
      check: {
        input:  "appcache validate",
        markup: "VVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {}
      },
      exec: {
        output: lines.map(getRegexForString)
      },
    },
  ]);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}

/**
 * Creates a regular expression that matches a string. This greatly simplifies
 * matching and debugging long strings.
 *
 * @param {String} text
 *        Text to convert
 * @return {RegExp}
 *         Regular expression matching text
 */
function getRegexForString(str) {
  str = str.replace(/(\.|\\|\/|\(|\)|\[|\]|\*|\+|\?|\$|\^|\|)/g, "\\$1");
  return new RegExp(str);
}
