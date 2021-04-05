var EXPORTED_SYMBOLS = ["BlocklistTestProxyChild"];

var Cm = Components.manager;

const kBlocklistServiceUUID = "{66354bc9-7ed1-4692-ae1d-8da97d6b205e}";
const kBlocklistServiceContractID = "@mozilla.org/extensions/blocklist;1";

let existingBlocklistFactory = null;
try {
  existingBlocklistFactory = Cm.getClassObject(
    Cc[kBlocklistServiceContractID],
    Ci.nsIFactory
  );
} catch (ex) {}

const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

/*
 * A lightweight blocklist proxy for testing purposes.
 */
var BlocklistProxy = {
  _uuid: null,

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsIBlocklistService",
    "nsITimerCallback",
  ]),

  init() {
    if (!this._uuid) {
      this._uuid = Cc["@mozilla.org/uuid-generator;1"]
        .getService(Ci.nsIUUIDGenerator)
        .generateUUID();
      Cm.nsIComponentRegistrar.registerFactory(
        this._uuid,
        "",
        "@mozilla.org/extensions/blocklist;1",
        this
      );
    }
  },

  uninit() {
    if (this._uuid) {
      Cm.nsIComponentRegistrar.unregisterFactory(this._uuid, this);
      if (existingBlocklistFactory) {
        Cm.nsIComponentRegistrar.registerFactory(
          Components.ID(kBlocklistServiceUUID),
          "Blocklist Service",
          "@mozilla.org/extensions/blocklist;1",
          existingBlocklistFactory
        );
      }
      this._uuid = null;
    }
  },

  notify(aTimer) {},

  async getAddonBlocklistState(aAddon, aAppVersion, aToolkitVersion) {
    await new Promise(r => setTimeout(r, 150));
    return 0; // STATE_NOT_BLOCKED
  },
};

class BlocklistTestProxyChild extends JSProcessActorChild {
  constructor() {
    super();
    BlocklistProxy.init();
  }

  receiveMessage(message) {
    if (message.name == "unload") {
      BlocklistProxy.uninit();
    }
  }
}
