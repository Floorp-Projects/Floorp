/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure that the different CORS error are logged to the console with the appropriate
// "Learn more" link.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/test-network-request.html";
const BASE_CORS_ERROR_URL =
  "https://developer.mozilla.org/docs/Web/HTTP/CORS/Errors/";
const BASE_CORS_ERROR_URL_PARAMS = new URLSearchParams({
  utm_source: "devtools",
  utm_medium: "firefox-cors-errors",
  utm_campaign: "default",
});

add_task(async function() {
  await pushPref("devtools.webconsole.filter.netxhr", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  let onCorsMessage;
  let message;

  info(`Setting "content.cors.disable" to true to test CORSDisabled message`);
  await pushPref("content.cors.disable", true);
  onCorsMessage = waitForMessage(hud, "Reason: CORS disabled");
  makeFaultyCorsCall("CORSDisabled");
  message = await onCorsMessage;
  await checkCorsMessage(message, "CORSDisabled");
  await pushPref("content.cors.disable", false);

  info("Test CORSPreflightDidNotSucceed");
  onCorsMessage = waitForMessage(hud, `CORS preflight channel did not succeed`);
  makeFaultyCorsCall("CORSPreflightDidNotSucceed");
  message = await onCorsMessage;
  await checkCorsMessage(message, "CORSPreflightDidNotSucceed");

  info("Test CORS did not succeed");
  onCorsMessage = waitForMessage(hud, "Reason: CORS request did not succeed");
  makeFaultyCorsCall("CORSDidNotSucceed");
  message = await onCorsMessage;
  await checkCorsMessage(message, "CORSDidNotSucceed");

  info("Test CORSExternalRedirectNotAllowed");
  onCorsMessage = waitForMessage(
    hud,
    "Reason: CORS request external redirect not allowed"
  );
  makeFaultyCorsCall("CORSExternalRedirectNotAllowed");
  message = await onCorsMessage;
  await checkCorsMessage(message, "CORSExternalRedirectNotAllowed");

  info("Test CORSMissingAllowOrigin");
  onCorsMessage = waitForMessage(
    hud,
    `Reason: CORS header ${quote("Access-Control-Allow-Origin")} missing`
  );
  makeFaultyCorsCall("CORSMissingAllowOrigin");
  message = await onCorsMessage;
  await checkCorsMessage(message, "CORSMissingAllowOrigin");

  info("Test CORSMultipleAllowOriginNotAllowed");
  onCorsMessage = waitForMessage(
    hud,
    `Reason: Multiple CORS header ${quote(
      "Access-Control-Allow-Origin"
    )} not allowed`
  );
  makeFaultyCorsCall("CORSMultipleAllowOriginNotAllowed");
  message = await onCorsMessage;
  await checkCorsMessage(message, "CORSMultipleAllowOriginNotAllowed");

  info("Test CORSAllowOriginNotMatchingOrigin");
  onCorsMessage = waitForMessage(
    hud,
    `Reason: CORS header ` +
      `${quote("Access-Control-Allow-Origin")} does not match ${quote(
        "mochi.test"
      )}`
  );
  makeFaultyCorsCall("CORSAllowOriginNotMatchingOrigin");
  message = await onCorsMessage;
  await checkCorsMessage(message, "CORSAllowOriginNotMatchingOrigin");

  info("Test CORSNotSupportingCredentials");
  onCorsMessage = waitForMessage(
    hud,
    `Reason: Credential is not supported if the CORS ` +
      `header ${quote("Access-Control-Allow-Origin")} is ${quote("*")}`
  );
  makeFaultyCorsCall("CORSNotSupportingCredentials");
  message = await onCorsMessage;
  await checkCorsMessage(message, "CORSNotSupportingCredentials");

  info("Test CORSMethodNotFound");
  onCorsMessage = waitForMessage(
    hud,
    `Reason: Did not find method in CORS header ` +
      `${quote("Access-Control-Allow-Methods")}`
  );
  makeFaultyCorsCall("CORSMethodNotFound");
  message = await onCorsMessage;
  await checkCorsMessage(message, "CORSMethodNotFound");

  info("Test CORSMissingAllowCredentials");
  onCorsMessage = waitForMessage(
    hud,
    `Reason: expected ${quote("true")} in CORS ` +
      `header ${quote("Access-Control-Allow-Credentials")}`
  );
  makeFaultyCorsCall("CORSMissingAllowCredentials");
  message = await onCorsMessage;
  await checkCorsMessage(message, "CORSMissingAllowCredentials");

  info("Test CORSInvalidAllowMethod");
  onCorsMessage = waitForMessage(
    hud,
    `Reason: invalid token ${quote("xyz;")} in CORS ` +
      `header ${quote("Access-Control-Allow-Methods")}`
  );
  makeFaultyCorsCall("CORSInvalidAllowMethod");
  message = await onCorsMessage;
  await checkCorsMessage(message, "CORSInvalidAllowMethod");

  info("Test CORSInvalidAllowHeader");
  onCorsMessage = waitForMessage(
    hud,
    `Reason: invalid token ${quote("xyz;")} in CORS ` +
      `header ${quote("Access-Control-Allow-Headers")}`
  );
  makeFaultyCorsCall("CORSInvalidAllowHeader");
  message = await onCorsMessage;
  await checkCorsMessage(message, "CORSInvalidAllowHeader");

  info("Test CORSMissingAllowHeaderFromPreflight");
  onCorsMessage = waitForMessage(
    hud,
    `Reason: missing token ${quote("xyz")} in CORS ` +
      `header ${quote(
        "Access-Control-Allow-Headers"
      )} from CORS preflight channel`
  );
  makeFaultyCorsCall("CORSMissingAllowHeaderFromPreflight");
  message = await onCorsMessage;
  await checkCorsMessage(message, "CORSMissingAllowHeaderFromPreflight");

  // See Bug 1480671.
  // XXX: how to make Origin to not be included in the request ?
  // onCorsMessage = waitForMessage(hud,
  //   `Reason: CORS header ${quote("Origin")} cannot be added`);
  // makeFaultyCorsCall("CORSOriginHeaderNotAdded");
  // message = await onCorsMessage;
  // await checkCorsMessage(message, "CORSOriginHeaderNotAdded");

  // See Bug 1480672.
  // XXX: Failing with another error: Console message: Security Error: Content at
  // http://example.com/browser/devtools/client/webconsole/test/browser/test-network-request.html
  // may not load or link to file:///Users/nchevobbe/Projects/mozilla-central/devtools/client/webconsole/test/browser/sjs_cors-test-server.sjs.
  // info("Test CORSRequestNotHttp");
  // onCorsMessage = waitForMessage(hud, "Reason: CORS request not http");
  // const dir = getChromeDir(getResolvedURI(gTestPath));
  // dir.append("sjs_cors-test-server.sjs");
  // makeFaultyCorsCall("CORSRequestNotHttp", Services.io.newFileURI(dir).spec);
  // message = await onCorsMessage;
  // await checkCorsMessage(message, "CORSRequestNotHttp");
});

async function checkCorsMessage(message, category) {
  const node = message.node;
  ok(
    node.classList.contains("warn"),
    "The cors message has the expected classname"
  );
  const learnMoreLink = node.querySelector(".learn-more-link");
  ok(learnMoreLink, "There is a Learn more link displayed");
  const linkSimulation = await simulateLinkClick(learnMoreLink);
  is(
    linkSimulation.link,
    getCategoryUrl(category),
    "Click on the link opens the expected page"
  );
}

function makeFaultyCorsCall(errorCategory, corsUrl) {
  ContentTask.spawn(
    gBrowser.selectedBrowser,
    [errorCategory, corsUrl],
    ([category, url]) => {
      if (!url) {
        const baseUrl =
          "http://mochi.test:8888/browser/devtools/client/webconsole/test/browser";
        url = `${baseUrl}/sjs_cors-test-server.sjs?corsErrorCategory=${category}`;
      }

      // Preflight request are not made for GET requests, so let's do a PUT.
      const method = "PUT";
      const options = { method };
      if (
        category === "CORSNotSupportingCredentials" ||
        category === "CORSMissingAllowCredentials"
      ) {
        options.credentials = "include";
      }

      if (category === "CORSMissingAllowHeaderFromPreflight") {
        options.headers = new content.Headers({ xyz: true });
      }

      content.fetch(url, options);
    }
  );
}

function quote(str) {
  const openingQuote = String.fromCharCode(8216);
  const closingQuote = String.fromCharCode(8217);
  return `${openingQuote}${str}${closingQuote}`;
}

function getCategoryUrl(category) {
  return `${BASE_CORS_ERROR_URL}${category}?${BASE_CORS_ERROR_URL_PARAMS}`;
}
