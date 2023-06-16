const { UIState } = ChromeUtils.importESModule(
  "resource://services-sync/UIState.sys.mjs"
);
const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

function promiseSyncReady() {
  let service = Cc["@mozilla.org/weave/service;1"].getService(
    Ci.nsISupports
  ).wrappedJSObject;
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
  if (isSendableURI) {
    sandbox.stub(BrowserUtils, "getShareableURL").returnsArg(0);
  } else {
    sandbox.stub(BrowserUtils, "getShareableURL").returns(null);
  }
  sandbox.stub(fxAccounts.device, "refreshDeviceList").resolves(true);
  sandbox.stub(fxAccounts.commands.sendTab, "send").resolves({ failed: [] });
  return sandbox;
}
