/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This is testing the aboutCertError page (Bug 1207107).

const GOOD_PAGE = "https://example.com/";
const BAD_CERT = "https://expired.example.com/";
const UNKNOWN_ISSUER = "https://self-signed.example.com ";
const BAD_STS_CERT = "https://badchain.include-subdomains.pinning.example.com:443";
const {TabStateFlusher} = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});
const ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

add_task(async function checkReturnToAboutHome() {
  info("Loading a bad cert page directly and making sure 'return to previous page' goes to about:home");
  let browser;
  let certErrorLoaded;
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, BAD_CERT);
    browser = gBrowser.selectedBrowser;
    certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
  }, false);

  info("Loading and waiting for the cert error");
  await certErrorLoaded;

  is(browser.webNavigation.canGoBack, false, "!webNavigation.canGoBack");
  is(browser.webNavigation.canGoForward, false, "!webNavigation.canGoForward");

  // Populate the shistory entries manually, since it happens asynchronously
  // and the following tests will be too soon otherwise.
  await TabStateFlusher.flush(browser);
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 1, "there is one shistory entry");

  info("Clicking the go back button on about:certerror");
  await ContentTask.spawn(browser, null, async function() {
    let doc = content.document;
    let returnButton = doc.getElementById("returnButton");
    is(returnButton.getAttribute("autofocus"), "true", "returnButton has autofocus");
    returnButton.click();

    await ContentTaskUtils.waitForEvent(this, "pageshow", true);
  });

  is(browser.webNavigation.canGoBack, true, "webNavigation.canGoBack");
  is(browser.webNavigation.canGoForward, false, "!webNavigation.canGoForward");
  is(gBrowser.currentURI.spec, "about:home", "Went back");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function checkReturnToPreviousPage() {
  info("Loading a bad cert page and making sure 'return to previous page' goes back");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, GOOD_PAGE);
  let browser = gBrowser.selectedBrowser;

  info("Loading and waiting for the cert error");
  let certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
  BrowserTestUtils.loadURI(browser, BAD_CERT);
  await certErrorLoaded;

  is(browser.webNavigation.canGoBack, true, "webNavigation.canGoBack");
  is(browser.webNavigation.canGoForward, false, "!webNavigation.canGoForward");

  // Populate the shistory entries manually, since it happens asynchronously
  // and the following tests will be too soon otherwise.
  await TabStateFlusher.flush(browser);
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 2, "there are two shistory entries");

  info("Clicking the go back button on about:certerror");
  await ContentTask.spawn(browser, null, async function() {
    let doc = content.document;
    let returnButton = doc.getElementById("returnButton");
    returnButton.click();

    await ContentTaskUtils.waitForEvent(this, "pageshow", true);
  });

  is(browser.webNavigation.canGoBack, false, "!webNavigation.canGoBack");
  is(browser.webNavigation.canGoForward, true, "webNavigation.canGoForward");
  is(gBrowser.currentURI.spec, GOOD_PAGE, "Went back");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function checkBadStsCert() {
  info("Loading a badStsCert and making sure exception button doesn't show up");
  await BrowserTestUtils.openNewForegroundTab(gBrowser, GOOD_PAGE);
  let browser = gBrowser.selectedBrowser;

  info("Loading and waiting for the cert error");
  let certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
  BrowserTestUtils.loadURI(browser, BAD_STS_CERT);
  await certErrorLoaded;

  let exceptionButtonHidden = await ContentTask.spawn(browser, null, async function() {
    let doc = content.document;
    let exceptionButton = doc.getElementById("exceptionDialogButton");
    return exceptionButton.hidden;
  });
  ok(exceptionButtonHidden, "Exception button is hidden");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
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

const PREF_BLOCKLIST_CLOCK_SKEW_SECONDS = "services.blocklist.clock_skew_seconds";

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
        learnMoreLink: learnMoreLink.href
      };
    });
  }

  let formatter = Services.intl.createDateTimeFormat(undefined, {
    dateStyle: "short"
  });

  // pretend we have a positively skewed (ahead) system time
  let serverDate = new Date("2015/10/27");
  let serverDateFmt = formatter.format(serverDate);
  let localDateFmt = formatter.format(new Date());

  let skew = Math.floor((Date.now() - serverDate.getTime()) / 1000);
  await SpecialPowers.pushPrefEnv({set: [[PREF_BLOCKLIST_CLOCK_SKEW_SECONDS, skew]]});

  info("Loading a bad cert page with a skewed clock");
  let message = await setUpPage();

  isnot(message.divDisplay, "none", "Wrong time message information is visible");
  ok(message.text.includes("clock appears to show the wrong time"),
     "Correct error message found");
  ok(message.text.includes("expired.example.com"), "URL found in error message");
  ok(message.systemDate.includes(localDateFmt), "correct local date displayed");
  ok(message.actualDate.includes(serverDateFmt), "correct server date displayed");
  ok(message.learnMoreLink.includes("time-errors"), "time-errors in the Learn More URL");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // pretend we have a negatively skewed (behind) system time
  serverDate = new Date();
  serverDate.setYear(serverDate.getFullYear() + 1);
  serverDateFmt = formatter.format(serverDate);

  skew = Math.floor((Date.now() - serverDate.getTime()) / 1000);
  await SpecialPowers.pushPrefEnv({set: [[PREF_BLOCKLIST_CLOCK_SKEW_SECONDS, skew]]});

  info("Loading a bad cert page with a skewed clock");
  message = await setUpPage();

  isnot(message.divDisplay, "none", "Wrong time message information is visible");
  ok(message.text.includes("clock appears to show the wrong time"),
     "Correct error message found");
  ok(message.text.includes("expired.example.com"), "URL found in error message");
  ok(message.systemDate.includes(localDateFmt), "correct local date displayed");
  ok(message.actualDate.includes(serverDateFmt), "correct server date displayed");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // pretend we only have a slightly skewed system time, four hours
  skew = 60 * 60 * 4;
  await SpecialPowers.pushPrefEnv({set: [[PREF_BLOCKLIST_CLOCK_SKEW_SECONDS, skew]]});

  info("Loading a bad cert page with an only slightly skewed clock");
  message = await setUpPage();

  is(message.divDisplay, "none", "Wrong time message information is not visible");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);

  // now pretend we have no skewed system time
  skew = 0;
  await SpecialPowers.pushPrefEnv({set: [[PREF_BLOCKLIST_CLOCK_SKEW_SECONDS, skew]]});

  info("Loading a bad cert page with no skewed clock");
  message = await setUpPage();

  is(message.divDisplay, "none", "Wrong time message information is not visible");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function checkAdvancedDetails() {
  info("Loading a bad cert page and verifying the main error and advanced details section");
  let browser;
  let certErrorLoaded;
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, BAD_CERT);
    browser = gBrowser.selectedBrowser;
    certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
  }, false);

  info("Loading and waiting for the cert error");
  await certErrorLoaded;

  let message = await ContentTask.spawn(browser, null, async function() {
    let doc = content.document;
    let shortDescText = doc.getElementById("errorShortDescText");
    info("Main error text: " + shortDescText.textContent);
    ok(shortDescText.textContent.includes("expired.example.com"),
       "Should list hostname in error message.");

    let advancedButton = doc.getElementById("advancedButton");
    advancedButton.click();
    let el = doc.getElementById("errorCode");
    return { textContent: el.textContent, tagName: el.tagName };
  });
  is(message.textContent, "SEC_ERROR_EXPIRED_CERTIFICATE",
     "Correct error message found");
  is(message.tagName, "a", "Error message is a link");

  message = await ContentTask.spawn(browser, null, async function() {
    let doc = content.document;
    let errorCode = doc.getElementById("errorCode");
    errorCode.click();
    let div = doc.getElementById("certificateErrorDebugInformation");
    let text = doc.getElementById("certificateErrorText");

    let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                     .getService(Ci.nsISerializationHelper);
    let serializable =  docShell.failedChannel.securityInfo
                                .QueryInterface(Ci.nsITransportSecurityInfo)
                                .QueryInterface(Ci.nsISerializable);
    let serializedSecurityInfo = serhelper.serializeToString(serializable);
    return {
      divDisplay: content.getComputedStyle(div).display,
      text: text.textContent,
      securityInfoAsString: serializedSecurityInfo
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

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function checkAdvancedDetailsForHSTS() {
  info("Loading a bad STS cert page and verifying the advanced details section");
  let browser;
  let certErrorLoaded;
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, BAD_STS_CERT);
    browser = gBrowser.selectedBrowser;
    certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
  }, false);

  info("Loading and waiting for the cert error");
  await certErrorLoaded;

  let message = await ContentTask.spawn(browser, null, async function() {
    let doc = content.document;
    let advancedButton = doc.getElementById("advancedButton");
    advancedButton.click();
    let ec = doc.getElementById("errorCode");
    let cdl = doc.getElementById("cert_domain_link");
    return {
      ecTextContent: ec.textContent,
      ecTagName: ec.tagName,
      cdlTextContent: cdl.textContent,
      cdlTagName: cdl.tagName
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

  message = await ContentTask.spawn(browser, null, async function() {
    let doc = content.document;
    let errorCode = doc.getElementById("errorCode");
    errorCode.click();
    let div = doc.getElementById("certificateErrorDebugInformation");
    let text = doc.getElementById("certificateErrorText");

    let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                     .getService(Ci.nsISerializationHelper);
    let serializable =  docShell.failedChannel.securityInfo
                                .QueryInterface(Ci.nsITransportSecurityInfo)
                                .QueryInterface(Ci.nsISerializable);
    let serializedSecurityInfo = serhelper.serializeToString(serializable);
    return {
      divDisplay: content.getComputedStyle(div).display,
      text: text.textContent,
      securityInfoAsString: serializedSecurityInfo
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

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function checkUnknownIssuerLearnMoreLink() {
  info("Loading a cert error for self-signed pages and checking the correct link is shown");
  let browser;
  let certErrorLoaded;
  await BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, UNKNOWN_ISSUER);
    browser = gBrowser.selectedBrowser;
    certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
  }, false);

  info("Loading and waiting for the cert error");
  await certErrorLoaded;

  let href = await ContentTask.spawn(browser, null, async function() {
    let learnMoreLink = content.document.getElementById("learnMoreLink");
    return learnMoreLink.href;
  });
  ok(href.endsWith("security-error"), "security-error in the Learn More URL");

  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

function getCertChain(securityInfoAsString) {
  let certChain = "";
  const serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                       .getService(Ci.nsISerializationHelper);
  let securityInfo = serhelper.deserializeObject(securityInfoAsString);
  securityInfo.QueryInterface(Ci.nsITransportSecurityInfo);
  let certs = securityInfo.failedCertChain.getEnumerator();
  while (certs.hasMoreElements()) {
    let cert = certs.getNext();
    cert.QueryInterface(Ci.nsIX509Cert);
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
