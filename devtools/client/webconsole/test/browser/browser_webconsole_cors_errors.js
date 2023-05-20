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

registerCleanupFunction(async function () {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
});

add_task(async function () {
  await pushPref("devtools.webconsole.filter.netxhr", true);

  const hud = await openNewTabAndConsole(TEST_URI);

  let onCorsMessage;
  let message;

  info(`Setting "content.cors.disable" to true to test CORSDisabled message`);
  await pushPref("content.cors.disable", true);
  onCorsMessage = waitForMessageByType(hud, "Reason: CORS disabled", ".error");
  makeFaultyCorsCall("CORSDisabled");
  message = await onCorsMessage;
  await checkCorsMessage(hud, message, "CORSDisabled");
  await pushPref("content.cors.disable", false);

  info("Test CORSPreflightDidNotSucceed");
  onCorsMessage = waitForMessageByType(
    hud,
    `(Reason: CORS preflight response did not succeed). Status code: `,
    ".error"
  );
  makeFaultyCorsCall("CORSPreflightDidNotSucceed");
  message = await onCorsMessage;
  await checkCorsMessage(hud, message, "CORSPreflightDidNotSucceed");

  info("Test CORS did not succeed");
  onCorsMessage = waitForMessageByType(
    hud,
    "(Reason: CORS request did not succeed). Status code: ",
    ".error"
  );
  makeFaultyCorsCall("CORSDidNotSucceed");
  message = await onCorsMessage;
  await checkCorsMessage(hud, message, "CORSDidNotSucceed");

  info("Test CORSExternalRedirectNotAllowed");
  onCorsMessage = waitForMessageByType(
    hud,
    "Reason: CORS request external redirect not allowed",
    ".error"
  );
  makeFaultyCorsCall("CORSExternalRedirectNotAllowed");
  message = await onCorsMessage;
  await checkCorsMessage(hud, message, "CORSExternalRedirectNotAllowed");

  info("Test CORSMissingAllowOrigin");
  onCorsMessage = waitForMessageByType(
    hud,
    `(Reason: CORS header ${quote(
      "Access-Control-Allow-Origin"
    )} missing). Status code: `,
    ".error"
  );
  makeFaultyCorsCall("CORSMissingAllowOrigin");
  message = await onCorsMessage;
  await checkCorsMessage(hud, message, "CORSMissingAllowOrigin");

  info("Test CORSMultipleAllowOriginNotAllowed");
  onCorsMessage = waitForMessageByType(
    hud,
    `Reason: Multiple CORS header ${quote(
      "Access-Control-Allow-Origin"
    )} not allowed`,
    ".error"
  );
  makeFaultyCorsCall("CORSMultipleAllowOriginNotAllowed");
  message = await onCorsMessage;
  await checkCorsMessage(hud, message, "CORSMultipleAllowOriginNotAllowed");

  info("Test CORSAllowOriginNotMatchingOrigin");
  onCorsMessage = waitForMessageByType(
    hud,
    `Reason: CORS header ` +
      `${quote("Access-Control-Allow-Origin")} does not match ${quote(
        "mochi.test"
      )}`,
    ".error"
  );
  makeFaultyCorsCall("CORSAllowOriginNotMatchingOrigin");
  message = await onCorsMessage;
  await checkCorsMessage(hud, message, "CORSAllowOriginNotMatchingOrigin");

  info("Test CORSNotSupportingCredentials");
  onCorsMessage = waitForMessageByType(
    hud,
    `Reason: Credential is not supported if the CORS ` +
      `header ${quote("Access-Control-Allow-Origin")} is ${quote("*")}`,
    ".error"
  );
  makeFaultyCorsCall("CORSNotSupportingCredentials");
  message = await onCorsMessage;
  await checkCorsMessage(hud, message, "CORSNotSupportingCredentials");

  info("Test CORSMethodNotFound");
  onCorsMessage = waitForMessageByType(
    hud,
    `Reason: Did not find method in CORS header ` +
      `${quote("Access-Control-Allow-Methods")}`,
    ".error"
  );
  makeFaultyCorsCall("CORSMethodNotFound");
  message = await onCorsMessage;
  await checkCorsMessage(hud, message, "CORSMethodNotFound");

  info("Test CORSMissingAllowCredentials");
  onCorsMessage = waitForMessageByType(
    hud,
    `Reason: expected ${quote("true")} in CORS ` +
      `header ${quote("Access-Control-Allow-Credentials")}`,
    ".error"
  );
  makeFaultyCorsCall("CORSMissingAllowCredentials");
  message = await onCorsMessage;
  await checkCorsMessage(hud, message, "CORSMissingAllowCredentials");

  info("Test CORSInvalidAllowMethod");
  onCorsMessage = waitForMessageByType(
    hud,
    `Reason: invalid token ${quote("xyz;")} in CORS ` +
      `header ${quote("Access-Control-Allow-Methods")}`,
    ".error"
  );
  makeFaultyCorsCall("CORSInvalidAllowMethod");
  message = await onCorsMessage;
  await checkCorsMessage(hud, message, "CORSInvalidAllowMethod");

  info("Test CORSInvalidAllowHeader");
  onCorsMessage = waitForMessageByType(
    hud,
    `Reason: invalid token ${quote("xyz;")} in CORS ` +
      `header ${quote("Access-Control-Allow-Headers")}`,
    ".error"
  );
  makeFaultyCorsCall("CORSInvalidAllowHeader");
  message = await onCorsMessage;
  await checkCorsMessage(hud, message, "CORSInvalidAllowHeader");

  info("Test CORSMissingAllowHeaderFromPreflight");
  onCorsMessage = waitForMessageByType(
    hud,
    `Reason: header ${quote("xyz")} is not allowed according to ` +
      `header ${quote(
        "Access-Control-Allow-Headers"
      )} from CORS preflight response`,
    ".error"
  );
  makeFaultyCorsCall("CORSMissingAllowHeaderFromPreflight");
  message = await onCorsMessage;
  await checkCorsMessage(hud, message, "CORSMissingAllowHeaderFromPreflight");

  // See Bug 1480671.
  // XXX: how to make Origin to not be included in the request ?
  // onCorsMessage = waitForMessageByType(hud,
  //   `Reason: CORS header ${quote("Origin")} cannot be added`,
  //   ".error");
  // makeFaultyCorsCall("CORSOriginHeaderNotAdded");
  // message = await onCorsMessage;
  // await checkCorsMessage(hud, message, "CORSOriginHeaderNotAdded");

  // See Bug 1480672.
  // XXX: Failing with another error: Console message: Security Error: Content at
  // http://example.com/browser/devtools/client/webconsole/test/browser/test-network-request.html
  // may not load or link to file:///Users/nchevobbe/Projects/mozilla-central/devtools/client/webconsole/test/browser/sjs_cors-test-server.sjs.
  // info("Test CORSRequestNotHttp");
  // onCorsMessage = waitForMessageByType(hud, "Reason: CORS request not http",
  //   ".error");
  // const dir = getChromeDir(getResolvedURI(gTestPath));
  // dir.append("sjs_cors-test-server.sjs");
  // makeFaultyCorsCall("CORSRequestNotHttp", Services.io.newFileURI(dir).spec);
  // message = await onCorsMessage;
  // await checkCorsMessage(hud, message, "CORSRequestNotHttp");
});

async function checkCorsMessage(hud, message, category) {
  // Get a new reference to the node, as it may have been scrolled out of existence.
  const node = await findMessageVirtualizedById({
    hud,
    messageId: message.node.getAttribute("data-message-id"),
  });
  node.scrollIntoView();
  ok(
    node.classList.contains("error"),
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
  SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [[errorCategory, corsUrl]],
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
