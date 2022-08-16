/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module is imported by code that uses the "download.xml" binding, and
 * provides prototypes for objects that handle input and display information.
 */

"use strict";

var EXPORTED_SYMBOLS = ["DownloadsViewUI"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

XPCOMUtils.defineLazyModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  Downloads: "resource://gre/modules/Downloads.jsm",
  DownloadUtils: "resource://gre/modules/DownloadUtils.jsm",
  DownloadsCommon: "resource:///modules/DownloadsCommon.jsm",
  FileUtils: "resource://gre/modules/FileUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "handlerSvc",
  "@mozilla.org/uriloader/handler-service;1",
  "nsIHandlerService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gReputationService",
  "@mozilla.org/reputationservice/application-reputation-service;1",
  Ci.nsIApplicationReputationService
);

const { Integration } = ChromeUtils.import(
  "resource://gre/modules/Integration.jsm"
);

Integration.downloads.defineModuleGetter(
  lazy,
  "DownloadIntegration",
  "resource://gre/modules/DownloadIntegration.jsm"
);

const HTML_NS = "http://www.w3.org/1999/xhtml";

var gDownloadElementButtons = {
  cancel: {
    commandName: "downloadsCmd_cancel",
    l10nId: "downloads-cmd-cancel",
    descriptionL10nId: "downloads-cancel-download",
    panelL10nId: "downloads-cmd-cancel-panel",
    iconClass: "downloadIconCancel",
  },
  retry: {
    commandName: "downloadsCmd_retry",
    l10nId: "downloads-cmd-retry",
    descriptionL10nId: "downloads-retry-download",
    panelL10nId: "downloads-cmd-retry-panel",
    iconClass: "downloadIconRetry",
  },
  show: {
    commandName: "downloadsCmd_show",
    l10nId: "downloads-cmd-show-button-2",
    descriptionL10nId: "downloads-cmd-show-description-2",
    panelL10nId: "downloads-cmd-show-panel-2",
    iconClass: "downloadIconShow",
  },
  subviewOpenOrRemoveFile: {
    commandName: "downloadsCmd_showBlockedInfo",
    l10nId: "downloads-cmd-choose-open",
    descriptionL10nId: "downloads-show-more-information",
    panelL10nId: "downloads-cmd-choose-open-panel",
    iconClass: "downloadIconSubviewArrow",
  },
  askOpenOrRemoveFile: {
    commandName: "downloadsCmd_chooseOpen",
    l10nId: "downloads-cmd-choose-open",
    panelL10nId: "downloads-cmd-choose-open-panel",
    iconClass: "downloadIconShow",
  },
  askRemoveFileOrAllow: {
    commandName: "downloadsCmd_chooseUnblock",
    l10nId: "downloads-cmd-choose-unblock",
    panelL10nId: "downloads-cmd-choose-unblock-panel",
    iconClass: "downloadIconShow",
  },
  removeFile: {
    commandName: "downloadsCmd_confirmBlock",
    l10nId: "downloads-cmd-remove-file",
    panelL10nId: "downloads-cmd-remove-file-panel",
    iconClass: "downloadIconCancel",
  },
};

/**
 * Associates each document with a pre-built DOM fragment representing the
 * download list item. This is then cloned to create each individual list item.
 * This is stored on the document to prevent leaks that would occur if a single
 * instance created by one document's DOMParser was stored globally.
 */
var gDownloadListItemFragments = new WeakMap();

var DownloadsViewUI = {
  /**
   * Returns true if the given string is the name of a command that can be
   * handled by the Downloads user interface, including standard commands.
   */
  isCommandName(name) {
    return name.startsWith("cmd_") || name.startsWith("downloadsCmd_");
  },

  /**
   * Get source url of the download without'http' or'https' prefix.
   */
  getStrippedUrl(download) {
    return lazy.UrlbarUtils.stripPrefixAndTrim(download?.source?.url, {
      stripHttp: true,
      stripHttps: true,
    })[0];
  },

  /**
   * Returns the user-facing label for the given Download object. This is
   * normally the leaf name of the download target file. In case this is a very
   * old history download for which the target file is unknown, the download
   * source URI is displayed.
   */
  getDisplayName(download) {
    if (
      download.error?.reputationCheckVerdict ==
      lazy.Downloads.Error.BLOCK_VERDICT_DOWNLOAD_SPAM
    ) {
      let l10n = {
        id: "downloads-blocked-from-url",
        args: { url: DownloadsViewUI.getStrippedUrl(download) },
      };
      return { l10n };
    }
    return download.target.path
      ? PathUtils.filename(download.target.path)
      : download.source.url;
  },

  /**
   * Given a Download object, returns a string representing its file size with
   * an appropriate measurement unit, for example "1.5 MB", or an empty string
   * if the size is unknown.
   */
  getSizeWithUnits(download) {
    if (download.target.size === undefined) {
      return "";
    }

    let [size, unit] = lazy.DownloadUtils.convertByteUnits(
      download.target.size
    );
    return lazy.DownloadsCommon.strings.sizeWithUnits(size, unit);
  },

  /**
   * Given a context menu and a download element on which it is invoked,
   * update items in the context menu to reflect available options for
   * that download element.
   */
  updateContextMenuForElement(contextMenu, element) {
    // Get the state and ensure only the appropriate items are displayed.
    let state = parseInt(element.getAttribute("state"), 10);

    const document = contextMenu.ownerDocument;

    const {
      DOWNLOAD_NOTSTARTED,
      DOWNLOAD_DOWNLOADING,
      DOWNLOAD_FINISHED,
      DOWNLOAD_FAILED,
      DOWNLOAD_CANCELED,
      DOWNLOAD_PAUSED,
      DOWNLOAD_BLOCKED_PARENTAL,
      DOWNLOAD_DIRTY,
      DOWNLOAD_BLOCKED_POLICY,
    } = lazy.DownloadsCommon;

    contextMenu.querySelector(".downloadPauseMenuItem").hidden =
      state != DOWNLOAD_DOWNLOADING;

    contextMenu.querySelector(".downloadResumeMenuItem").hidden =
      state != DOWNLOAD_PAUSED;

    // Only show "unblock" for blocked (dirty) items that have not been
    // confirmed and have temporary data:
    contextMenu.querySelector(".downloadUnblockMenuItem").hidden =
      state != DOWNLOAD_DIRTY || !element.classList.contains("temporary-block");

    // Can only remove finished/failed/canceled/blocked downloads.
    contextMenu.querySelector(".downloadRemoveFromHistoryMenuItem").hidden = ![
      DOWNLOAD_FINISHED,
      DOWNLOAD_FAILED,
      DOWNLOAD_CANCELED,
      DOWNLOAD_BLOCKED_PARENTAL,
      DOWNLOAD_DIRTY,
      DOWNLOAD_BLOCKED_POLICY,
    ].includes(state);

    // Can reveal downloads with data on the file system using the relevant OS
    // tool (Explorer, Finder, appropriate Linux file system viewer):
    contextMenu.querySelector(".downloadShowMenuItem").hidden =
      ![
        DOWNLOAD_NOTSTARTED,
        DOWNLOAD_DOWNLOADING,
        DOWNLOAD_FINISHED,
        DOWNLOAD_PAUSED,
      ].includes(state) ||
      (state == DOWNLOAD_FINISHED && !element.hasAttribute("exists"));

    // Show the separator if we're showing either unblock or reveal menu items.
    contextMenu.querySelector(".downloadCommandsSeparator").hidden =
      contextMenu.querySelector(".downloadUnblockMenuItem").hidden &&
      contextMenu.querySelector(".downloadShowMenuItem").hidden;

    let download = element._shell.download;
    let mimeInfo = lazy.DownloadsCommon.getMimeInfo(download);
    let { preferredAction, useSystemDefault, defaultDescription } = mimeInfo
      ? mimeInfo
      : {};

    // Hide the "Delete" item if there's no file data to delete.
    contextMenu.querySelector(".downloadDeleteFileMenuItem").hidden =
      download.deleted ||
      !(download.target?.exists || download.target?.partFileExists);

    // Hide the "Go To Download Page" item if there's no referrer. Ideally the
    // Downloads API will require a referrer (see bug 1723712) to create a
    // download, but this fallback will ensure any failures aren't user facing.
    contextMenu.querySelector(
      ".downloadOpenReferrerMenuItem"
    ).hidden = !download.source.referrerInfo?.originalReferrer;

    // Hide the "use system viewer" and "always use system viewer" items
    // if the feature is disabled or this download doesn't support it:
    let useSystemViewerItem = contextMenu.querySelector(
      ".downloadUseSystemDefaultMenuItem"
    );
    let alwaysUseSystemViewerItem = contextMenu.querySelector(
      ".downloadAlwaysUseSystemDefaultMenuItem"
    );
    let canViewInternally = element.hasAttribute("viewable-internally");
    useSystemViewerItem.hidden =
      !lazy.DownloadsCommon.openInSystemViewerItemEnabled ||
      !canViewInternally ||
      !download.target?.exists;

    alwaysUseSystemViewerItem.hidden =
      !lazy.DownloadsCommon.alwaysOpenInSystemViewerItemEnabled ||
      !canViewInternally;

    // Set menuitem labels to display the system viewer's name. Stop the l10n
    // mutation observer temporarily since we're going to synchronously
    // translate the elements to avoid translation delay. See bug 1737951 & bug
    // 1746748. This can be simplified when they're resolved.
    try {
      document.l10n.pauseObserving();
      // Handler descriptions longer than 40 characters will be skipped to avoid
      // unreasonably stretching the context menu.
      if (defaultDescription && defaultDescription.length < 40) {
        document.l10n.setAttributes(
          useSystemViewerItem,
          "downloads-cmd-use-system-default-named",
          { handler: defaultDescription }
        );
        document.l10n.setAttributes(
          alwaysUseSystemViewerItem,
          "downloads-cmd-always-use-system-default-named",
          { handler: defaultDescription }
        );
      } else {
        // In the unlikely event that defaultDescription is somehow missing/invalid,
        // fall back to the static "Open In System Viewer" label.
        document.l10n.setAttributes(
          useSystemViewerItem,
          "downloads-cmd-use-system-default"
        );
        document.l10n.setAttributes(
          alwaysUseSystemViewerItem,
          "downloads-cmd-always-use-system-default"
        );
      }
    } finally {
      document.l10n.resumeObserving();
    }
    document.l10n.translateElements([
      useSystemViewerItem,
      alwaysUseSystemViewerItem,
    ]);

    // If non default mime-type or cannot be opened internally, display
    // "always open similar files" item instead so that users can add a new
    // mimetype to about:preferences table and set to open with system default.
    // Only appear if browser.download.improvements_to_download_panel is enabled.
    let alwaysOpenSimilarFilesItem = contextMenu.querySelector(
      ".downloadAlwaysOpenSimilarFilesMenuItem"
    );

    /**
     * In HelperAppDlg.jsm, we determine whether or not an "always open..." checkbox
     * should appear in the unknownContentType window. Here, we use similar checks to
     * determine if we should show the "always open similar files" context menu item.
     *
     * Note that we also read the content type using mimeInfo to detect better and available
     * mime types, given a file extension. Some sites default to "application/octet-stream",
     * further limiting what file types can be added to about:preferences, even for file types
     * that are in fact capable of being handled with a default application.
     *
     * There are also cases where download.contentType is undefined (ex. when opening
     * the context menu on a previously downloaded item via download history).
     * Using mimeInfo ensures that content type exists and prevents intermittence.
     */
    //
    let filename = PathUtils.filename(download.target.path);

    let isExemptExecutableExtension = Services.policies.isExemptExecutableExtension(
      download.source.originalUrl || download.source.url,
      filename?.split(".").at(-1)
    );

    let shouldNotRememberChoice =
      !mimeInfo?.type ||
      mimeInfo.type === "application/octet-stream" ||
      mimeInfo.type === "application/x-msdownload" ||
      mimeInfo.type === "application/x-msdos-program" ||
      (lazy.gReputationService.isExecutable(filename) &&
        !isExemptExecutableExtension) ||
      (mimeInfo.type === "text/plain" &&
        lazy.gReputationService.isBinary(download.target.path));

    if (DownloadsViewUI.improvementsIsOn && !canViewInternally) {
      alwaysOpenSimilarFilesItem.hidden =
        state !== DOWNLOAD_FINISHED || shouldNotRememberChoice;
    } else {
      alwaysOpenSimilarFilesItem.hidden = true;
    }

    // Update checkbox for "always open..." options.
    if (preferredAction === useSystemDefault) {
      alwaysUseSystemViewerItem.setAttribute("checked", "true");
      alwaysOpenSimilarFilesItem.setAttribute("checked", "true");
    } else {
      alwaysUseSystemViewerItem.removeAttribute("checked");
      alwaysOpenSimilarFilesItem.removeAttribute("checked");
    }
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  DownloadsViewUI,
  "improvementsIsOn",
  "browser.download.improvements_to_download_panel",
  false
);

XPCOMUtils.defineLazyPreferenceGetter(
  DownloadsViewUI,
  "clearHistoryOnDelete",
  "browser.download.clearHistoryOnDelete",
  0
);

DownloadsViewUI.BaseView = class {
  canClearDownloads(nodeContainer) {
    // Downloads can be cleared if there's at least one removable download in
    // the list (either a history download or a completed session download).
    // Because history downloads are always removable and are listed after the
    // session downloads, check from bottom to top.
    for (let elt = nodeContainer.lastChild; elt; elt = elt.previousSibling) {
      // Stopped, paused, and failed downloads with partial data are removed.
      let download = elt._shell.download;
      if (download.stopped && !(download.canceled && download.hasPartialData)) {
        return true;
      }
    }
    return false;
  }
};

/**
 * A download element shell is responsible for handling the commands and the
 * displayed data for a single element that uses the "download.xml" binding.
 *
 * The information to display is obtained through the associated Download object
 * from the JavaScript API for downloads, and commands are executed using a
 * combination of Download methods and DownloadsCommon.jsm helper functions.
 *
 * Specialized versions of this shell must be defined, and they are required to
 * implement the "download" property or getter. Currently these objects are the
 * HistoryDownloadElementShell and the DownloadsViewItem for the panel. The
 * history view may use a HistoryDownload object in place of a Download object.
 */
DownloadsViewUI.DownloadElementShell = function() {};

DownloadsViewUI.DownloadElementShell.prototype = {
  /**
   * The richlistitem for the download, initialized by the derived object.
   */
  element: null,

  /**
   * Manages the "active" state of the shell. By default all the shells are
   * inactive, thus their UI is not updated. They must be activated when
   * entering the visible area.
   */
  ensureActive() {
    if (!this._active) {
      this._active = true;
      this.connect();
      this.onChanged();
    }
  },
  get active() {
    return !!this._active;
  },

  connect() {
    let document = this.element.ownerDocument;
    let downloadListItemFragment = gDownloadListItemFragments.get(document);
    // When changing the markup within the fragment, please ensure that
    // the functions within DownloadsView still operate correctly.
    if (!downloadListItemFragment) {
      let MozXULElement = document.defaultView.MozXULElement;
      downloadListItemFragment = MozXULElement.parseXULToFragment(`
        <hbox class="downloadMainArea" flex="1" align="center">
          <image class="downloadTypeIcon" validate="always"/>
          <vbox class="downloadContainer" flex="1" pack="center">
            <description class="downloadTarget" crop="center"/>
            <description class="downloadDetails downloadDetailsNormal"
                         crop="end"/>
            <description class="downloadDetails downloadDetailsHover"
                         crop="end"/>
            <description class="downloadDetails downloadDetailsButtonHover"
                         crop="end"/>
          </vbox>
          <image class="downloadBlockedBadge" />
        </hbox>
        <button class="downloadButton"/>
      `);
      gDownloadListItemFragments.set(document, downloadListItemFragment);
    }
    this.element.setAttribute("active", true);
    this.element.setAttribute("orient", "horizontal");
    this.element.addEventListener("click", ev => {
      ev.target.ownerGlobal.DownloadsView.onDownloadClick(ev);
    });
    this.element.appendChild(
      document.importNode(downloadListItemFragment, true)
    );
    let downloadButton = this.element.querySelector(".downloadButton");
    downloadButton.addEventListener("command", function(event) {
      event.target.ownerGlobal.DownloadsView.onDownloadButton(event);
    });
    for (let [propertyName, selector] of [
      ["_downloadTypeIcon", ".downloadTypeIcon"],
      ["_downloadTarget", ".downloadTarget"],
      ["_downloadDetailsNormal", ".downloadDetailsNormal"],
      ["_downloadDetailsHover", ".downloadDetailsHover"],
      ["_downloadDetailsButtonHover", ".downloadDetailsButtonHover"],
      ["_downloadButton", ".downloadButton"],
    ]) {
      this[propertyName] = this.element.querySelector(selector);
    }

    // HTML elements can be created directly without using parseXULToFragment.
    let progress = (this._downloadProgress = document.createElementNS(
      HTML_NS,
      "progress"
    ));
    progress.className = "downloadProgress";
    progress.setAttribute("max", "100");
    this._downloadTarget.insertAdjacentElement("afterend", progress);
  },

  /**
   * URI string for the file type icon displayed in the download element.
   */
  get image() {
    if (!this.download.target.path) {
      // Old history downloads may not have a target path.
      return "moz-icon://.unknown?size=32";
    }

    // When a download that was previously in progress finishes successfully, it
    // means that the target file now exists and we can extract its specific
    // icon, for example from a Windows executable. To ensure that the icon is
    // reloaded, however, we must change the URI used by the XUL image element,
    // for example by adding a query parameter. This only works if we add one of
    // the parameters explicitly supported by the nsIMozIconURI interface.
    return (
      "moz-icon://" +
      this.download.target.path +
      "?size=32" +
      (this.download.succeeded ? "&state=normal" : "")
    );
  },

  get browserWindow() {
    return lazy.BrowserWindowTracker.getTopWindow();
  },

  /**
   * Updates the display name and icon.
   *
   * @param displayName
   *        This is usually the full file name of the download without the path.
   * @param icon
   *        URL of the icon to load, generally from the "image" property.
   */
  showDisplayNameAndIcon(displayName, icon) {
    if (displayName.l10n) {
      let document = this.element.ownerDocument;
      document.l10n.setAttributes(
        this._downloadTarget,
        displayName.l10n.id,
        displayName.l10n.args
      );
    } else {
      this._downloadTarget.setAttribute("value", displayName);
      this._downloadTarget.setAttribute("tooltiptext", displayName);
    }
    this._downloadTypeIcon.setAttribute("src", icon);
  },

  /**
   * Updates the displayed progress bar.
   *
   * @param mode
   *        Either "normal" or "undetermined".
   * @param value
   *        Percentage of the progress bar to display, from 0 to 100.
   * @param paused
   *        True to display the progress bar style for paused downloads.
   */
  showProgress(mode, value, paused) {
    if (mode == "undetermined") {
      this._downloadProgress.removeAttribute("value");
    } else {
      this._downloadProgress.setAttribute("value", value);
    }
    this._downloadProgress.toggleAttribute("paused", !!paused);
  },

  /**
   * Updates the full status line.
   *
   * @param status
   *        Status line of the Downloads Panel or the Downloads View.
   * @param hoverStatus
   *        Label to show in the Downloads Panel when the mouse pointer is over
   *        the main area of the item. If not specified, this will be the same
   *        as the status line. This is ignored in the Downloads View. Type is
   *        either l10n object or string literal.
   */
  showStatus(status, hoverStatus = status) {
    let document = this.element.ownerDocument;
    if (status?.l10n) {
      document.l10n.setAttributes(
        this._downloadDetailsNormal,
        status.l10n.id,
        status.l10n.args
      );
    } else {
      this._downloadDetailsNormal.removeAttribute("data-l10n-id");
      this._downloadDetailsNormal.setAttribute("value", status);
      this._downloadDetailsNormal.setAttribute("tooltiptext", status);
    }
    if (hoverStatus?.l10n) {
      hoverStatus.l10n.id
        ? document.l10n.setAttributes(
            this._downloadDetailsHover,
            hoverStatus.l10n.id,
            hoverStatus.l10n.args
          )
        : document.l10n.setAttributes(
            this._downloadDetailsHover,
            hoverStatus.l10n
          );
    } else {
      this._downloadDetailsHover.removeAttribute("data-l10n-id");
      this._downloadDetailsHover.setAttribute("value", hoverStatus);
    }
  },

  /**
   * Updates the status line combining the given state label with other labels.
   *
   * @param stateLabel
   *        Label representing the state of the download, for example "Failed".
   *        In the Downloads Panel, this is the only text displayed when the
   *        the mouse pointer is not over the main area of the item. In the
   *        Downloads View, this label is combined with the host and date, for
   *        example "Failed - example.com - 1:45 PM".
   * @param hoverStatus
   *        Label to show in the Downloads Panel when the mouse pointer is over
   *        the main area of the item. If not specified, this will be the
   *        state label combined with the host and date. This is ignored in the
   *        Downloads View. Type is either l10n object or string literal.
   */
  showStatusWithDetails(stateLabel, hoverStatus) {
    if (stateLabel.l10n) {
      this.showStatus(stateLabel, hoverStatus);
      return;
    }
    let [displayHost] = lazy.DownloadUtils.getURIHost(this.download.source.url);
    let [displayDate] = lazy.DownloadUtils.getReadableDates(
      new Date(this.download.endTime)
    );

    let firstPart = lazy.DownloadsCommon.strings.statusSeparator(
      stateLabel,
      displayHost
    );
    let fullStatus = lazy.DownloadsCommon.strings.statusSeparator(
      firstPart,
      displayDate
    );

    if (!this.isPanel) {
      this.showStatus(fullStatus);
    } else {
      this.showStatus(stateLabel, hoverStatus || fullStatus);
    }
  },

  /**
   * Updates the main action button and makes it visible.
   *
   * @param type
   *        One of the presets defined in gDownloadElementButtons.
   */
  showButton(type) {
    let {
      commandName,
      l10nId,
      descriptionL10nId,
      panelL10nId,
      iconClass,
    } = gDownloadElementButtons[type];

    this.buttonCommandName = commandName;
    let stringId = this.isPanel ? panelL10nId : l10nId;
    let document = this.element.ownerDocument;
    document.l10n.setAttributes(this._downloadButton, stringId);
    if (this.isPanel && descriptionL10nId) {
      document.l10n.setAttributes(
        this._downloadDetailsButtonHover,
        descriptionL10nId
      );
    }
    this._downloadButton.setAttribute("class", "downloadButton " + iconClass);
    this._downloadButton.removeAttribute("hidden");
  },

  hideButton() {
    this._downloadButton.hidden = true;
  },

  lastEstimatedSecondsLeft: Infinity,

  /**
   * This is called when a major state change occurs in the download, but is not
   * called for every progress update in order to improve performance.
   */
  _updateState() {
    this.showDisplayNameAndIcon(
      DownloadsViewUI.getDisplayName(this.download),
      this.image
    );
    this.element.setAttribute(
      "state",
      lazy.DownloadsCommon.stateOfDownload(this.download)
    );

    if (!this.download.stopped) {
      // When the download becomes in progress, we make all the major changes to
      // the user interface here. The _updateStateInner function takes care of
      // displaying the right button type for all other state changes.
      this.showButton("cancel");

      // If there was a verdict set but the download is running we can assume
      // that the verdict has been overruled and can be removed.
      this.element.removeAttribute("verdict");
    }

    // Since state changed, reset the time left estimation.
    this.lastEstimatedSecondsLeft = Infinity;

    this._updateStateInner();
  },

  /**
   * This is called for all changes in the download, including progress updates.
   * For major state changes, _updateState is called first, but several elements
   * are still updated here. When the download is in progress, this function
   * takes a faster path with less element updates to improve performance.
   */
  _updateStateInner() {
    let progressPaused = false;

    this.element.classList.toggle(
      "openWhenFinished",
      DownloadsViewUI.improvementsIsOn && !this.download.stopped
    );

    if (!this.download.stopped) {
      // The download is in progress, so we don't change the button state
      // because the _updateState function already did it. We still need to
      // update all elements that may change during the download.
      let totalBytes = this.download.hasProgress
        ? this.download.totalBytes
        : -1;
      let [
        status,
        newEstimatedSecondsLeft,
      ] = lazy.DownloadUtils.getDownloadStatus(
        this.download.currentBytes,
        totalBytes,
        this.download.speed,
        this.lastEstimatedSecondsLeft
      );
      this.lastEstimatedSecondsLeft = newEstimatedSecondsLeft;

      if (
        DownloadsViewUI.improvementsIsOn &&
        this.download.launchWhenSucceeded
      ) {
        status = lazy.DownloadUtils.getFormattedTimeStatus(
          newEstimatedSecondsLeft
        );
      }
      let hoverStatus = DownloadsViewUI.improvementsIsOn
        ? {
            l10n: "downloading-file-click-to-open",
          }
        : undefined;
      this.showStatus(status, hoverStatus);
    } else {
      let verdict = "";

      // The download is not in progress, so we update the user interface based
      // on other properties. The order in which we check the properties of the
      // Download object is the same used by stateOfDownload.
      if (this.download.deleted) {
        this.showDeletedOrMissing();
      } else if (this.download.succeeded) {
        lazy.DownloadsCommon.log(
          "_updateStateInner, target exists? ",
          this.download.target.path,
          this.download.target.exists
        );
        if (this.download.target.exists) {
          // This is a completed download, and the target file still exists.
          this.element.setAttribute("exists", "true");

          this.element.toggleAttribute(
            "viewable-internally",
            lazy.DownloadIntegration.shouldViewDownloadInternally(
              lazy.DownloadsCommon.getMimeInfo(this.download)?.type
            )
          );

          let sizeWithUnits = DownloadsViewUI.getSizeWithUnits(this.download);
          if (this.isPanel) {
            // In the Downloads Panel, we show the file size after the state
            // label, for example "Completed - 1.5 MB". When the pointer is over
            // the main area of the item, this label is replaced with a
            // description of the default action, which opens the file.
            let status = lazy.DownloadsCommon.strings.stateCompleted;
            if (sizeWithUnits) {
              status = lazy.DownloadsCommon.strings.statusSeparator(
                status,
                sizeWithUnits
              );
            }
            this.showStatus(status, { l10n: "downloads-open-file" });
          } else {
            // In the Downloads View, we show the file size in place of the
            // state label, for example "1.5 MB - example.com - 1:45 PM".
            this.showStatusWithDetails(
              sizeWithUnits || lazy.DownloadsCommon.strings.sizeUnknown
            );
          }
          this.showButton("show");
        } else {
          // This is a completed download, but the target file does not exist
          // anymore, so the main action of opening the file is unavailable.
          this.showDeletedOrMissing();
        }
      } else if (this.download.error) {
        if (this.download.error.becauseBlockedByParentalControls) {
          // This download was blocked permanently by parental controls.
          this.showStatusWithDetails(
            lazy.DownloadsCommon.strings.stateBlockedParentalControls
          );
          this.hideButton();
        } else if (this.download.error.becauseBlockedByReputationCheck) {
          verdict = this.download.error.reputationCheckVerdict;
          let hover = "";
          if (!this.download.hasBlockedData) {
            // This download was blocked permanently by reputation check.
            this.hideButton();
          } else if (this.isPanel) {
            // This download was blocked temporarily by reputation check. In the
            // Downloads Panel, a subview can be used to remove the file or open
            // the download anyways.
            this.showButton("subviewOpenOrRemoveFile");
            hover = { l10n: "downloads-show-more-information" };
          } else {
            // This download was blocked temporarily by reputation check. In the
            // Downloads View, the interface depends on the threat severity.
            switch (verdict) {
              case lazy.Downloads.Error.BLOCK_VERDICT_UNCOMMON:
              case lazy.Downloads.Error.BLOCK_VERDICT_INSECURE:
              case lazy.Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED:
                // Keep the option the user chose on the save dialogue
                if (this.download.launchWhenSucceeded) {
                  this.showButton("askOpenOrRemoveFile");
                } else {
                  this.showButton("askRemoveFileOrAllow");
                }
                break;
              case lazy.Downloads.Error.BLOCK_VERDICT_DOWNLOAD_SPAM:
                this.showButton("askRemoveFileOrAllow");
                break;
              default:
                // Assume Downloads.Error.BLOCK_VERDICT_MALWARE
                this.showButton("removeFile");
                break;
            }
          }
          this.showStatusWithDetails(this.rawBlockedTitleAndDetails[0], hover);
        } else {
          // This download failed without being blocked, and can be restarted.
          this.showStatusWithDetails(lazy.DownloadsCommon.strings.stateFailed);
          this.showButton("retry");
        }
      } else if (this.download.canceled) {
        if (this.download.hasPartialData) {
          // This download was paused. The main action button will cancel the
          // download, and in both the Downloads Panel and the Downlods View the
          // status includes the size, for example "Paused - 1.1 MB".
          let totalBytes = this.download.hasProgress
            ? this.download.totalBytes
            : -1;
          let transfer = lazy.DownloadUtils.getTransferTotal(
            this.download.currentBytes,
            totalBytes
          );
          this.showStatus(
            lazy.DownloadsCommon.strings.statusSeparatorBeforeNumber(
              lazy.DownloadsCommon.strings.statePaused,
              transfer
            )
          );
          this.showButton("cancel");
          progressPaused = true;
        } else {
          // This download was canceled.
          this.showStatusWithDetails(
            lazy.DownloadsCommon.strings.stateCanceled
          );
          this.showButton("retry");
        }
      } else {
        // This download was added to the global list before it started. While
        // we still support this case, at the moment it can only be triggered by
        // internally developed add-ons and regression tests, and should not
        // happen unless there is a bug. This means the stateStarting string can
        // probably be removed when converting the localization to Fluent.
        this.showStatus(lazy.DownloadsCommon.strings.stateStarting);
        this.showButton("cancel");
      }

      // These attributes are only set in this slower code path, because they
      // are irrelevant for downloads that are in progress.
      if (verdict) {
        this.element.setAttribute("verdict", verdict);
      } else {
        this.element.removeAttribute("verdict");
      }

      this.element.classList.toggle(
        "temporary-block",
        !!this.download.hasBlockedData
      );
    }

    // These attributes are set in all code paths, because they are relevant for
    // downloads that are in progress and for other states.
    if (this.download.hasProgress) {
      this.showProgress("normal", this.download.progress, progressPaused);
    } else {
      this.showProgress("undetermined", 100, progressPaused);
    }
  },

  /**
   * Returns [title, [details1, details2]] for blocked downloads.
   * The title or details could be raw strings or l10n objects.
   */
  get rawBlockedTitleAndDetails() {
    let s = lazy.DownloadsCommon.strings;
    if (
      !this.download.error ||
      !this.download.error.becauseBlockedByReputationCheck
    ) {
      return [null, null];
    }
    switch (this.download.error.reputationCheckVerdict) {
      case lazy.Downloads.Error.BLOCK_VERDICT_UNCOMMON:
        return [s.blockedUncommon2, [s.unblockTypeUncommon2, s.unblockTip2]];
      case lazy.Downloads.Error.BLOCK_VERDICT_INSECURE:
        return [
          s.blockedPotentiallyInsecure,
          [s.unblockInsecure, s.unblockTip2],
        ];
      case lazy.Downloads.Error.BLOCK_VERDICT_POTENTIALLY_UNWANTED:
        return [
          s.blockedPotentiallyUnwanted,
          [s.unblockTypePotentiallyUnwanted2, s.unblockTip2],
        ];
      case lazy.Downloads.Error.BLOCK_VERDICT_MALWARE:
        return [s.blockedMalware, [s.unblockTypeMalware, s.unblockTip2]];

      case lazy.Downloads.Error.BLOCK_VERDICT_DOWNLOAD_SPAM:
        let title = {
          id: "downloads-files-not-downloaded",
          args: {
            num: this.download.blockedDownloadsCount,
          },
        };
        let details = {
          id: "downloads-blocked-download-detailed-info",
          args: { url: DownloadsViewUI.getStrippedUrl(this.download) },
        };
        return [{ l10n: title }, [{ l10n: details }, null]];
    }
    throw new Error(
      "Unexpected reputationCheckVerdict: " +
        this.download.error.reputationCheckVerdict
    );
  },

  showDeletedOrMissing() {
    this.element.removeAttribute("exists");
    let label =
      lazy.DownloadsCommon.strings[
        this.download.deleted ? "fileDeleted" : "fileMovedOrMissing"
      ];
    this.showStatusWithDetails(label, label);
    this.hideButton();
  },

  /**
   * Shows the appropriate unblock dialog based on the verdict, and executes the
   * action selected by the user in the dialog, which may involve unblocking,
   * opening or removing the file.
   *
   * @param window
   *        The window to which the dialog should be anchored.
   * @param dialogType
   *        Can be "unblock", "chooseUnblock", or "chooseOpen".
   */
  confirmUnblock(window, dialogType) {
    lazy.DownloadsCommon.confirmUnblockDownload({
      verdict: this.download.error.reputationCheckVerdict,
      window,
      dialogType,
    })
      .then(action => {
        if (action == "open") {
          return this.unblockAndOpenDownload();
        } else if (action == "unblock") {
          return this.download.unblock();
        } else if (action == "confirmBlock") {
          return this.download.confirmBlock();
        }
        return Promise.resolve();
      })
      .catch(Cu.reportError);
  },

  /**
   * Unblocks the downloaded file and opens it.
   *
   * @return A promise that's resolved after the file has been opened.
   */
  unblockAndOpenDownload() {
    return this.download.unblock().then(() => this.downloadsCmd_open());
  },

  unblockAndSave() {
    return this.download.unblock();
  },
  /**
   * Returns the name of the default command to use for the current state of the
   * download, when there is a double click or another default interaction. If
   * there is no default command for the current state, returns an empty string.
   * The commands are implemented as functions on this object or derived ones.
   */
  get currentDefaultCommandName() {
    switch (lazy.DownloadsCommon.stateOfDownload(this.download)) {
      case lazy.DownloadsCommon.DOWNLOAD_NOTSTARTED:
        return "downloadsCmd_cancel";
      case lazy.DownloadsCommon.DOWNLOAD_FAILED:
      case lazy.DownloadsCommon.DOWNLOAD_CANCELED:
        return "downloadsCmd_retry";
      case lazy.DownloadsCommon.DOWNLOAD_PAUSED:
        return "downloadsCmd_pauseResume";
      case lazy.DownloadsCommon.DOWNLOAD_FINISHED:
        return "downloadsCmd_open";
      case lazy.DownloadsCommon.DOWNLOAD_BLOCKED_PARENTAL:
        return "downloadsCmd_openReferrer";
      case lazy.DownloadsCommon.DOWNLOAD_DIRTY:
        return "downloadsCmd_showBlockedInfo";
    }
    return "";
  },

  /**
   * Returns true if the specified command can be invoked on the current item.
   * The commands are implemented as functions on this object or derived ones.
   *
   * @param aCommand
   *        Name of the command to check, for example "downloadsCmd_retry".
   */
  isCommandEnabled(aCommand) {
    switch (aCommand) {
      case "downloadsCmd_retry":
        return this.download.canceled || this.download.error;
      case "downloadsCmd_pauseResume":
        return this.download.hasPartialData && !this.download.error;
      case "downloadsCmd_openReferrer":
        return (
          !!this.download.source.referrerInfo &&
          !!this.download.source.referrerInfo.originalReferrer
        );
      case "downloadsCmd_confirmBlock":
      case "downloadsCmd_chooseUnblock":
      case "downloadsCmd_chooseOpen":
      case "downloadsCmd_unblock":
      case "downloadsCmd_unblockAndSave":
      case "downloadsCmd_unblockAndOpen":
        return this.download.hasBlockedData;
      case "downloadsCmd_cancel":
        return this.download.hasPartialData || !this.download.stopped;
      case "downloadsCmd_open":
      case "downloadsCmd_open:current":
      case "downloadsCmd_open:tab":
      case "downloadsCmd_open:tabshifted":
      case "downloadsCmd_open:window":
      case "downloadsCmd_alwaysOpenSimilarFiles":
        // This property is false if the download did not succeed.
        return this.download.target.exists;

      case "downloadsCmd_show":
      case "downloadsCmd_deleteFile":
        let { target } = this.download;
        return (
          !this.download.deleted && (target.exists || target.partFileExists)
        );

      case "downloadsCmd_delete":
      case "cmd_delete":
        // We don't want in-progress downloads to be removed accidentally.
        return this.download.stopped;
      case "downloadsCmd_openInSystemViewer":
      case "downloadsCmd_alwaysOpenInSystemViewer":
        return lazy.DownloadIntegration.shouldViewDownloadInternally(
          lazy.DownloadsCommon.getMimeInfo(this.download)?.type
        );
    }
    return DownloadsViewUI.isCommandName(aCommand) && !!this[aCommand];
  },

  doCommand(aCommand) {
    // split off an optional command "modifier" into an argument,
    // e.g. "downloadsCmd_open:window"
    let [command, modifier] = aCommand.split(":");
    if (DownloadsViewUI.isCommandName(command)) {
      this[command](modifier);
    }
  },

  onButton() {
    this.doCommand(this.buttonCommandName);
  },

  downloadsCmd_cancel() {
    // This is the correct way to avoid race conditions when cancelling.
    this.download.cancel().catch(() => {});
    this.download
      .removePartialData()
      .catch(Cu.reportError)
      .finally(() => this.download.target.refresh());
  },

  downloadsCmd_confirmBlock() {
    this.download.confirmBlock().catch(Cu.reportError);
  },

  downloadsCmd_open(openWhere = "tab") {
    lazy.DownloadsCommon.openDownload(this.download, {
      openWhere,
    });
  },

  downloadsCmd_openReferrer() {
    this.element.ownerGlobal.openURL(
      this.download.source.referrerInfo.originalReferrer
    );
  },

  downloadsCmd_pauseResume() {
    if (this.download.stopped) {
      this.download.start();
    } else {
      this.download.cancel();
    }
  },

  downloadsCmd_show() {
    let file = new lazy.FileUtils.File(this.download.target.path);
    lazy.DownloadsCommon.showDownloadedFile(file);
  },

  downloadsCmd_retry() {
    if (this.download.start) {
      // Errors when retrying are already reported as download failures.
      this.download.start().catch(() => {});
      return;
    }

    let window = this.browserWindow || this.element.ownerGlobal;
    let document = window.document;

    // Do not suggest a file name if we don't know the original target.
    let targetPath = this.download.target.path
      ? PathUtils.filename(this.download.target.path)
      : null;
    window.DownloadURL(this.download.source.url, targetPath, document);
  },

  downloadsCmd_delete() {
    // Alias for the 'cmd_delete' command, because it may clash with another
    // controller which causes unexpected behavior as different codepaths claim
    // ownership.
    this.cmd_delete();
  },

  cmd_delete() {
    lazy.DownloadsCommon.deleteDownload(this.download).catch(Cu.reportError);
  },

  async downloadsCmd_deleteFile() {
    // Remove the download from the session and history downloads, delete part files.
    await lazy.DownloadsCommon.deleteDownloadFiles(
      this.download,
      DownloadsViewUI.clearHistoryOnDelete
    );
  },

  downloadsCmd_openInSystemViewer() {
    // For this interaction only, pass a flag to override the preferredAction for this
    // mime-type and open using the system viewer
    lazy.DownloadsCommon.openDownload(this.download, {
      useSystemDefault: true,
    }).catch(Cu.reportError);
  },

  downloadsCmd_alwaysOpenInSystemViewer() {
    // this command toggles between setting preferredAction for this mime-type to open
    // using the system viewer, or to open the file in browser.
    const mimeInfo = lazy.DownloadsCommon.getMimeInfo(this.download);
    if (!mimeInfo) {
      throw new Error(
        "Can't open download with unknown mime-type in system viewer"
      );
    }
    if (mimeInfo.preferredAction !== mimeInfo.useSystemDefault) {
      // User has selected to open this mime-type with the system viewer from now on
      lazy.DownloadsCommon.log(
        "downloadsCmd_alwaysOpenInSystemViewer command for download: ",
        this.download,
        "switching to use system default for " + mimeInfo.type
      );
      mimeInfo.preferredAction = mimeInfo.useSystemDefault;
      mimeInfo.alwaysAskBeforeHandling = false;
    } else {
      lazy.DownloadsCommon.log(
        "downloadsCmd_alwaysOpenInSystemViewer command for download: ",
        this.download,
        "currently uses system default, switching to handleInternally"
      );
      // User has selected to not open this mime-type with the system viewer
      mimeInfo.preferredAction = mimeInfo.handleInternally;
    }
    lazy.handlerSvc.store(mimeInfo);
    lazy.DownloadsCommon.openDownload(this.download).catch(Cu.reportError);
  },

  downloadsCmd_alwaysOpenSimilarFiles() {
    const mimeInfo = lazy.DownloadsCommon.getMimeInfo(this.download);
    if (!mimeInfo) {
      throw new Error("Can't open download with unknown mime-type");
    }

    // User has selected to always open this mime-type from now on and will add this
    // mime-type to our preferences table with the system default option. Open the
    // file immediately after selecting the menu item like alwaysOpenInSystemViewer.
    if (mimeInfo.preferredAction !== mimeInfo.useSystemDefault) {
      mimeInfo.preferredAction = mimeInfo.useSystemDefault;
      lazy.handlerSvc.store(mimeInfo);
      lazy.DownloadsCommon.openDownload(this.download).catch(Cu.reportError);
    } else {
      // Otherwise, if user unchecks this option after already enabling it from the
      // context menu, resort to saveToDisk.
      mimeInfo.preferredAction = mimeInfo.saveToDisk;
      lazy.handlerSvc.store(mimeInfo);
    }
  },
};
