/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

ChromeUtils.import("resource://services-sync/UIState.jsm", this);

registerCleanupFunction(function() {
  delete window.sinon;
});

function promiseSyncReady() {
  let service = Cc["@mozilla.org/weave/service;1"]
                  .getService(Ci.nsISupports)
                  .wrappedJSObject;
  return service.whenLoaded();
}

function setupSendTabMocks({ syncReady = true, fxaDevices = null,
                             state = UIState.STATUS_SIGNED_IN, isSendableURI = true }) {
  const sandbox = sinon.sandbox.create();
  sandbox.stub(gSync, "syncReady").get(() => syncReady);
  if (fxaDevices) {
    // Clone fxaDevices because it gets sorted in-place.
    sandbox.stub(Weave.Service.clientsEngine, "fxaDevices").get(() => [...fxaDevices]);
  }
  sandbox.stub(Weave.Service.clientsEngine, "hasSyncedThisSession").get(() => !!fxaDevices);
  sandbox.stub(UIState, "get").returns({ status: state });
  sandbox.stub(gSync, "isSendableURI").returns(isSendableURI);
  return sandbox;
}
