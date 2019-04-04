/* eslint-env mozilla/frame-script */

function SHistoryListener() {
}

SHistoryListener.prototype = {
  retval: true,
  last: "initial",

  OnHistoryNewEntry(aNewURI) {
    this.last = "newentry";
  },

  OnHistoryGotoIndex() {
    this.last = "gotoindex";
  },

  OnHistoryPurge() {
    this.last = "purge";
  },

  OnHistoryReload() {
    this.last = "reload";
    return this.retval;
  },

  OnHistoryReplaceEntry() {},

  QueryInterface: ChromeUtils.generateQI([Ci.nsISHistoryListener,
                                          Ci.nsISupportsWeakReference]),
};

let testAPI = {
  shistory: null,
  listeners: [ new SHistoryListener(),
               new SHistoryListener() ],

  init() {
    this.shistory = docShell.QueryInterface(Ci.nsIWebNavigation).sessionHistory;
    for (let listener of this.listeners) {
      this.shistory.legacySHistory.addSHistoryListener(listener);
    }
  },

  cleanup() {
    for (let listener of this.listeners) {
      this.shistory.legacySHistory.removeSHistoryListener(listener);
    }
    this.shistory = null;
    sendAsyncMessage("bug422543:cleanup:return", {});
  },

  getListenerStatus() {
    sendAsyncMessage("bug422543:getListenerStatus:return",
                     this.listeners.map(l => l.last));
  },

  resetListeners() {
    for (let listener of this.listeners) {
      listener.last = "initial";
    }

    sendAsyncMessage("bug422543:resetListeners:return", {});
  },

  notifyReload() {
    let history = this.shistory.legacySHistory;
    let rval = history.notifyOnHistoryReload();
    sendAsyncMessage("bug422543:notifyReload:return", { rval });
  },

  setRetval({ num, val }) {
    this.listeners[num].retval = val;
    sendAsyncMessage("bug422543:setRetval:return", {});
  },
};

addMessageListener("bug422543:cleanup", () => { testAPI.cleanup(); });
addMessageListener("bug422543:getListenerStatus", () => { testAPI.getListenerStatus(); });
addMessageListener("bug422543:notifyReload", () => { testAPI.notifyReload(); });
addMessageListener("bug422543:resetListeners", () => { testAPI.resetListeners(); });
addMessageListener("bug422543:setRetval", (msg) => { testAPI.setRetval(msg.data); });

testAPI.init();
