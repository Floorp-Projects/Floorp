class SHistoryListener {
  constructor() {
    this.retval = true;
    this.last = "initial";
  }

  OnHistoryNewEntry() {
    this.last = "newentry";
  }

  OnHistoryGotoIndex() {
    this.last = "gotoindex";
  }

  OnHistoryPurge() {
    this.last = "purge";
  }

  OnHistoryReload() {
    this.last = "reload";
    return this.retval;
  }

  OnHistoryReplaceEntry() {}
}
SHistoryListener.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsISHistoryListener",
  "nsISupportsWeakReference",
]);

let listeners;

export class Bug422543Child extends JSWindowActorChild {
  constructor() {
    super();
  }

  actorCreated() {
    if (listeners) {
      return;
    }

    this.shistory = this.docShell.nsIWebNavigation.sessionHistory;
    listeners = [new SHistoryListener(), new SHistoryListener()];

    for (let listener of listeners) {
      this.shistory.legacySHistory.addSHistoryListener(listener);
    }
  }

  cleanup() {
    for (let listener of listeners) {
      this.shistory.legacySHistory.removeSHistoryListener(listener);
    }
    this.shistory = null;
    listeners = null;
    return {};
  }

  getListenerStatus() {
    return listeners.map(l => l.last);
  }

  resetListeners() {
    for (let listener of listeners) {
      listener.last = "initial";
    }

    return {};
  }

  notifyReload() {
    let history = this.shistory.legacySHistory;
    let rval = history.notifyOnHistoryReload();
    return { rval };
  }

  setRetval({ num, val }) {
    listeners[num].retval = val;
    return {};
  }

  receiveMessage(msg) {
    switch (msg.name) {
      case "cleanup":
        return this.cleanup();
      case "getListenerStatus":
        return this.getListenerStatus();
      case "notifyReload":
        return this.notifyReload();
      case "resetListeners":
        return this.resetListeners();
      case "setRetval":
        return this.setRetval(msg.data);
    }
    return null;
  }
}
