/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const MANY_SANS_ISSUER = "https://many-subject-alt-names.example.com:443";

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
    "AN_1.example.com,  SAN_2.example.com,  SAN_3.example.com,  SAN_4.example.com,  SAN_5.example.com, SAN_6.example.com,  SAN_7.example.com,  SAN_8.example.com,  SAN_9.example.com,  SAN_10.example.com, SAN_11.example.com,  SAN_12.example.com,  SAN_13.example.com,  SAN_14.example.com,  SAN_15.example.com, SAN_16.example.com,  SAN_17.example.com,  SAN_18.example.com,  SAN_19.example.com,  SAN_20.example.com, SAN_21.example.com,  SAN_22.example.com,  SAN_23.example.com,  SAN_24.example.com,  SAN_25.example.com, SAN_26.example.com,  SAN_27.example.com,  SAN_28.example.com,  SAN_29.example.com,  SAN_30.example.com, SAN_31.example.com,  SAN_32.example.com,  SAN_33.example.com,  SAN_34.example.com,  SAN_35.example.com,  SAN_36.example.com,  SAN_37.example.com,  SAN_38.example.com,  SAN_39.example.com,  SAN_40.example.com, SAN_41.example.com,  SAN_42.example.com,  SAN_43.example.com,  SAN_44.example.com,  SAN_45.example.com, SAN_46.example.com,  SAN_47.example.com,  SAN_48.example.com,  SAN_49.example.com,  SAN_50.example.com"
  );
  is(isCorrectMessage, true, "Should displaying 50 SANs");
  let isWrongMessage = message.includes("{???}");
  is(isWrongMessage, false, "That message shouldn't appear");
  let isWrongMessage2 = message.includes("SAN_51.example.com");
  is(isWrongMessage2, false, "Should not display more then 50 SANs");
};

add_task(async function checkUntrustedCertError() {
  info(
    `Loading ${MANY_SANS_ISSUER} which does not have a subject specified in the certificate`
  );
  let tab = await openErrorPage(MANY_SANS_ISSUER);
  let browser = tab.linkedBrowser;
  info("Clicking the exceptionDialogButton in advanced panel");
  let badCertTechnicalInfoText = await SpecialPowers.spawn(
    browser,
    [],
    checkAdvancedAndGetTechnicalInfoText
  );
  checkCorrectMessages(badCertTechnicalInfoText, browser);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
