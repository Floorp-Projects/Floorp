/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This is testing the aboutCertError page (Bug 1207107).

const GOOD_PAGE = "https://example.com/";
const GOOD_PAGE_2 = "https://example.org/";
const DUMMY_PAGE = getRootDirectory(gTestPath).replace("chrome://mochitests/content", GOOD_PAGE) + "dummy_page.html";
const BAD_CERT = "https://expired.example.com/";
const UNKNOWN_ISSUER = "https://self-signed.example.com ";
const BAD_STS_CERT = "https://badchain.include-subdomains.pinning.example.com:443";
const {TabStateFlusher} = ChromeUtils.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});

function injectErrorPageFrame(tab, src) {
  return ContentTask.spawn(tab.linkedBrowser, {frameSrc: src}, async function({frameSrc}) {
    let loaded = ContentTaskUtils.waitForEvent(content.wrappedJSObject, "DOMFrameContentLoaded");
    let iframe = content.document.createElement("iframe");
    iframe.src = frameSrc;
    content.document.body.appendChild(iframe);
    await loaded;
    // We will have race conditions when accessing the frame content after setting a src,
    // so we can't wait for AboutNetErrorLoad. Let's wait for the certerror class to
    // appear instead (which should happen at the same time as AboutNetErrorLoad).
    await ContentTaskUtils.waitForCondition(() =>
      iframe.contentDocument.body.classList.contains("certerror"));
  });
}

async function openErrorPage(src, useFrame) {
  let tab;
  if (useFrame) {
    info("Loading cert error page in an iframe");
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, DUMMY_PAGE);
    await injectErrorPageFrame(tab, src);
  } else {
    let certErrorLoaded;
    tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, src);
      let browser = gBrowser.selectedBrowser;
      certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
    }, false);
    info("Loading and waiting for the cert error");
    await certErrorLoaded;
  }

  return tab;
}

add_task(async function checkReturnToAboutHome() {
  info("Loading a bad cert page directly and making sure 'return to previous page' goes to about:home");
  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(BAD_CERT, useFrame);
    let browser = tab.linkedBrowser;

    is(browser.webNavigation.canGoBack, false, "!webNavigation.canGoBack");
    is(browser.webNavigation.canGoForward, false, "!webNavigation.canGoForward");

    // Populate the shistory entries manually, since it happens asynchronously
    // and the following tests will be too soon otherwise.
    await TabStateFlusher.flush(browser);
    let {entries} = JSON.parse(SessionStore.getTabState(tab));
    is(entries.length, 1, "there is one shistory entry");

    info("Clicking the go back button on about:certerror");
    await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let doc = frame ? content.document.querySelector("iframe").contentDocument : content.document;

      let returnButton = doc.getElementById("returnButton");
      if (!frame) {
        is(returnButton.getAttribute("autofocus"), "true", "returnButton has autofocus");
      }
      returnButton.click();

      await ContentTaskUtils.waitForEvent(this, "pageshow", true);
    });

    is(browser.webNavigation.canGoBack, true, "webNavigation.canGoBack");
    is(browser.webNavigation.canGoForward, false, "!webNavigation.canGoForward");
    is(gBrowser.currentURI.spec, "about:home", "Went back");

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function checkExceptionDialogButton() {
  Services.prefs.setBoolPref("browser.security.newcerterrorpage.enabled", true);
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
  Services.prefs.clearUserPref("browser.security.newcerterrorpage.enabled");
});

add_task(async function checkReturnToPreviousPage() {
  info("Loading a bad cert page and making sure 'return to previous page' goes back");
  for (let useFrame of [false, true]) {
    let tab;
    let browser;
    if (useFrame) {
      tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, GOOD_PAGE);
      browser = tab.linkedBrowser;

      BrowserTestUtils.loadURI(browser, GOOD_PAGE_2);
      await BrowserTestUtils.browserLoaded(browser, false, GOOD_PAGE_2);
      await injectErrorPageFrame(tab, BAD_CERT);
    } else {
      tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, GOOD_PAGE);
      browser = gBrowser.selectedBrowser;

      info("Loading and waiting for the cert error");
      let certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
      BrowserTestUtils.loadURI(browser, BAD_CERT);
      await certErrorLoaded;
    }

    is(browser.webNavigation.canGoBack, true, "webNavigation.canGoBack");
    is(browser.webNavigation.canGoForward, false, "!webNavigation.canGoForward");

    // Populate the shistory entries manually, since it happens asynchronously
    // and the following tests will be too soon otherwise.
    await TabStateFlusher.flush(browser);
    let {entries} = JSON.parse(SessionStore.getTabState(tab));
    is(entries.length, 2, "there are two shistory entries");

    info("Clicking the go back button on about:certerror");
    await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let doc = frame ? content.document.querySelector("iframe").contentDocument : content.document;
      let returnButton = doc.getElementById("returnButton");
      returnButton.click();

      await ContentTaskUtils.waitForEvent(this, "pageshow", true);
    });

    is(browser.webNavigation.canGoBack, false, "!webNavigation.canGoBack");
    is(browser.webNavigation.canGoForward, true, "webNavigation.canGoForward");
    is(gBrowser.currentURI.spec, GOOD_PAGE, "Went back");

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
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
    if (Services.prefs.getBoolPref("browser.security.newcerterrorpage.enabled", false)) {
      ok(message.includes("SSL_ERROR_BAD_CERT_DOMAIN"), "Didn't find SSL_ERROR_BAD_CERT_DOMAIN.");
      ok(message.includes("The certificate is only valid for"), "Didn't find error message.");
      ok(message.includes("a security certificate that is not valid for"), "Didn't find error message.");
      ok(message.includes("badchain.include-subdomains.pinning.example.com"), "Didn't find domain in error message.");

      BrowserTestUtils.removeTab(gBrowser.selectedTab);
      return;
    }
    ok(message.includes("SSL_ERROR_BAD_CERT_DOMAIN"), "Didn't find SSL_ERROR_BAD_CERT_DOMAIN.");
    ok(message.includes("The certificate is only valid for"), "Didn't find error message.");
    ok(message.includes("uses an invalid security certificate"), "Didn't find error message.");
    ok(message.includes("badchain.include-subdomains.pinning.example.com"), "Didn't find domain in error message.");

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

// This checks that the appinfo.appBuildID starts with a date string,
// which is required for the misconfigured system time check.
add_task(async function checkAppBuildIDIsDate() {
  let appBuildID = Services.appinfo.appBuildID;
  let year = parseInt(appBuildID.substr(0, 4), 10);
  let month = parseInt(appBuildID.substr(4, 2), 10);
  let day = parseInt(appBuildID.substr(6, 2), 10);

  ok(year >= 2016 && year <= 2100, "appBuildID contains a valid year");
  ok(month >= 1 && month <= 12, "appBuildID contains a valid month");
  ok(day >= 1 && day <= 31, "appBuildID contains a valid day");
});

const PREF_SERVICES_SETTINGS_CLOCK_SKEW_SECONDS = "services.settings.clock_skew_seconds";

add_task(async function checkWrongSystemTimeWarning() {
  async function setUpPage() {
    let browser;
    let certErrorLoaded;
    await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, BAD_CERT);
      browser = gBrowser.selectedBrowser;
      certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
    }, false);

    info("Loading and waiting for the cert error");
    await certErrorLoaded;

    return ContentTask.spawn(browser, null, async function() {
      let doc = content.document;
      let div = doc.getElementById("wrongSystemTimePanel");
      let systemDateDiv = doc.getElementById("wrongSystemTime_systemDate");
      let actualDateDiv = doc.getElementById("wrongSystemTime_actualDate");
      let learnMoreLink = doc.getElementById("learnMoreLink");

      return {
        divDisplay: content.getComputedStyle(div).display,
        text: div.textContent,
        systemDate: systemDateDiv.textContent,
        actualDate: actualDateDiv.textContent,
        learnMoreLink: learnMoreLink.href,
      };
    });
  }

  let formatter = new Services.intl.DateTimeFormat(undefined, {
    dateStyle: "short",
  });

  // pretend we have a positively skewed (ahead) system time
  let serverDate = new Date("2015/10/27");
  let serverDateFmt = formatter.format(serverDate);
  let localDateFmt = formatter.format(new Date());

  let skew = Math.floor((Date.now() - serverDate.getTime()) / 1000);
  await SpecialPowers.pushPrefEnv({set: [[PREF_SERVICES_SETTINGS_CLOCK_SKEW_SECONDS, skew]]});

  info("Loading a bad cert page with a skewed clock");
  let message = await setUpPage();

  isnot(message.divDisplay, "none", "Wrong time message information is visible");
  ok(message.text.includes("clock appears to show the wrong time"),
     "Correct error message found");
  ok(message.text.includes("expired.example.com"), "URL found in error message");
  ok(message.systemDate.includes(localDateFmt), "correct local date displayed");
  ok(message.actualDate.includes(serverDateFmt), "correct server date displayed");
  ok(message.learnMoreLink.includes("time-errors"), "time-errors in the Learn More URL");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // pretend we have a negatively skewed (behind) system time
  serverDate = new Date();
  serverDate.setYear(serverDate.getFullYear() + 1);
  serverDateFmt = formatter.format(serverDate);

  skew = Math.floor((Date.now() - serverDate.getTime()) / 1000);
  await SpecialPowers.pushPrefEnv({set: [[PREF_SERVICES_SETTINGS_CLOCK_SKEW_SECONDS, skew]]});

  info("Loading a bad cert page with a skewed clock");
  message = await setUpPage();

  isnot(message.divDisplay, "none", "Wrong time message information is visible");
  ok(message.text.includes("clock appears to show the wrong time"),
     "Correct error message found");
  ok(message.text.includes("expired.example.com"), "URL found in error message");
  ok(message.systemDate.includes(localDateFmt), "correct local date displayed");
  ok(message.actualDate.includes(serverDateFmt), "correct server date displayed");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // pretend we only have a slightly skewed system time, four hours
  skew = 60 * 60 * 4;
  await SpecialPowers.pushPrefEnv({set: [[PREF_SERVICES_SETTINGS_CLOCK_SKEW_SECONDS, skew]]});

  info("Loading a bad cert page with an only slightly skewed clock");
  message = await setUpPage();

  is(message.divDisplay, "none", "Wrong time message information is not visible");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // now pretend we have no skewed system time
  skew = 0;
  await SpecialPowers.pushPrefEnv({set: [[PREF_SERVICES_SETTINGS_CLOCK_SKEW_SECONDS, skew]]});

  info("Loading a bad cert page with no skewed clock");
  message = await setUpPage();

  is(message.divDisplay, "none", "Wrong time message information is not visible");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}).skip(); // Skipping because of bug 1414804.

add_task(async function checkAdvancedDetails() {
  info("Loading a bad cert page and verifying the main error and advanced details section");
  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(BAD_CERT, useFrame);
    let browser = tab.linkedBrowser;

    let message = await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let doc = frame ? content.document.querySelector("iframe").contentDocument : content.document;

      let shortDescText = doc.getElementById("errorShortDescText");
      info("Main error text: " + shortDescText.textContent);
      ok(shortDescText.textContent.includes("expired.example.com"),
         "Should list hostname in error message.");

      let exceptionButton = doc.getElementById("exceptionDialogButton");
      ok(!exceptionButton.disabled, "Exception button is not disabled by default.");

      let advancedButton = doc.getElementById("advancedButton");
      advancedButton.click();
      let el = doc.getElementById("errorCode");
      return { textContent: el.textContent, tagName: el.tagName };
    });
    is(message.textContent, "SEC_ERROR_EXPIRED_CERTIFICATE",
       "Correct error message found");
    is(message.tagName, "a", "Error message is a link");

    message = await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let win = frame ? content.document.querySelector("iframe").contentWindow : content;
      let doc = win.document;

      let errorCode = doc.getElementById("errorCode");
      errorCode.click();
      let div = doc.getElementById("certificateErrorDebugInformation");
      let text = doc.getElementById("certificateErrorText");

      let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                       .getService(Ci.nsISerializationHelper);
      let serializable =  win.docShell.failedChannel.securityInfo
                                      .QueryInterface(Ci.nsITransportSecurityInfo)
                                      .QueryInterface(Ci.nsISerializable);
      let serializedSecurityInfo = serhelper.serializeToString(serializable);
      return {
        divDisplay: content.getComputedStyle(div).display,
        text: text.textContent,
        securityInfoAsString: serializedSecurityInfo,
      };
    });
    isnot(message.divDisplay, "none", "Debug information is visible");
    ok(message.text.includes(BAD_CERT), "Correct URL found");
    ok(message.text.includes("Certificate has expired"),
       "Correct error message found");
    ok(message.text.includes("HTTP Strict Transport Security: false"),
       "Correct HSTS value found");
    ok(message.text.includes("HTTP Public Key Pinning: false"),
       "Correct HPKP value found");
    let certChain = getCertChain(message.securityInfoAsString);
    ok(message.text.includes(certChain), "Found certificate chain");

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

add_task(async function checkAdvancedDetailsForHSTS() {
  info("Loading a bad STS cert page and verifying the advanced details section");
  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(BAD_STS_CERT, useFrame);
    let browser = tab.linkedBrowser;

    let message = await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let doc = frame ? content.document.querySelector("iframe").contentDocument : content.document;

      let advancedButton = doc.getElementById("advancedButton");
      advancedButton.click();
      let ec = doc.getElementById("errorCode");
      let cdl = doc.getElementById("cert_domain_link");
      return {
        ecTextContent: ec.textContent,
        ecTagName: ec.tagName,
        cdlTextContent: cdl.textContent,
        cdlTagName: cdl.tagName,
      };
    });

    const badStsUri = Services.io.newURI(BAD_STS_CERT);
    is(message.ecTextContent, "SSL_ERROR_BAD_CERT_DOMAIN",
       "Correct error message found");
    is(message.ecTagName, "a", "Error message is a link");
    const url = badStsUri.prePath.slice(badStsUri.prePath.indexOf(".") + 1);
    is(message.cdlTextContent, url,
       "Correct cert_domain_link contents found");
    is(message.cdlTagName, "a", "cert_domain_link is a link");

    message = await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let win = frame ? content.document.querySelector("iframe").contentWindow : content;
      let doc = win.document;

      let errorCode = doc.getElementById("errorCode");
      errorCode.click();
      let div = doc.getElementById("certificateErrorDebugInformation");
      let text = doc.getElementById("certificateErrorText");

      let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                       .getService(Ci.nsISerializationHelper);
      let serializable =  win.docShell.failedChannel.securityInfo
                                      .QueryInterface(Ci.nsITransportSecurityInfo)
                                      .QueryInterface(Ci.nsISerializable);
      let serializedSecurityInfo = serhelper.serializeToString(serializable);
      return {
        divDisplay: content.getComputedStyle(div).display,
        text: text.textContent,
        securityInfoAsString: serializedSecurityInfo,
      };
    });
    isnot(message.divDisplay, "none", "Debug information is visible");
    ok(message.text.includes(badStsUri.spec), "Correct URL found");
    ok(message.text.includes("requested domain name does not match the server\u2019s certificate"),
       "Correct error message found");
    ok(message.text.includes("HTTP Strict Transport Security: false"),
       "Correct HSTS value found");
    ok(message.text.includes("HTTP Public Key Pinning: true"),
       "Correct HPKP value found");
    let certChain = getCertChain(message.securityInfoAsString);
    ok(message.text.includes(certChain), "Found certificate chain");

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function checkUnknownIssuerLearnMoreLink() {
  info("Loading a cert error for self-signed pages and checking the correct link is shown");
  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(UNKNOWN_ISSUER, useFrame);
    let browser = tab.linkedBrowser;

    let href = await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let doc = frame ? content.document.querySelector("iframe").contentDocument : content.document;
      let learnMoreLink = doc.getElementById("learnMoreLink");
      return learnMoreLink.href;
    });
    ok(href.endsWith("security-error"), "security-error in the Learn More URL");

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

function getCertChain(securityInfoAsString) {
  let certChain = "";
  const serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                       .getService(Ci.nsISerializationHelper);
  let securityInfo = serhelper.deserializeObject(securityInfoAsString);
  securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
  for (let cert of securityInfo.failedCertChain.getEnumerator()) {
    certChain += getPEMString(cert);
  }
  return certChain;
}

function getDERString(cert) {
  var length = {};
  var derArray = cert.getRawDER(length);
  var derString = "";
  for (var i = 0; i < derArray.length; i++) {
    derString += String.fromCharCode(derArray[i]);
  }
  return derString;
}

function getPEMString(cert) {
  var derb64 = btoa(getDERString(cert));
  // Wrap the Base64 string into lines of 64 characters,
  // with CRLF line breaks (as specified in RFC 1421).
  var wrapped = derb64.replace(/(\S{64}(?!$))/g, "$1\r\n");
  return "-----BEGIN CERTIFICATE-----\r\n"
         + wrapped
         + "\r\n-----END CERTIFICATE-----\r\n";
}
