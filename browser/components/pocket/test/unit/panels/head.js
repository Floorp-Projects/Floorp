const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

const testGlobal = {
  PKT_PANEL_OVERLAY: class {
    create() {}
  },
  RPMRemoveMessageListener: () => {},
  RPMAddMessageListener: () => {},
  RPMSendAsyncMessage: () => {},
  window: {},
  self: {},
};

Services.scriptloader.loadSubScript(
  "chrome://pocket/content/panels/js/vendor.bundle.js",
  testGlobal
);

Services.scriptloader.loadSubScript(
  "chrome://pocket/content/panels/js/main.bundle.js",
  testGlobal
);
