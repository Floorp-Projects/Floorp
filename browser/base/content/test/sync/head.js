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

function setupSendTabMocks({ syncReady, clientsSynced, targets, state, isSendableURI }) {
  const sandbox = sinon.sandbox.create();
  sandbox.stub(gSync, "syncReady").get(() => syncReady);
  sandbox.stub(Weave.Service.clientsEngine, "isFirstSync").get(() => !clientsSynced);
  sandbox.stub(gSync, "sendTabTargets").get(() => targets);
  sandbox.stub(UIState, "get").returns({ status: state });
  sandbox.stub(gSync, "isSendableURI").returns(isSendableURI);
  return sandbox;
}
