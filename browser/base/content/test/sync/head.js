/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

Cu.import("resource://services-sync/UIState.jsm", this);

registerCleanupFunction(function() {
  delete window.sinon;
});

function promiseSyncReady() {
  let service = Cc["@mozilla.org/weave/service;1"]
                  .getService(Components.interfaces.nsISupports)
                  .wrappedJSObject;
  return service.whenLoaded();
}

function setupSendTabMocks({ syncReady, clientsSynced, remoteClients, state, isSendableURI }) {
  const sandbox = sinon.sandbox.create();
  sandbox.stub(gSync, "syncReady").get(() => syncReady);
  sandbox.stub(Weave.Service.clientsEngine, "lastSync").get(() => clientsSynced ? Date.now() : 0);
  sandbox.stub(gSync, "remoteClients").get(() => remoteClients);
  sandbox.stub(UIState, "get").returns({ status: state });
  sandbox.stub(gSync, "isSendableURI").returns(isSendableURI);
  return sandbox;
}
