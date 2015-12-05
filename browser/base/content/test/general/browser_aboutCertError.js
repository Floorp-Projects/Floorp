/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This is testing the aboutCertError page (Bug 1207107).

const GOOD_PAGE = "https://example.com/";
const BAD_CERT = "https://expired.example.com/";
const BAD_STS_CERT = "https://badchain.include-subdomains.pinning.example.com:443";
const {TabStateFlusher} = Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});
const ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

add_task(function* checkReturnToAboutHome() {
  info("Loading a bad cert page directly and making sure 'return to previous page' goes to about:home");
  let browser;
  let certErrorLoaded;
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = gBrowser.addTab(BAD_CERT);
    browser = gBrowser.selectedBrowser;
    certErrorLoaded = waitForCertErrorLoad(browser);
  }, false);

  info("Loading and waiting for the cert error");
  yield certErrorLoaded;

  is(browser.webNavigation.canGoBack, false, "!webNavigation.canGoBack");
  is(browser.webNavigation.canGoForward, false, "!webNavigation.canGoForward");

  // Populate the shistory entries manually, since it happens asynchronously
  // and the following tests will be too soon otherwise.
  yield TabStateFlusher.flush(browser);
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 1, "there is one shistory entry");

  info("Clicking the go back button on about:certerror");
  let pageshowPromise = promiseWaitForEvent(browser, "pageshow");
  yield ContentTask.spawn(browser, null, function* () {
    let doc = content.document;
    let returnButton = doc.getElementById("returnButton");
    returnButton.click();
  });
  yield pageshowPromise;

  is(browser.webNavigation.canGoBack, true, "webNavigation.canGoBack");
  is(browser.webNavigation.canGoForward, false, "!webNavigation.canGoForward");
  is(gBrowser.currentURI.spec, "about:home", "Went back");

  gBrowser.removeCurrentTab();
});

add_task(function* checkReturnToPreviousPage() {
  info("Loading a bad cert page and making sure 'return to previous page' goes back");
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, GOOD_PAGE);
  let browser = gBrowser.selectedBrowser;

  info("Loading and waiting for the cert error");
  let certErrorLoaded = waitForCertErrorLoad(browser);
  BrowserTestUtils.loadURI(browser, BAD_CERT);
  yield certErrorLoaded;

  is(browser.webNavigation.canGoBack, true, "webNavigation.canGoBack");
  is(browser.webNavigation.canGoForward, false, "!webNavigation.canGoForward");

  // Populate the shistory entries manually, since it happens asynchronously
  // and the following tests will be too soon otherwise.
  yield TabStateFlusher.flush(browser);
  let {entries} = JSON.parse(ss.getTabState(tab));
  is(entries.length, 2, "there are two shistory entries");

  info("Clicking the go back button on about:certerror");
  let pageshowPromise = promiseWaitForEvent(browser, "pageshow");
  yield ContentTask.spawn(browser, null, function* () {
    let doc = content.document;
    let returnButton = doc.getElementById("returnButton");
    returnButton.click();
  });
  yield pageshowPromise;

  is(browser.webNavigation.canGoBack, false, "!webNavigation.canGoBack");
  is(browser.webNavigation.canGoForward, true, "webNavigation.canGoForward");
  is(gBrowser.currentURI.spec, GOOD_PAGE, "Went back");

  gBrowser.removeCurrentTab();
});

add_task(function* checkBadStsCert() {
  info("Loading a badStsCert and making sure exception button doesn't show up");
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, GOOD_PAGE);
  let browser = gBrowser.selectedBrowser;

  info("Loading and waiting for the cert error");
  let certErrorLoaded = waitForCertErrorLoad(browser);
  BrowserTestUtils.loadURI(browser, BAD_STS_CERT);
  yield certErrorLoaded;

  let exceptionButtonHidden = yield ContentTask.spawn(browser, null, function* () {
    let doc = content.document;
    let exceptionButton = doc.getElementById("exceptionDialogButton");
    return exceptionButton.hidden;
  });
  ok(exceptionButtonHidden, "Exception button is hidden");

  gBrowser.removeCurrentTab();
});

add_task(function* checkAdvancedDetails() {
  info("Loading a bad cert page and verifying the advanced details section");
  let browser;
  let certErrorLoaded;
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = gBrowser.addTab(BAD_CERT);
    browser = gBrowser.selectedBrowser;
    certErrorLoaded = waitForCertErrorLoad(browser);
  }, false);

  info("Loading and waiting for the cert error");
  yield certErrorLoaded;

  let message = yield ContentTask.spawn(browser, null, function* () {
    let doc = content.document;
    let advancedButton = doc.getElementById("advancedButton");
    advancedButton.click();
    let el = doc.getElementById("errorCode");
    return { textContent: el.textContent, tagName: el.tagName };
  });
  is(message.textContent, "SEC_ERROR_EXPIRED_CERTIFICATE",
     "Correct error message found");
  is(message.tagName, "a", "Error message is a link");

  message = yield ContentTask.spawn(browser, null, function* () {
    let doc = content.document;
    let errorCode = doc.getElementById("errorCode");
    errorCode.click();
    let div = doc.getElementById("certificateErrorDebugInformation");
    let text = doc.getElementById("certificateErrorText");

    let docshell = content.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);
    let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                     .getService(Ci.nsISerializationHelper);
    let serializable =  docShell.failedChannel.securityInfo
                                .QueryInterface(Ci.nsITransportSecurityInfo)
                                .QueryInterface(Ci.nsISerializable);
    let serializedSecurityInfo = serhelper.serializeToString(serializable);
    return {
      divDisplay: div.style.display,
      text: text.textContent,
      securityInfoAsString: serializedSecurityInfo
    };
  });
  is(message.divDisplay, "block", "Debug information is visible");
  ok(message.text.contains(BAD_CERT), "Correct URL found");
  ok(message.text.contains("Certificate has expired"),
     "Correct error message found");
  ok(message.text.contains("HTTP Strict Transport Security: false"),
     "Correct HSTS value found");
  ok(message.text.contains("HTTP Public Key Pinning: false"),
     "Correct HPKP value found");
  let certChain = getCertChain(message.securityInfoAsString);
  ok(message.text.contains(certChain), "Found certificate chain");

  gBrowser.removeCurrentTab();
});

add_task(function* checkAdvancedDetailsForHSTS() {
  info("Loading a bad STS cert page and verifying the advanced details section");
  let browser;
  let certErrorLoaded;
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, () => {
    gBrowser.selectedTab = gBrowser.addTab(BAD_STS_CERT);
    browser = gBrowser.selectedBrowser;
    certErrorLoaded = waitForCertErrorLoad(browser);
  }, false);

  info("Loading and waiting for the cert error");
  yield certErrorLoaded;

  let message = yield ContentTask.spawn(browser, null, function* () {
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

  const badStsUri = Services.io.newURI(BAD_STS_CERT, null, null);
  is(message.ecTextContent, "SSL_ERROR_BAD_CERT_DOMAIN",
     "Correct error message found");
  is(message.ecTagName, "a", "Error message is a link");
  const url = badStsUri.prePath.slice(badStsUri.prePath.indexOf(".") + 1);
  is(message.cdlTextContent, url,
     "Correct cert_domain_link contents found");
  is(message.cdlTagName, "a", "cert_domain_link is a link");

  message = yield ContentTask.spawn(browser, null, function* () {
    let doc = content.document;
    let errorCode = doc.getElementById("errorCode");
    errorCode.click();
    let div = doc.getElementById("certificateErrorDebugInformation");
    let text = doc.getElementById("certificateErrorText");

    let docshell = content.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIWebNavigation)
                          .QueryInterface(Ci.nsIDocShell);
    let serhelper = Cc["@mozilla.org/network/serialization-helper;1"]
                     .getService(Ci.nsISerializationHelper);
    let serializable =  docShell.failedChannel.securityInfo
                                .QueryInterface(Ci.nsITransportSecurityInfo)
                                .QueryInterface(Ci.nsISerializable);
    let serializedSecurityInfo = serhelper.serializeToString(serializable);
    return {
      divDisplay: div.style.display,
      text: text.textContent,
      securityInfoAsString: serializedSecurityInfo
    };
  });
  is(message.divDisplay, "block", "Debug information is visible");
  ok(message.text.contains(badStsUri.spec), "Correct URL found");
  ok(message.text.contains("requested domain name does not match the server's certificate"),
     "Correct error message found");
  ok(message.text.contains("HTTP Strict Transport Security: false"),
     "Correct HSTS value found");
  ok(message.text.contains("HTTP Public Key Pinning: true"),
     "Correct HPKP value found");
  let certChain = getCertChain(message.securityInfoAsString);
  ok(message.text.contains(certChain), "Found certificate chain");

  gBrowser.removeCurrentTab();
});

function waitForCertErrorLoad(browser) {
  return new Promise(resolve => {
    info("Waiting for DOMContentLoaded event");
    browser.addEventListener("DOMContentLoaded", function load() {
      browser.removeEventListener("DOMContentLoaded", load, false, true);
      resolve();
    }, false, true);
  });
}

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

function getDERString(cert)
{
  var length = {};
  var derArray = cert.getRawDER(length);
  var derString = '';
  for (var i = 0; i < derArray.length; i++) {
    derString += String.fromCharCode(derArray[i]);
  }
  return derString;
}

function getPEMString(cert)
{
  var derb64 = btoa(getDERString(cert));
  // Wrap the Base64 string into lines of 64 characters,
  // with CRLF line breaks (as specified in RFC 1421).
  var wrapped = derb64.replace(/(\S{64}(?!$))/g, "$1\r\n");
  return "-----BEGIN CERTIFICATE-----\r\n"
         + wrapped
         + "\r\n-----END CERTIFICATE-----\r\n";
}
