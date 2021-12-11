/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_SVG_DISABLED = "svg.disabled";
const PREF_WEBP_ENABLED = "image.webp.enabled";
const PDF_MIME = "application/pdf";
const OCTET_MIME = "application/octet-stream";
const XML_MIME = "text/xml";
const SVG_MIME = "image/svg+xml";
const WEBP_MIME = "image/webp";

const { Integration } = ChromeUtils.import(
  "resource://gre/modules/Integration.jsm"
);
const {
  DownloadsViewableInternally,
  PREF_ENABLED_TYPES,
  PREF_BRANCH_WAS_REGISTERED,
  PREF_BRANCH_PREVIOUS_ACTION,
  PREF_BRANCH_PREVIOUS_ASK,
} = ChromeUtils.import("resource:///modules/DownloadsViewableInternally.jsm");

/* global DownloadIntegration */
Integration.downloads.defineModuleGetter(
  this,
  "DownloadIntegration",
  "resource://gre/modules/DownloadIntegration.jsm"
);

const HandlerService = Cc[
  "@mozilla.org/uriloader/handler-service;1"
].getService(Ci.nsIHandlerService);
const MIMEService = Cc["@mozilla.org/mime;1"].getService(Ci.nsIMIMEService);

function checkPreferInternal(mime, ext, expectedPreferInternal) {
  const handler = MIMEService.getFromTypeAndExtension(mime, ext);
  if (expectedPreferInternal) {
    Assert.equal(
      handler?.preferredAction,
      Ci.nsIHandlerInfo.handleInternally,
      `checking ${mime} preferredAction == handleInternally`
    );
  } else {
    Assert.notEqual(
      handler?.preferredAction,
      Ci.nsIHandlerInfo.handleInternally,
      `checking ${mime} preferredAction != handleInternally`
    );
  }
}

function shouldView(mime, ext) {
  return DownloadIntegration.shouldViewDownloadInternally(mime, ext);
}

function checkShouldView(mime, ext, expectedShouldView) {
  Assert.equal(
    shouldView(mime, ext),
    expectedShouldView,
    `checking ${mime} shouldViewDownloadInternally`
  );
}

function checkWasRegistered(ext, expectedWasRegistered) {
  Assert.equal(
    Services.prefs.getBoolPref(PREF_BRANCH_WAS_REGISTERED + ext, false),
    expectedWasRegistered,
    `checking ${ext} was registered pref`
  );
}

function checkAll(mime, ext, expected) {
  checkPreferInternal(mime, ext, expected);
  checkShouldView(mime, ext, expected);
  checkWasRegistered(ext, expected);
}

add_task(async function test_viewable_internally() {
  Services.prefs.setCharPref(PREF_ENABLED_TYPES, "xml , svg,webp");
  Services.prefs.setBoolPref(PREF_SVG_DISABLED, false);
  Services.prefs.setBoolPref(PREF_WEBP_ENABLED, true);

  checkAll(XML_MIME, "xml", false);
  checkAll(SVG_MIME, "svg", false);
  checkAll(WEBP_MIME, "webp", false);

  DownloadsViewableInternally.register();

  checkAll(XML_MIME, "xml", true);
  checkAll(SVG_MIME, "svg", true);
  checkAll(WEBP_MIME, "webp", true);

  // Remove SVG so it won't be cleared
  Services.prefs.clearUserPref(PREF_BRANCH_WAS_REGISTERED + "svg");

  // Disable xml and svg, check that xml becomes disabled
  Services.prefs.setCharPref(PREF_ENABLED_TYPES, "webp");

  checkAll(XML_MIME, "xml", false);
  checkAll(WEBP_MIME, "webp", true);

  // SVG shouldn't be cleared
  checkPreferInternal(SVG_MIME, "svg", true);

  Assert.ok(
    shouldView(PDF_MIME),
    "application/pdf should be unaffected by pref"
  );
  Assert.ok(
    shouldView(OCTET_MIME, "pdf"),
    ".pdf should be accepted by extension"
  );
  Assert.ok(
    shouldView(OCTET_MIME, "PDF"),
    ".pdf should be detected case-insensitively"
  );
  Assert.ok(!shouldView(OCTET_MIME, "exe"), ".exe shouldn't be accepted");

  Assert.ok(!shouldView(XML_MIME), "text/xml should be disabled by pref");
  Assert.ok(!shouldView(SVG_MIME), "image/xml+svg should be disabled by pref");

  // Enable, check that everything is enabled again
  Services.prefs.setCharPref(PREF_ENABLED_TYPES, "xml,svg,webp");

  checkPreferInternal(XML_MIME, "xml", true);
  checkPreferInternal(SVG_MIME, "svg", true);

  Assert.ok(
    shouldView(PDF_MIME),
    "application/pdf should be unaffected by pref"
  );
  Assert.ok(shouldView(XML_MIME), "text/xml should be enabled by pref");
  Assert.ok(
    shouldView("application/xml"),
    "alternate MIME type application/xml should be accepted"
  );
  Assert.ok(
    shouldView(OCTET_MIME, "xml"),
    ".xml should be accepted by extension"
  );

  // Disable viewable internally, pre-set handlers.
  Services.prefs.setCharPref(PREF_ENABLED_TYPES, "");

  for (const [mime, ext, action, ask] of [
    [XML_MIME, "xml", Ci.nsIHandlerInfo.useSystemDefault, true],
    [SVG_MIME, "svg", Ci.nsIHandlerInfo.saveToDisk, true],
    [WEBP_MIME, "webp", Ci.nsIHandlerInfo.saveToDisk, false],
  ]) {
    let handler = MIMEService.getFromTypeAndExtension(mime, ext);
    handler.preferredAction = action;
    handler.alwaysAskBeforeHandling = ask;

    HandlerService.store(handler);
    checkPreferInternal(mime, ext, false);

    // Expect to read back the same values
    handler = MIMEService.getFromTypeAndExtension(mime, ext);
    Assert.equal(handler.preferredAction, action);
    Assert.equal(handler.alwaysAskBeforeHandling, ask);
  }

  // Enable viewable internally, XML should not be replaced, SVG and WebP should be saved.
  Services.prefs.setCharPref(PREF_ENABLED_TYPES, "svg,webp,xml");

  Assert.equal(
    Services.prefs.getIntPref(PREF_BRANCH_PREVIOUS_ACTION + "svg"),
    Ci.nsIHandlerInfo.saveToDisk,
    "svg action should be saved"
  );
  Assert.equal(
    Services.prefs.getBoolPref(PREF_BRANCH_PREVIOUS_ASK + "svg"),
    true,
    "svg ask should be saved"
  );
  Assert.equal(
    Services.prefs.getIntPref(PREF_BRANCH_PREVIOUS_ACTION + "webp"),
    Ci.nsIHandlerInfo.saveToDisk,
    "webp action should be saved"
  );
  Assert.equal(
    Services.prefs.getBoolPref(PREF_BRANCH_PREVIOUS_ASK + "webp"),
    false,
    "webp ask should be saved"
  );

  {
    let handler = MIMEService.getFromTypeAndExtension(XML_MIME, "xml");
    Assert.equal(
      handler.preferredAction,
      Ci.nsIHandlerInfo.useSystemDefault,
      "svg action should be preserved"
    );
    Assert.equal(
      !!handler.alwaysAskBeforeHandling,
      true,
      "svg ask should be preserved"
    );
    // Clean up
    HandlerService.remove(handler);
  }
  // It should still be possible to view XML internally
  checkShouldView(XML_MIME, "xml", true);
  checkWasRegistered("xml", true);

  checkAll(SVG_MIME, "svg", true);
  checkAll(WEBP_MIME, "webp", true);

  // Disable SVG to test SVG enabled check (depends on the pref)
  Services.prefs.setBoolPref(PREF_SVG_DISABLED, true);
  // Should have restored the settings from above
  {
    let handler = MIMEService.getFromTypeAndExtension(SVG_MIME, "svg");
    Assert.equal(handler.preferredAction, Ci.nsIHandlerInfo.saveToDisk);
    Assert.equal(!!handler.alwaysAskBeforeHandling, true);
    // Clean up
    HandlerService.remove(handler);
  }
  checkAll(SVG_MIME, "svg", false);

  Services.prefs.setBoolPref(PREF_SVG_DISABLED, false);
  checkAll(SVG_MIME, "svg", true);

  // Test WebP enabled check (depends on the pref)
  Services.prefs.setBoolPref(PREF_WEBP_ENABLED, false);
  // Should have restored the settings from above
  {
    let handler = MIMEService.getFromTypeAndExtension(WEBP_MIME, "webp");
    Assert.equal(handler.preferredAction, Ci.nsIHandlerInfo.saveToDisk);
    Assert.equal(!!handler.alwaysAskBeforeHandling, false);
    // Clean up
    HandlerService.remove(handler);
  }
  checkAll(WEBP_MIME, "webp", false);

  Services.prefs.setBoolPref(PREF_WEBP_ENABLED, true);
  checkAll(WEBP_MIME, "webp", true);

  Assert.ok(!shouldView(null, "pdf"), "missing MIME shouldn't be accepted");
  Assert.ok(!shouldView(null, "xml"), "missing MIME shouldn't be accepted");
  Assert.ok(!shouldView(OCTET_MIME), "unsupported MIME shouldn't be accepted");
  Assert.ok(!shouldView(OCTET_MIME, "exe"), ".exe shouldn't be accepted");
});

registerCleanupFunction(() => {
  // Clear all types to remove any saved values
  Services.prefs.setCharPref(PREF_ENABLED_TYPES, "");
  // Reset to the defaults
  Services.prefs.clearUserPref(PREF_ENABLED_TYPES);
  Services.prefs.clearUserPref(PREF_SVG_DISABLED);
  Services.prefs.clearUserPref(PREF_WEBP_ENABLED);
});
