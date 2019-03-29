/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This is testing the aboutCertError page (Bug 1207107).

const GOOD_PAGE = "https://example.com/";
const GOOD_PAGE_2 = "https://example.org/";
const BAD_CERT = "https://expired.example.com/";
const UNKNOWN_ISSUER = "https://self-signed.example.com ";
const BAD_STS_CERT = "https://badchain.include-subdomains.pinning.example.com:443";
const {TabStateFlusher} = ChromeUtils.import("resource:///modules/sessionstore/TabStateFlusher.jsm");

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
      // Note that going back to about:newtab might cause a process flip, if
      // the browser is configured to run about:newtab in its own special
      // content process.
      returnButton.click();
    });

    await BrowserTestUtils.waitForLocationChange(gBrowser, "about:home");

    is(browser.webNavigation.canGoBack, true, "webNavigation.canGoBack");
    is(browser.webNavigation.canGoForward, false, "!webNavigation.canGoForward");
    is(gBrowser.currentURI.spec, "about:home", "Went back");

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
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

add_task(async function checkCautionClass() {
  info("Checking that are potentially more dangerous get a 'caution' class");
  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(UNKNOWN_ISSUER, useFrame);
    let browser = tab.linkedBrowser;

    await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let doc = frame ? content.document.querySelector("iframe").contentDocument : content.document;
      is(doc.body.classList.contains("caution"), !frame, `Cert error body has ${frame ? "no" : ""} caution class`);
    });

    BrowserTestUtils.removeTab(gBrowser.selectedTab);

    tab = await openErrorPage(BAD_STS_CERT, useFrame);
    browser = tab.linkedBrowser;

    await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let doc = frame ? content.document.querySelector("iframe").contentDocument : content.document;
      ok(!doc.body.classList.contains("caution"), "Cert error body has no caution class");
    });

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function checkViewCertificate() {
  info("Loading a cert error and checking that the certificate can be shown.");
  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(UNKNOWN_ISSUER, useFrame);
    let browser = tab.linkedBrowser;

    let dialogOpened = BrowserTestUtils.domWindowOpened();

    await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let doc = frame ? content.document.querySelector("iframe").contentDocument : content.document;
      let viewCertificate = doc.getElementById("viewCertificate");
      viewCertificate.click();
    });

    let win = await dialogOpened;
    await BrowserTestUtils.waitForEvent(win, "load");
    is(win.document.documentURI, "chrome://pippki/content/certViewer.xul",
      "Opened the cert viewer dialog");
    is(win.document.getElementById("commonname").value, "self-signed.example.com",
      "Shows the correct certificate in the dialog");
    win.close();

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function checkBadStsCertHeadline() {
  info("Loading a bad sts cert error page and checking that the correct headline is shown");
  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(BAD_CERT, useFrame);
    let browser = tab.linkedBrowser;

    let titleContent = await ContentTask.spawn(browser, {frame: useFrame}, async function({frame}) {
      let doc = frame ? content.document.querySelector("iframe").contentDocument : content.document;
      let titleText = doc.querySelector(".title-text");
      return titleText.textContent;
    });
    if (useFrame) {
      ok(titleContent.endsWith("Security Issue"), "Did Not Connect: Potential Security Issue");
    } else {
      ok(titleContent.endsWith("Risk Ahead"), "Warning: Potential Security Risk Ahead");
    }
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});
