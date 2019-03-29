/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BAD_CERT = "https://expired.example.com/";
const BAD_STS_CERT = "https://badchain.include-subdomains.pinning.example.com:443";
const PREF_PERMANENT_OVERRIDE = "security.certerrors.permanentOverride";

add_task(async function checkExceptionDialogButton() {
  info("Loading a bad cert page and making sure the exceptionDialogButton directly adds an exception");
  let tab = await openErrorPage(BAD_CERT);
  let browser = tab.linkedBrowser;
  let loaded = BrowserTestUtils.browserLoaded(browser, false, BAD_CERT);
  info("Clicking the exceptionDialogButton in advanced panel");
  await ContentTask.spawn(browser, null, async function() {
    let doc = content.document;
    let exceptionButton = doc.getElementById("exceptionDialogButton");
    exceptionButton.click();
  });

  info("Loading the url after adding exception");
  await loaded;

  await ContentTask.spawn(browser, null, async function() {
    let doc = content.document;
    ok(!doc.documentURI.startsWith("about:certerror"), "Exception has been added");
  });

  let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                              .getService(Ci.nsICertOverrideService);
  certOverrideService.clearValidityOverride("expired.example.com", -1);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function checkPermanentExceptionPref() {
  info("Loading a bad cert page and making sure the permanent state of exceptions can be controlled via pref");

  for (let permanentOverride of [false, true]) {
    Services.prefs.setBoolPref(PREF_PERMANENT_OVERRIDE, permanentOverride);

    let tab = await openErrorPage(BAD_CERT);
    let browser = tab.linkedBrowser;
    let loaded = BrowserTestUtils.browserLoaded(browser, false, BAD_CERT);
    info("Clicking the exceptionDialogButton in advanced panel");
    let securityInfoAsString = await ContentTask.spawn(browser, null, async function() {
      let doc = content.document;
      let exceptionButton = doc.getElementById("exceptionDialogButton");
      exceptionButton.click();
      let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                       .getService(Ci.nsISerializationHelper);
      let serializable =  content.docShell.failedChannel.securityInfo
                                 .QueryInterface(Ci.nsITransportSecurityInfo)
                                 .QueryInterface(Ci.nsISerializable);
      return serhelper.serializeToString(serializable);
    });

    info("Loading the url after adding exception");
    await loaded;

    await ContentTask.spawn(browser, null, async function() {
      let doc = content.document;
      ok(!doc.documentURI.startsWith("about:certerror"), "Exception has been added");
    });

    let certOverrideService = Cc["@mozilla.org/security/certoverride;1"]
                                .getService(Ci.nsICertOverrideService);

    let isTemporary = {};
    let cert = getSecurityInfo(securityInfoAsString).serverCert;
    let hasException =
      certOverrideService.hasMatchingOverride("expired.example.com", -1, cert, {}, isTemporary);
    ok(hasException, "Has stored an exception for the page.");
    is(isTemporary.value, !permanentOverride,
      `Has stored a ${permanentOverride ? "permanent" : "temporary"} exception for the page.`);

    certOverrideService.clearValidityOverride("expired.example.com", -1);
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }

  Services.prefs.clearUserPref(PREF_PERMANENT_OVERRIDE);
});

add_task(async function checkBadStsCert() {
  info("Loading a badStsCert and making sure exception button doesn't show up");

  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(BAD_STS_CERT, useFrame);
    let browser = tab.linkedBrowser;

    await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let doc = frame ? content.document.querySelector("iframe").contentDocument : content.document;
      let exceptionButton = doc.getElementById("exceptionDialogButton");
      ok(ContentTaskUtils.is_hidden(exceptionButton), "Exception button is hidden.");
    });

    let message = await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let doc = frame ? content.document.querySelector("iframe").contentDocument : content.document;
      let advancedButton = doc.getElementById("advancedButton");
      advancedButton.click();
      return doc.getElementById("badCertTechnicalInfo").textContent;
    });
    ok(message.includes("SSL_ERROR_BAD_CERT_DOMAIN"), "Didn't find SSL_ERROR_BAD_CERT_DOMAIN.");
    ok(message.includes("The certificate is only valid for"), "Didn't find error message.");
    ok(message.includes("a certificate that is not valid for"), "Didn't find error message.");
    ok(message.includes("badchain.include-subdomains.pinning.example.com"), "Didn't find domain in error message.");

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function checkhideAddExceptionButtonViaPref() {
  info("Loading a bad cert page and verifying the pref security.certerror.hideAddException");
  Services.prefs.setBoolPref("security.certerror.hideAddException", true);

  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(BAD_CERT, useFrame);
    let browser = tab.linkedBrowser;

    await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let doc = frame ? content.document.querySelector("iframe").contentDocument : content.document;

      let exceptionButton = doc.querySelector(".exceptionDialogButtonContainer");
      ok(ContentTaskUtils.is_hidden(exceptionButton), "Exception button is hidden.");
    });

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }

  Services.prefs.clearUserPref("security.certerror.hideAddException");
});

add_task(async function checkhideAddExceptionButtonInFrames() {
  info("Loading a bad cert page in a frame and verifying it's hidden.");
  let tab = await openErrorPage(BAD_CERT, true);
  let browser = tab.linkedBrowser;

  await ContentTask.spawn(browser, null, async function() {
    let doc = content.document.querySelector("iframe").contentDocument;
    let exceptionButton = doc.getElementById("exceptionDialogButton");
    ok(ContentTaskUtils.is_hidden(exceptionButton), "Exception button is hidden.");
  });

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
