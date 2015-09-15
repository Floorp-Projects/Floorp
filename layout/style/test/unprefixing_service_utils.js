/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Shared data & functionality used in tests for CSS Unprefixing Service.

// Whitelisted hosts:
// (per implementation of nsPrincipal::IsOnCSSUnprefixingWhitelist())
var gWhitelistedHosts = [
  // test1.example.org is on the whitelist.
  "test1.example.org",
  // test2.example.org is on the "allow all subdomains" whitelist.
  "test2.example.org",
  "sub1.test2.example.org",
  "sub2.test2.example.org"
];

// *NOT* whitelisted hosts:
var gNotWhitelistedHosts = [
  // Though test1.example.org is on the whitelist, its subdomains are not.
  "sub1.test1.example.org",
  // mochi.test is not on the whitelist.
  "mochi.test:8888"
];

// Names of prefs:
const PREF_UNPREFIXING_SERVICE =
  "layout.css.unprefixing-service.enabled";
const PREF_INCLUDE_TEST_DOMAINS =
  "layout.css.unprefixing-service.include-test-domains";

// Helper-function to make unique URLs in testHost():
var gCounter = 0;
function getIncreasingCounter() {
  return gCounter++;
}

// This function tests a particular host in our iframe.
// @param aHost           The host to be tested
// @param aExpectEnabled  Should we expect unprefixing to be enabled for host?
function testHost(aHost, aExpectEnabled) {
  // Build the URL:
  let url = window.location.protocol; // "http:" or "https:"
  url += "//";
  url += aHost;

  // Append the path-name, up to the actual filename (the final "/"):
  const re = /(.*\/).*/;
  url += window.location.pathname.replace(re, "$1");
  url += IFRAME_TESTFILE;
  // In case this is the same URL as last time, we add "?N" for some unique N,
  // to make each URL different, so that the iframe actually (re)loads:
  url += "?" + getIncreasingCounter();
  // We give the URL a #suffix to indicate to the test whether it should expect
  // that unprefixing is enabled or disabled:
  url += (aExpectEnabled ? "#expectEnabled" : "#expectDisabled");

  let iframe = document.getElementById("testIframe");
  iframe.contentWindow.location = url;
  // The iframe will report its results back via postMessage.
  // Our caller had better have set up a postMessage listener.
}

// Register a postMessage() handler, to allow our cross-origin iframe to
// communicate back to the main page's mochitest functionality.
// The handler expects postMessage to be called with an object like:
//   { type: ["is"|"ok"|"testComplete"], ... }
// The "is" and "ok" types will trigger the corresponding function to be
// called in the main page, with named arguments provided in the payload.
// The "testComplete" type will trigger the passed-in aTestCompleteCallback
// function to be invoked (e.g. to advance to the next testcase, or to finish
// the overall test, as-appropriate).
function registerPostMessageListener(aTestCompleteCallback) {
  let receiveMessage = function(event) {
    if (event.data.type === "is") {
      is(event.data.actual, event.data.expected, event.data.desc);
    } else if (event.data.type === "ok") {
      ok(event.data.condition, event.data.desc);
    } else if (event.data.type === "testComplete") {
      aTestCompleteCallback();
    } else {
      ok(false, "unrecognized data in postMessage call");
    }
  };

  window.addEventListener("message", receiveMessage, false);
}
