const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

const testGlobal = {
  PKT_PANEL_OVERLAY: class {
    create() {}
  },
  RPMRemoveMessageListener: () => {},
  RPMAddMessageListener: () => {},
  RPMSendAsyncMessage: () => {},
  window: {},
};

Services.scriptloader.loadSubScript(
  "chrome://pocket/content/panels/js/main.bundle.js",
  testGlobal
);
