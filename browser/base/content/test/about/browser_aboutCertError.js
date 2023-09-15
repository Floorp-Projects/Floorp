/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This is testing the aboutCertError page (Bug 1207107).

const GOOD_PAGE = "https://example.com/";
const GOOD_PAGE_2 = "https://example.org/";
const BAD_CERT = "https://expired.example.com/";
const UNKNOWN_ISSUER = "https://self-signed.example.com ";
const BAD_STS_CERT =
  "https://badchain.include-subdomains.pinning.example.com:443";
const { TabStateFlusher } = ChromeUtils.importESModule(
  "resource:///modules/sessionstore/TabStateFlusher.sys.mjs"
);

add_task(async function checkReturnToAboutHome() {
  info(
    "Loading a bad cert page directly and making sure 'return to previous page' goes to about:home"
  );
  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(BAD_CERT, useFrame);
    let browser = tab.linkedBrowser;

    is(browser.webNavigation.canGoBack, false, "!webNavigation.canGoBack");
    is(
      browser.webNavigation.canGoForward,
      false,
      "!webNavigation.canGoForward"
    );

    // Populate the shistory entries manually, since it happens asynchronously
    // and the following tests will be too soon otherwise.
    await TabStateFlusher.flush(browser);
    let { entries } = JSON.parse(SessionStore.getTabState(tab));
    is(entries.length, 1, "there is one shistory entry");

    info("Clicking the go back button on about:certerror");
    let bc = browser.browsingContext;
    if (useFrame) {
      bc = bc.children[0];
    }
    let locationChangePromise = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      "about:home"
    );
    await SpecialPowers.spawn(bc, [useFrame], async function (subFrame) {
      let returnButton = content.document.getElementById("returnButton");
      if (!subFrame) {
        if (!Services.focus.focusedElement == returnButton) {
          await ContentTaskUtils.waitForEvent(returnButton, "focus");
        }
        Assert.ok(true, "returnButton has focus");
      }
      // Note that going back to about:newtab might cause a process flip, if
      // the browser is configured to run about:newtab in its own special
      // content process.
      returnButton.click();
    });

    await locationChangePromise;

    is(browser.webNavigation.canGoBack, true, "webNavigation.canGoBack");
    is(
      browser.webNavigation.canGoForward,
      false,
      "!webNavigation.canGoForward"
    );
    is(gBrowser.currentURI.spec, "about:home", "Went back");

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function checkReturnToPreviousPage() {
  info(
    "Loading a bad cert page and making sure 'return to previous page' goes back"
  );
  for (let useFrame of [false, true]) {
    let tab;
    let browser;
    if (useFrame) {
      tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, GOOD_PAGE);
      browser = tab.linkedBrowser;

      BrowserTestUtils.startLoadingURIString(browser, GOOD_PAGE_2);
      await BrowserTestUtils.browserLoaded(browser, false, GOOD_PAGE_2);
      await injectErrorPageFrame(tab, BAD_CERT);
    } else {
      tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, GOOD_PAGE);
      browser = gBrowser.selectedBrowser;

      info("Loading and waiting for the cert error");
      let certErrorLoaded = BrowserTestUtils.waitForErrorPage(browser);
      BrowserTestUtils.startLoadingURIString(browser, BAD_CERT);
      await certErrorLoaded;
    }

    is(browser.webNavigation.canGoBack, true, "webNavigation.canGoBack");
    is(
      browser.webNavigation.canGoForward,
      false,
      "!webNavigation.canGoForward"
    );

    // Populate the shistory entries manually, since it happens asynchronously
    // and the following tests will be too soon otherwise.
    await TabStateFlusher.flush(browser);
    let { entries } = JSON.parse(SessionStore.getTabState(tab));
    is(entries.length, 2, "there are two shistory entries");

    info("Clicking the go back button on about:certerror");
    let bc = browser.browsingContext;
    if (useFrame) {
      bc = bc.children[0];
    }

    let pageShownPromise = BrowserTestUtils.waitForContentEvent(
      browser,
      "pageshow",
      true
    );
    await SpecialPowers.spawn(bc, [useFrame], async function (subFrame) {
      let returnButton = content.document.getElementById("returnButton");
      returnButton.click();
    });
    await pageShownPromise;

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
  info(
    "Loading a bad cert page and verifying the main error and advanced details section"
  );
  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(BAD_CERT, useFrame);
    let browser = tab.linkedBrowser;

    let bc = browser.browsingContext;
    if (useFrame) {
      bc = bc.children[0];
    }

    let message = await SpecialPowers.spawn(bc, [], async function () {
      let doc = content.document;

      const shortDesc = doc.getElementById("errorShortDesc");
      const sdArgs = JSON.parse(shortDesc.dataset.l10nArgs);
      is(
        sdArgs.hostname,
        "expired.example.com",
        "Should list hostname in error message."
      );

      Assert.ok(
        doc.getElementById("certificateErrorDebugInformation").hidden,
        "Debug info is initially hidden"
      );

      let exceptionButton = doc.getElementById("exceptionDialogButton");
      Assert.ok(
        !exceptionButton.disabled,
        "Exception button is not disabled by default."
      );

      let advancedButton = doc.getElementById("advancedButton");
      advancedButton.click();

      // Wait until fluent sets the errorCode inner text.
      let errorCode;
      await ContentTaskUtils.waitForCondition(() => {
        errorCode = doc.getElementById("errorCode");
        return errorCode && errorCode.textContent != "";
      }, "error code has been set inside the advanced button panel");

      return { textContent: errorCode.textContent, tagName: errorCode.tagName };
    });
    is(
      message.textContent,
      "SEC_ERROR_EXPIRED_CERTIFICATE",
      "Correct error message found"
    );
    is(message.tagName, "a", "Error message is a link");

    message = await SpecialPowers.spawn(bc, [], async function () {
      let doc = content.document;
      let errorCode = doc.getElementById("errorCode");
      errorCode.click();
      let div = doc.getElementById("certificateErrorDebugInformation");
      let text = doc.getElementById("certificateErrorText");
      Assert.ok(
        content.getComputedStyle(div).display !== "none",
        "Debug information is visible"
      );
      let failedCertChain =
        content.docShell.failedChannel.securityInfo.failedCertChain.map(cert =>
          cert.getBase64DERString()
        );
      return {
        divDisplay: content.getComputedStyle(div).display,
        text: text.textContent,
        failedCertChain,
      };
    });
    isnot(message.divDisplay, "none", "Debug information is visible");
    ok(message.text.includes(BAD_CERT), "Correct URL found");
    ok(
      message.text.includes("Certificate has expired"),
      "Correct error message found"
    );
    ok(
      message.text.includes("HTTP Strict Transport Security: false"),
      "Correct HSTS value found"
    );
    ok(
      message.text.includes("HTTP Public Key Pinning: false"),
      "Correct HPKP value found"
    );
    let certChain = getCertChainAsString(message.failedCertChain);
    ok(message.text.includes(certChain), "Found certificate chain");

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function checkAdvancedDetailsForHSTS() {
  info(
    "Loading a bad STS cert page and verifying the advanced details section"
  );
  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(BAD_STS_CERT, useFrame);
    let browser = tab.linkedBrowser;

    let bc = browser.browsingContext;
    if (useFrame) {
      bc = bc.children[0];
    }

    let message = await SpecialPowers.spawn(bc, [], async function () {
      let doc = content.document;
      let advancedButton = doc.getElementById("advancedButton");
      advancedButton.click();

      // Wait until fluent sets the errorCode inner text.
      let ec;
      await ContentTaskUtils.waitForCondition(() => {
        ec = doc.getElementById("errorCode");
        return ec.textContent != "";
      }, "error code has been set inside the advanced button panel");

      let cdl = doc.getElementById("cert_domain_link");
      return {
        ecTextContent: ec.textContent,
        ecTagName: ec.tagName,
        cdlTextContent: cdl.textContent,
        cdlTagName: cdl.tagName,
      };
    });

    const badStsUri = Services.io.newURI(BAD_STS_CERT);
    is(
      message.ecTextContent,
      "SSL_ERROR_BAD_CERT_DOMAIN",
      "Correct error message found"
    );
    is(message.ecTagName, "a", "Error message is a link");
    const url = badStsUri.prePath.slice(badStsUri.prePath.indexOf(".") + 1);
    is(message.cdlTextContent, url, "Correct cert_domain_link contents found");
    is(message.cdlTagName, "a", "cert_domain_link is a link");

    message = await SpecialPowers.spawn(bc, [], async function () {
      let doc = content.document;
      let errorCode = doc.getElementById("errorCode");
      errorCode.click();
      let div = doc.getElementById("certificateErrorDebugInformation");
      let text = doc.getElementById("certificateErrorText");
      let failedCertChain =
        content.docShell.failedChannel.securityInfo.failedCertChain.map(cert =>
          cert.getBase64DERString()
        );
      return {
        divDisplay: content.getComputedStyle(div).display,
        text: text.textContent,
        failedCertChain,
      };
    });
    isnot(message.divDisplay, "none", "Debug information is visible");
    ok(message.text.includes(badStsUri.spec), "Correct URL found");
    ok(
      message.text.includes(
        "requested domain name does not match the server\u2019s certificate"
      ),
      "Correct error message found"
    );
    ok(
      message.text.includes("HTTP Strict Transport Security: false"),
      "Correct HSTS value found"
    );
    ok(
      message.text.includes("HTTP Public Key Pinning: true"),
      "Correct HPKP value found"
    );
    let certChain = getCertChainAsString(message.failedCertChain);
    ok(message.text.includes(certChain), "Found certificate chain");

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function checkUnknownIssuerLearnMoreLink() {
  info(
    "Loading a cert error for self-signed pages and checking the correct link is shown"
  );
  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(UNKNOWN_ISSUER, useFrame);
    let browser = tab.linkedBrowser;

    let bc = browser.browsingContext;
    if (useFrame) {
      bc = bc.children[0];
    }

    let href = await SpecialPowers.spawn(bc, [], async function () {
      let learnMoreLink = content.document.getElementById("learnMoreLink");
      return learnMoreLink.href;
    });
    ok(href.endsWith("security-error"), "security-error in the Learn More URL");

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function checkViewCertificate() {
  info("Loading a cert error and checking that the certificate can be shown.");
  for (let useFrame of [true, false]) {
    if (useFrame) {
      // Bug #1573502
      continue;
    }
    let tab = await openErrorPage(UNKNOWN_ISSUER, useFrame);
    let browser = tab.linkedBrowser;

    let bc = browser.browsingContext;
    if (useFrame) {
      bc = bc.children[0];
    }

    let loaded = BrowserTestUtils.waitForNewTab(gBrowser, null, true);
    await SpecialPowers.spawn(bc, [], async function () {
      let viewCertificate = content.document.getElementById("viewCertificate");
      viewCertificate.click();
    });
    await loaded;

    let spec = gBrowser.selectedTab.linkedBrowser.documentURI.spec;
    Assert.ok(
      spec.startsWith("about:certificate"),
      "about:certificate is the new opened tab"
    );

    await SpecialPowers.spawn(
      gBrowser.selectedTab.linkedBrowser,
      [],
      async function () {
        let doc = content.document;
        let certificateSection = await ContentTaskUtils.waitForCondition(() => {
          return doc.querySelector("certificate-section");
        }, "Certificate section found");

        let infoGroup =
          certificateSection.shadowRoot.querySelector("info-group");
        Assert.ok(infoGroup, "infoGroup found");

        let items = infoGroup.shadowRoot.querySelectorAll("info-item");
        let commonnameID = items[items.length - 1].shadowRoot
          .querySelector("label")
          .getAttribute("data-l10n-id");
        Assert.equal(
          commonnameID,
          "certificate-viewer-common-name",
          "The correct item was selected"
        );

        let commonnameValue =
          items[items.length - 1].shadowRoot.querySelector(".info").textContent;
        Assert.equal(
          commonnameValue,
          "self-signed.example.com",
          "Shows the correct certificate in the page"
        );
      }
    );
    BrowserTestUtils.removeTab(gBrowser.selectedTab); // closes about:certificate
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function checkBadStsCertHeadline() {
  info(
    "Loading a bad sts cert error page and checking that the correct headline is shown"
  );
  for (let useFrame of [false, true]) {
    let tab = await openErrorPage(BAD_CERT, useFrame);
    let browser = tab.linkedBrowser;

    let bc = browser.browsingContext;
    if (useFrame) {
      bc = bc.children[0];
    }

    await SpecialPowers.spawn(bc, [useFrame], async _useFrame => {
      const titleText = content.document.querySelector(".title-text");
      is(
        titleText.dataset.l10nId,
        _useFrame ? "nssBadCert-sts-title" : "nssBadCert-title",
        "Error page title is set"
      );
    });
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function checkSandboxedIframe() {
  info(
    "Loading a bad sts cert error in a sandboxed iframe and check that the correct headline is shown"
  );
  let useFrame = true;
  let sandboxed = true;
  let tab = await openErrorPage(BAD_CERT, useFrame, sandboxed);
  let browser = tab.linkedBrowser;

  let bc = browser.browsingContext.children[0];
  await SpecialPowers.spawn(bc, [], async function () {
    let doc = content.document;

    const titleText = doc.querySelector(".title-text");
    is(
      titleText.dataset.l10nId,
      "nssBadCert-sts-title",
      "Title shows Did Not Connect: Potential Security Issue"
    );

    const errorLabel = doc.querySelector(
      '[data-l10n-id="cert-error-code-prefix-link"]'
    );
    const elArgs = JSON.parse(errorLabel.dataset.l10nArgs);
    is(
      elArgs.error,
      "SEC_ERROR_EXPIRED_CERTIFICATE",
      "Correct error message found"
    );
    is(
      doc.getElementById("errorCode").tagName,
      "a",
      "Error message contains a link"
    );
  });
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function checkViewSource() {
  info(
    "Loading a bad sts cert error in a sandboxed iframe and check that the correct headline is shown"
  );
  let uri = "view-source:" + BAD_CERT;
  let tab = await openErrorPage(uri);
  let browser = tab.linkedBrowser;

  await SpecialPowers.spawn(browser, [], async function () {
    let doc = content.document;

    const errorLabel = doc.querySelector(
      '[data-l10n-id="cert-error-code-prefix-link"]'
    );
    const elArgs = JSON.parse(errorLabel.dataset.l10nArgs);
    is(
      elArgs.error,
      "SEC_ERROR_EXPIRED_CERTIFICATE",
      "Correct error message found"
    );
    is(
      doc.getElementById("errorCode").tagName,
      "a",
      "Error message contains a link"
    );

    const titleText = doc.querySelector(".title-text");
    is(titleText.dataset.l10nId, "nssBadCert-title", "Error page title is set");

    const shortDesc = doc.getElementById("errorShortDesc");
    const sdArgs = JSON.parse(shortDesc.dataset.l10nArgs);
    is(
      sdArgs.hostname,
      "expired.example.com",
      "Should list hostname in error message."
    );
  });

  let loaded = BrowserTestUtils.browserLoaded(browser, false, uri);
  info("Clicking the exceptionDialogButton in advanced panel");
  await SpecialPowers.spawn(browser, [], async function () {
    let doc = content.document;
    let exceptionButton = doc.getElementById("exceptionDialogButton");
    exceptionButton.click();
  });

  info("Loading the url after adding exception");
  await loaded;

  await SpecialPowers.spawn(browser, [], async function () {
    let doc = content.document;
    ok(
      !doc.documentURI.startsWith("about:certerror"),
      "Exception has been added"
    );
  });

  let certOverrideService = Cc[
    "@mozilla.org/security/certoverride;1"
  ].getService(Ci.nsICertOverrideService);
  certOverrideService.clearValidityOverride("expired.example.com", -1, {});

  loaded = BrowserTestUtils.waitForErrorPage(browser);
  BrowserReloadSkipCache();
  await loaded;

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
