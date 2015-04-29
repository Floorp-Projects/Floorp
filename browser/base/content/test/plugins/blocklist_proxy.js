const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cm = Components.manager;

const kBlocklistServiceUUID = "{66354bc9-7ed1-4692-ae1d-8da97d6b205e}";
const kBlocklistServiceContractID = "@mozilla.org/extensions/blocklist;1";
const kBlocklistServiceFactory = Cm.getClassObject(Cc[kBlocklistServiceContractID], Ci.nsIFactory);

Cu.import('resource://gre/modules/XPCOMUtils.jsm');

/*
 * A lightweight blocklist proxy for the testing purposes.
 */
let BlocklistProxy = {
  _uuid: null,

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsIBlocklistService,
                                         Ci.nsITimerCallback]),

  init: function() {
    if (!this._uuid) {
      this._uuid =
        Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator)
                                           .generateUUID();
      Cm.nsIComponentRegistrar.registerFactory(this._uuid, "",
                                               "@mozilla.org/extensions/blocklist;1",
                                               this);
    }
  },

  uninit: function() {
    if (this._uuid) {
      Cm.nsIComponentRegistrar.unregisterFactory(this._uuid, this);
      Cm.nsIComponentRegistrar.registerFactory(Components.ID(kBlocklistServiceUUID),
                                               "Blocklist Service",
                                               "@mozilla.org/extensions/blocklist;1",
                                               kBlocklistServiceFactory);
      this._uuid = null;
    }
  },

  notify: function (aTimer) {
  },

  observe: function (aSubject, aTopic, aData) {
  },

  isAddonBlocklisted: function (aAddon, aAppVersion, aToolkitVersion) {
    return false;
  },

  getAddonBlocklistState: function (aAddon, aAppVersion, aToolkitVersion) {
    return 0; // STATE_NOT_BLOCKED
  },

  getPluginBlocklistState: function (aPluginTag, aAppVersion, aToolkitVersion) {
    return 0; // STATE_NOT_BLOCKED
  },

  getAddonBlocklistURL: function (aAddon, aAppVersion, aToolkitVersion) {
    return "";
  },

  getPluginBlocklistURL: function (aPluginTag) {
    return "";
  },

  getPluginInfoURL: function (aPluginTag) {
    return "";
  },
}

BlocklistProxy.init();
addEventListener("unload", () => {
  BlocklistProxy.uninit();
});
