/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const EXPIRED_CERT = "https://expired.example.com/";
const BAD_CERT = "https://mismatch.badcertdomain.example.com/";

const kErrors = [
  "MOZILLA_PKIX_ERROR_MITM_DETECTED",
  "SEC_ERROR_UNKNOWN_ISSUER",
  "SEC_ERROR_CA_CERT_INVALID",
  "SEC_ERROR_UNTRUSTED_ISSUER",
  "SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED",
  "SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE",
  "MOZILLA_PKIX_ERROR_SELF_SIGNED_CERT",
  "MOZILLA_PKIX_ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED",
];

/**
 * This is a quasi-unit test style test to check what happens
 * when we encounter certificates with multiple problems.
 *
 * We should consistently display the most urgent message first.
 */
add_task(async function test_expired_bad_cert() {
  let tab = await openErrorPage(EXPIRED_CERT);
  const kExpiryLabel = "cert-error-expired-now";
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(browser, [kExpiryLabel, kErrors], async function(
    knownExpiryLabel,
    errorCodes
  ) {
    await ContentTaskUtils.waitForCondition(
      () =>
        !!content.document.querySelectorAll(`#badCertTechnicalInfo label`)
          .length
    );
    // First check that for this real cert, which is simply expired and nothing else,
    // we show the expiry info:
    let rightLabel = content.document.querySelector(
      `#badCertTechnicalInfo label[data-l10n-id="${knownExpiryLabel}"]`
    );
    ok(rightLabel, "Expected a label with the right contents.");
    info(content.document.querySelector("#badCertTechnicalInfo").innerHTML);

    const kExpiredErrors = errorCodes.map(err => {
      return Cu.cloneInto(
        {
          errorCodeString: err,
          isUntrusted: true,
          isNotValidAtThisTime: true,
          validNotBefore: 0,
          validNotAfter: 86400000,
        },
        content.window
      );
    });
    for (let err of kExpiredErrors) {
      // Test hack: invoke the content-privileged helper method with the fake cert info.
      await Cu.waiveXrays(content.window).setTechnicalDetailsOnCertError(err);
      let allLabels = content.document.querySelectorAll(
        "#badCertTechnicalInfo label"
      );
      ok(
        allLabels.length,
        "There should be an advanced technical description for " +
          err.errorCodeString
      );
      for (let label of allLabels) {
        let id = label.getAttribute("data-l10n-id");
        ok(
          id,
          `Label for ${err.errorCodeString} should have data-l10n-id (was: ${id}).`
        );
        isnot(
          id,
          knownExpiryLabel,
          `Label for ${err.errorCodeString} should not be about expiry.`
        );
      }
    }
  });
  await BrowserTestUtils.removeTab(tab);
});

/**
 * The same as above, but now for alt-svc domain mismatch certs.
 */
add_task(async function test_alt_svc_bad_cert() {
  let tab = await openErrorPage(BAD_CERT);
  const kErrKnownPrefix = "cert-error-domain-mismatch";
  const kErrKnownAlt = "cert-error-domain-mismatch-single-nolink";
  let browser = tab.linkedBrowser;
  await SpecialPowers.spawn(
    browser,
    [kErrKnownAlt, kErrKnownPrefix, kErrors],
    async function(knownAlt, knownAltPrefix, errorCodes) {
      await ContentTaskUtils.waitForCondition(
        () =>
          !!content.document.querySelectorAll(`#badCertTechnicalInfo label`)
            .length
      );
      // First check that for this real cert, which is simply for the wrong domain and nothing else,
      // we show the alt-svc info:
      let rightLabel = content.document.querySelector(
        `#badCertTechnicalInfo label[data-l10n-id="${knownAlt}"]`
      );
      ok(rightLabel, "Expected a label with the right contents.");
      info(content.document.querySelector("#badCertTechnicalInfo").innerHTML);

      const kAltSvcErrors = errorCodes.map(err => {
        return Cu.cloneInto(
          {
            errorCodeString: err,
            isUntrusted: true,
            isDomainMismatch: true,
          },
          content.window
        );
      });
      for (let err of kAltSvcErrors) {
        // Test hack: invoke the content-privileged helper method with the fake cert info.
        await Cu.waiveXrays(content.window).setTechnicalDetailsOnCertError(err);
        let allLabels = content.document.querySelectorAll(
          "#badCertTechnicalInfo label"
        );
        ok(
          allLabels.length,
          "There should be an advanced technical description for " +
            err.errorCodeString
        );
        for (let label of allLabels) {
          let id = label.getAttribute("data-l10n-id");
          ok(
            id,
            `Label for ${err.errorCodeString} should have data-l10n-id (was: ${id}).`
          );
          isnot(
            id,
            knownAlt,
            `Label for ${err.errorCodeString} should not mention other domains.`
          );
          ok(
            !id.startsWith(knownAltPrefix),
            `Label shouldn't start with ${knownAltPrefix}`
          );
        }
      }
    }
  );
  await BrowserTestUtils.removeTab(tab);
});
