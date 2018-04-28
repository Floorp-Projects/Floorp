var Cm = Components.manager;

const kBlocklistServiceUUID = "{66354bc9-7ed1-4692-ae1d-8da97d6b205e}";
const kBlocklistServiceContractID = "@mozilla.org/extensions/blocklist;1";
const kBlocklistServiceFactory = Cm.getClassObject(Cc[kBlocklistServiceContractID], Ci.nsIFactory);

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");

SimpleTest.requestFlakyTimeout("Need to simulate blocklist calls actually taking non-0 time to return");

/*
 * A lightweight blocklist proxy for the testing purposes.
 */
var BlocklistProxy = {
  _uuid: null,

  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsIBlocklistService,
                                          Ci.nsITimerCallback]),

  init() {
    if (!this._uuid) {
      this._uuid =
        Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator)
                                           .generateUUID();
      Cm.nsIComponentRegistrar.registerFactory(this._uuid, "",
                                               "@mozilla.org/extensions/blocklist;1",
                                               this);
    }
  },

  uninit() {
    if (this._uuid) {
      Cm.nsIComponentRegistrar.unregisterFactory(this._uuid, this);
      Cm.nsIComponentRegistrar.registerFactory(Components.ID(kBlocklistServiceUUID),
                                               "Blocklist Service",
                                               "@mozilla.org/extensions/blocklist;1",
                                               kBlocklistServiceFactory);
      this._uuid = null;
    }
  },

  notify(aTimer) {
  },

  observe(aSubject, aTopic, aData) {
  },

  async getAddonBlocklistState(aAddon, aAppVersion, aToolkitVersion) {
    await new Promise(r => setTimeout(r, 150));
    return 0; // STATE_NOT_BLOCKED
  },

  async getPluginBlocklistState(aPluginTag, aAppVersion, aToolkitVersion) {
    await new Promise(r => setTimeout(r, 150));
    return 0; // STATE_NOT_BLOCKED
  },

  async getPluginBlockURL(aPluginTag) {
    await new Promise(r => setTimeout(r, 150));
    return "";
  },
};

BlocklistProxy.init();
addEventListener("unload", () => {
  BlocklistProxy.uninit();
});
