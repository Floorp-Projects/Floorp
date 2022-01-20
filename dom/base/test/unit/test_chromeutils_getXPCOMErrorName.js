"use strict";

// Test ChromeUtils.getXPCOMErrorName

add_task(function test_getXPCOMErrorName() {
  info("Force the initialization of NSS to get the error names right");
  Cc["@mozilla.org/psm;1"].getService(Ci.nsISupports);

  Assert.equal(
    ChromeUtils.getXPCOMErrorName(Cr.NS_OK),
    "NS_OK",
    "getXPCOMErrorName works for NS_OK"
  );

  Assert.equal(
    ChromeUtils.getXPCOMErrorName(Cr.NS_ERROR_FAILURE),
    "NS_ERROR_FAILURE",
    "getXPCOMErrorName works for NS_ERROR_FAILURE"
  );

  const nssErrors = Cc["@mozilla.org/nss_errors_service;1"].getService(
    Ci.nsINSSErrorsService
  );
  Assert.equal(
    ChromeUtils.getXPCOMErrorName(
      nssErrors.getXPCOMFromNSSError(Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE)
    ),
    "NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_SECURITY, SEC_ERROR_IO)",
    "getXPCOMErrorName works for NSS_SEC_ERROR_BASE"
  );
  // See https://searchfox.org/mozilla-central/rev/a48e21143960b383004afa9ff9411c5cf6d5a958/security/nss/lib/util/secerr.h#20
  const SEC_ERROR_BAD_DATA = Ci.nsINSSErrorsService.NSS_SEC_ERROR_BASE + 2;
  Assert.equal(
    ChromeUtils.getXPCOMErrorName(
      nssErrors.getXPCOMFromNSSError(SEC_ERROR_BAD_DATA)
    ),
    "NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_SECURITY, SEC_ERROR_BAD_DATA)",
    "getXPCOMErrorName works for NSS's SEC_ERROR_BAD_DATA"
  );
});
