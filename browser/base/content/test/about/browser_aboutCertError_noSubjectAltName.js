/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BROWSER_NAME = document
  .getElementById("bundle_brand")
  .getString("brandShortName");
const UNKNOWN_ISSUER = "https://no-subject-alt-name.example.com:443";

const checkAdvancedAndGetTechnicalInfoText = async () => {
  let doc = content.document;

  let advancedButton = doc.getElementById("advancedButton");
  ok(advancedButton, "advancedButton found");
  is(
    advancedButton.hasAttribute("disabled"),
    false,
    "advancedButton should be clickable"
  );
  advancedButton.click();

  let badCertAdvancedPanel = doc.getElementById("badCertAdvancedPanel");
  ok(badCertAdvancedPanel, "badCertAdvancedPanel found");

  let badCertTechnicalInfo = doc.getElementById("badCertTechnicalInfo");
  ok(badCertTechnicalInfo, "badCertTechnicalInfo found");

  // Wait until fluent sets the errorCode inner text.
  await ContentTaskUtils.waitForCondition(() => {
    let errorCode = doc.getElementById("errorCode");
    return errorCode.textContent == "SSL_ERROR_BAD_CERT_DOMAIN";
  }, "correct error code has been set inside the advanced button panel");

  let viewCertificate = doc.getElementById("viewCertificate");
  ok(viewCertificate, "viewCertificate found");

  return badCertTechnicalInfo.innerHTML;
};

const checkCorrectMessages = message => {
  let isCorrectMessage = message.includes(
    "Websites prove their identity via certificates. " +
      BROWSER_NAME +
      " does not trust this site because it uses a certificate that is" +
      " not valid for no-subject-alt-name.example.com"
  );
  is(isCorrectMessage, true, "That message should appear");
  let isWrongMessage = message.includes("The certificate is only valid for ");
  is(isWrongMessage, false, "That message shouldn't appear");
};

add_task(async function checkUntrustedCertError() {
  info(
    `Loading ${UNKNOWN_ISSUER} which does not have a subject specified in the certificate`
  );
  let tab = await openErrorPage(UNKNOWN_ISSUER);
  let browser = tab.linkedBrowser;
  info("Clicking the exceptionDialogButton in advanced panel");
  let badCertTechnicalInfoText = await ContentTask.spawn(
    browser,
    null,
    checkAdvancedAndGetTechnicalInfoText
  );
  checkCorrectMessages(badCertTechnicalInfoText, browser);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
