ChromeUtils.import("resource://gre/modules/Services.jsm");
Cu.importGlobalProperties(["URL"]);

const {actionTypes: at} = ChromeUtils.import("resource://activity-stream/common/Actions.jsm", {});

ChromeUtils.defineModuleGetter(this, "DownloadsViewUI",
  "resource:///modules/DownloadsViewUI.jsm");
ChromeUtils.defineModuleGetter(this, "DownloadsCommon",
  "resource:///modules/DownloadsCommon.jsm");

const DOWNLOAD_CHANGED_DELAY_TIME = 1000; // time in ms to delay timer for downloads changed events

class DownloadElement extends DownloadsViewUI.DownloadElementShell {
  constructor(download, browser) {
    super();
    this._download = download;
    this.element = browser;
    this.element._shell = this;
  }

  get download() {
    return this._download;
  }

  get fileType() {
    if (!this.download.target.path) {
      return null;
    }
    let items = this.download.target.path.split(".");
    return items[items.length - 1].toUpperCase();
  }

  downloadsCmd_copyLocation() {
    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);
    clipboard.copyString(this.download.source.url);
  }
}

this.DownloadsManager = class DownloadsManager {
  constructor(store) {
    this._downloadData = null;
    this._store = null;
    this._viewableDownloadItems = new Map();
    this._downloadTimer = null;
  }

  setTimeout(callback, delay) {
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(callback, delay, Ci.nsITimer.TYPE_ONE_SHOT);
    return timer;
  }

  formatDownload(element) {
    const downloadedItem = element.download;
    let description;
    if (element.fileType) {
      // If we have a file type: '1.5 MB — PNG'
      description = `${element.sizeStrings.stateLabel} \u2014 ${element.fileType}`;
    } else {
      // If we do not have a file type: '1.5 MB'
      description = `${element.sizeStrings.stateLabel}`;
    }
    return {
      hostname: new URL(downloadedItem.source.url).hostname,
      url: downloadedItem.source.url,
      path: downloadedItem.target.path,
      title: element.displayName,
      description,
      referrer: downloadedItem.source.referrer
    };
  }

  init(store) {
    this._store = store;
    this._browser = Services.appShell.hiddenDOMWindow;
    this._downloadData = DownloadsCommon.getData(this._browser.ownerGlobal, true, false, true);
    this._downloadData.addView(this);
  }

  onDownloadAdded(download) {
    const elem = new DownloadElement(download, this._browser);
    const downloadedItem = elem.download;
    if (!this._viewableDownloadItems.has(downloadedItem.source.url)) {
      this._viewableDownloadItems.set(downloadedItem.source.url, elem);

      // On startup, all existing downloads fire this notification, so debounce them
      if (this._downloadTimer) {
        this._downloadTimer.delay = DOWNLOAD_CHANGED_DELAY_TIME;
      } else {
        this._downloadTimer = this.setTimeout(() => {
          this._downloadTimer = null;
          this._store.dispatch({type: at.DOWNLOAD_CHANGED});
        }, DOWNLOAD_CHANGED_DELAY_TIME);
      }
    }
  }

  onDownloadRemoved(download) {
    if (this._viewableDownloadItems.has(download.source.url)) {
      this._viewableDownloadItems.delete(download.source.url);
      this._store.dispatch({type: at.DOWNLOAD_CHANGED});
    }
  }

  async getDownloads(threshold, {numItems = this._viewableDownloadItems.size, onlySucceeded = false, onlyExists = false}) {
    if (!threshold) {
      return [];
    }
    let results = [];

    // Only get downloads within the time threshold specified and sort by recency
    const downloadThreshold = Date.now() - threshold;
    let downloads = [...this._viewableDownloadItems.values()]
                      .filter(elem => elem.download.endTime > downloadThreshold)
                      .sort((elem1, elem2) => elem1.download.endTime < elem2.download.endTime);

    for (const elem of downloads) {
      // Only include downloads where the file still exists
      if (onlyExists) {
        // Refresh download to ensure the 'exists' attribute is up to date
        await elem.download.refresh();
        if (!elem.download.target.exists) { continue; }
      }
      // Only include downloads that were completed successfully
      if (onlySucceeded) {
        if (!elem.download.succeeded) { continue; }
      }
      const formattedDownloadForHighlights = this.formatDownload(elem);
      results.push(formattedDownloadForHighlights);
      if (results.length === numItems) {
        break;
      }
    }
    return results;
  }

  uninit() {
    if (this._downloadData) {
      this._downloadData.removeView(this);
      this._downloadData = null;
    }
    if (this._downloadTimer) {
      this._downloadTimer.cancel();
      this._downloadTimer = null;
    }
  }

  onAction(action) {
    let downloadsCmd;
    switch (action.type) {
      case at.COPY_DOWNLOAD_LINK:
        downloadsCmd = "downloadsCmd_copyLocation";
        break;
      case at.REMOVE_DOWNLOAD_FILE:
        downloadsCmd = "downloadsCmd_delete";
        break;
      case at.SHOW_DOWNLOAD_FILE:
        downloadsCmd = "downloadsCmd_show";
        break;
      case at.OPEN_DOWNLOAD_FILE:
        downloadsCmd = "downloadsCmd_open";
        break;
      case at.UNINIT:
        this.uninit();
        break;
    }
    // Call the appropriate downloads command function based on the event we received
    if (downloadsCmd) {
      let elem = this._viewableDownloadItems.get(action.data.url);
      if (elem) {
        elem[downloadsCmd]();
      }
    }
  }
};
this.EXPORTED_SYMBOLS = ["DownloadsManager"];
