const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

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
