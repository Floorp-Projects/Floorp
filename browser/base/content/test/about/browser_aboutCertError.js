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
const { TabStateFlusher } = ChromeUtils.import(
  "resource:///modules/sessionstore/TabStateFlusher.jsm"
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
    await SpecialPowers.spawn(bc, [useFrame], async function(subFrame) {
      let returnButton = content.document.getElementById("returnButton");
      if (!subFrame) {
        Assert.equal(
          returnButton.getAttribute("autofocus"),
          "true",
          "returnButton has autofocus"
        );
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
    await SpecialPowers.spawn(bc, [useFrame], async function(subFrame) {
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

    let message = await SpecialPowers.spawn(bc, [], async function() {
      let doc = content.document;
      let shortDescText = doc.getElementById("errorShortDescText");
      Assert.ok(
        shortDescText.textContent.includes("expired.example.com"),
        "Should list hostname in error message."
      );

      let exceptionButton = doc.getElementById("exceptionDialogButton");
      Assert.ok(
        !exceptionButton.disabled,
        "Exception button is not disabled by default."
      );

      let advancedButton = doc.getElementById("advancedButton");
      advancedButton.click();

      // Wait until fluent sets the errorCode inner text.
      let el;
      await ContentTaskUtils.waitForCondition(() => {
        el = doc.getElementById("errorCode");
        return el.textContent != "";
      }, "error code has been set inside the advanced button panel");

      return { textContent: el.textContent, tagName: el.tagName };
    });
    is(
      message.textContent,
      "SEC_ERROR_EXPIRED_CERTIFICATE",
      "Correct error message found"
    );
    is(message.tagName, "a", "Error message is a link");

    message = await SpecialPowers.spawn(bc, [], async function() {
      let doc = content.document;
      let errorCode = doc.getElementById("errorCode");
      errorCode.click();
      let div = doc.getElementById("certificateErrorDebugInformation");
      let text = doc.getElementById("certificateErrorText");

      let serhelper = Cc[
        "@mozilla.org/network/serialization-helper;1"
      ].getService(Ci.nsISerializationHelper);
      let serializable = content.docShell.failedChannel.securityInfo
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
    let certChain = getCertChain(message.securityInfoAsString);
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

    let message = await SpecialPowers.spawn(bc, [], async function() {
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

    message = await SpecialPowers.spawn(bc, [], async function() {
      let doc = content.document;

      let errorCode = doc.getElementById("errorCode");
      errorCode.click();
      let div = doc.getElementById("certificateErrorDebugInformation");
      let text = doc.getElementById("certificateErrorText");

      let serhelper = Cc[
        "@mozilla.org/network/serialization-helper;1"
      ].getService(Ci.nsISerializationHelper);
      let serializable = content.docShell.failedChannel.securityInfo
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
    let certChain = getCertChain(message.securityInfoAsString);
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

    let href = await SpecialPowers.spawn(bc, [], async function() {
      let learnMoreLink = content.document.getElementById("learnMoreLink");
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

    let bc = browser.browsingContext;
    if (useFrame) {
      bc = bc.children[0];
    }

    await SpecialPowers.spawn(bc, [useFrame], async function(subFrame) {
      Assert.equal(
        content.document.body.classList.contains("caution"),
        !subFrame,
        `Cert error body has ${subFrame ? "no" : ""} caution class`
      );
    });

    BrowserTestUtils.removeTab(gBrowser.selectedTab);

    tab = await openErrorPage(BAD_STS_CERT, useFrame);
    bc = tab.linkedBrowser.browsingContext;
    await SpecialPowers.spawn(bc, [], async function() {
      Assert.ok(
        !content.document.body.classList.contains("caution"),
        "Cert error body has no caution class"
      );
    });

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
});

add_task(async function checkViewCertificate() {
  info("Loading a cert error and checking that the certificate can be shown.");
  SpecialPowers.pushPrefEnv({
    set: [["security.aboutcertificate.enabled", true]],
  });
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
    await SpecialPowers.spawn(bc, [], async function() {
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
      async function() {
        let doc = content.document;
        let certificateSection = await ContentTaskUtils.waitForCondition(() => {
          return doc.querySelector("certificate-section");
        }, "Certificate section found");

        let infoGroup = certificateSection.shadowRoot.querySelector(
          "info-group"
        );
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

        let commonnameValue = items[items.length - 1].shadowRoot.querySelector(
          ".info"
        ).textContent;
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
      let titleText = content.document.querySelector(".title-text");
      await ContentTaskUtils.waitForCondition(
        () => titleText.textContent,
        "Error page title is initialized"
      );
      let titleContent = titleText.textContent;
      if (_useFrame) {
        ok(
          titleContent.endsWith("Security Issue"),
          "Did Not Connect: Potential Security Issue"
        );
      } else {
        ok(
          titleContent.endsWith("Risk Ahead"),
          "Warning: Potential Security Risk Ahead"
        );
      }
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
  await SpecialPowers.spawn(bc, [], async function() {
    let doc = content.document;
    let titleText = doc.querySelector(".title-text");
    Assert.ok(
      titleText.textContent.endsWith("Security Issue"),
      "Title shows Did Not Connect: Potential Security Issue"
    );

    // Wait until fluent sets the errorCode inner text.
    let el;
    await ContentTaskUtils.waitForCondition(() => {
      el = doc.getElementById("errorCode");
      return el.textContent != "";
    }, "error code has been set inside the advanced button panel");

    Assert.equal(
      el.textContent,
      "SEC_ERROR_EXPIRED_CERTIFICATE",
      "Correct error message found"
    );
    Assert.equal(el.tagName, "a", "Error message is a link");
  });
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
