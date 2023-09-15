/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const PREF_SVG_DISABLED = "svg.disabled";
const PREF_AVIF_ENABLED = "image.avif.enabled";
const PDF_MIME = "application/pdf";
const OCTET_MIME = "application/octet-stream";
const XML_MIME = "text/xml";
const SVG_MIME = "image/svg+xml";
const AVIF_MIME = "image/avif";

const { Integration } = ChromeUtils.importESModule(
  "resource://gre/modules/Integration.sys.mjs"
);
const {
  DownloadsViewableInternally,
  PREF_ENABLED_TYPES,
  PREF_BRANCH_WAS_REGISTERED,
  PREF_BRANCH_PREVIOUS_ACTION,
  PREF_BRANCH_PREVIOUS_ASK,
} = ChromeUtils.importESModule(
  "resource:///modules/DownloadsViewableInternally.sys.mjs"
);

/* global DownloadIntegration */
Integration.downloads.defineESModuleGetter(
  this,
  "DownloadIntegration",
  "resource://gre/modules/DownloadIntegration.sys.mjs"
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
  checkPreferInternal(mime, ext, expected && ext != "xml" && ext != "svg");
  checkShouldView(mime, ext, expected);
  if (ext != "xml" && ext != "svg") {
    checkWasRegistered(ext, expected);
  }
}

add_task(async function test_viewable_internally() {
  Services.prefs.setCharPref(PREF_ENABLED_TYPES, "xml , svg,avif");
  Services.prefs.setBoolPref(PREF_SVG_DISABLED, false);
  Services.prefs.setBoolPref(PREF_AVIF_ENABLED, true);

  checkAll(XML_MIME, "xml", false);
  checkAll(SVG_MIME, "svg", false);
  checkAll(AVIF_MIME, "avif", false);

  DownloadsViewableInternally.register();

  checkAll(XML_MIME, "xml", true);
  checkAll(SVG_MIME, "svg", true);
  checkAll(AVIF_MIME, "avif", true);

  // Disable xml, and avif, check that avif becomes disabled
  Services.prefs.setCharPref(PREF_ENABLED_TYPES, "svg");

  // (XML is externally managed)
  checkAll(XML_MIME, "xml", true);

  // Avif should be disabled
  checkAll(AVIF_MIME, "avif", false);

  // SVG shouldn't be cleared as it's still enabled
  checkAll(SVG_MIME, "svg", true);

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

  Assert.ok(!shouldView(AVIF_MIME), "image/avif should be disabled by pref");

  // Enable, check that everything is enabled again
  Services.prefs.setCharPref(PREF_ENABLED_TYPES, "xml,svg,avif");

  checkAll(XML_MIME, "xml", true);
  checkAll(SVG_MIME, "svg", true);
  checkPreferInternal(AVIF_MIME, "avif", true);

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

  // Enable viewable internally, SVG and XML should not be replaced.
  Services.prefs.setCharPref(PREF_ENABLED_TYPES, "svg,xml");

  Assert.equal(
    Services.prefs.prefHasUserValue(PREF_BRANCH_PREVIOUS_ACTION + "svg"),
    false,
    "svg action should not be stored"
  );
  Assert.equal(
    Services.prefs.prefHasUserValue(PREF_BRANCH_PREVIOUS_ASK + "svg"),
    false,
    "svg ask should not be stored"
  );

  {
    let handler = MIMEService.getFromTypeAndExtension(SVG_MIME, "svg");
    Assert.equal(
      handler.preferredAction,
      Ci.nsIHandlerInfo.saveToDisk,
      "svg action should be preserved"
    );
    Assert.equal(
      !!handler.alwaysAskBeforeHandling,
      true,
      "svg ask should be preserved"
    );
    // Clean up
    HandlerService.remove(handler);
    handler = MIMEService.getFromTypeAndExtension(XML_MIME, "xml");
    Assert.equal(
      handler.preferredAction,
      Ci.nsIHandlerInfo.useSystemDefault,
      "xml action should be preserved"
    );
    Assert.equal(
      !!handler.alwaysAskBeforeHandling,
      true,
      "xml ask should be preserved"
    );
    // Clean up
    HandlerService.remove(handler);
  }
  // It should still be possible to view XML internally
  checkShouldView(XML_MIME, "xml", true);

  checkAll(SVG_MIME, "svg", true);

  // Disable SVG to test SVG enabled check (depends on the pref)
  Services.prefs.setBoolPref(PREF_SVG_DISABLED, true);
  checkAll(SVG_MIME, "svg", false);
  Services.prefs.setBoolPref(PREF_SVG_DISABLED, false);
  {
    let handler = MIMEService.getFromTypeAndExtension(SVG_MIME, "svg");
    handler.preferredAction = Ci.nsIHandlerInfo.saveToDisk;
    handler.alwaysAskBeforeHandling = false;
    HandlerService.store(handler);
  }

  checkAll(SVG_MIME, "svg", true);

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
});
