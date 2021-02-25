const { UIState } = ChromeUtils.import("resource://services-sync/UIState.jsm");
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

function promiseSyncReady() {
  let service = Cc["@mozilla.org/weave/service;1"].getService(Ci.nsISupports)
    .wrappedJSObject;
  return service.whenLoaded();
}

function setupSendTabMocks({
  fxaDevices = null,
  state = UIState.STATUS_SIGNED_IN,
  isSendableURI = true,
}) {
  const sandbox = sinon.createSandbox();
  sandbox.stub(fxAccounts.device, "recentDeviceList").get(() => fxaDevices);
  sandbox.stub(UIState, "get").returns({
    status: state,
    syncEnabled: true,
  });
  sandbox.stub(BrowserUtils, "isShareableURL").returns(isSendableURI);
  sandbox.stub(fxAccounts.device, "refreshDeviceList").resolves(true);
  return sandbox;
}
