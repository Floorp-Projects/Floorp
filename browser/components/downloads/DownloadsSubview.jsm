/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "DownloadsSubview",
];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Downloads",
                                  "resource://gre/modules/Downloads.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadsCommon",
                                  "resource:///modules/DownloadsCommon.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "DownloadsViewUI",
                                  "resource:///modules/DownloadsViewUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");

let gPanelViewInstances = new WeakMap();
const kEvents = ["ViewShowing", "ViewHiding", "click", "command"];
const kRefreshBatchSize = 10;
const kMaxWaitForIdleMs = 200;
XPCOMUtils.defineLazyGetter(this, "kButtonLabels", () => {
  return {
    show: DownloadsCommon.strings[AppConstants.platform == "macosx" ? "showMacLabel" : "showLabel"],
    open: DownloadsCommon.strings.openFileLabel,
    retry: DownloadsCommon.strings.retryLabel,
  };
});

class DownloadsSubview extends DownloadsViewUI.BaseView {
  constructor(panelview) {
    super();
    this.document = panelview.ownerDocument;
    this.window = panelview.ownerGlobal;

    this.context = "panelDownloadsContextMenu";

    this.panelview = panelview;
    this.container = this.document.getElementById("panelMenu_downloadsMenu");
    while (this.container.lastChild) {
      this.container.lastChild.remove();
    }
    this.panelview.addEventListener("click", DownloadsSubview.onClick);
    this.panelview.addEventListener("ViewHiding", DownloadsSubview.onViewHiding);

    this._viewItemsForDownloads = new WeakMap();

    let contextMenu = this.document.getElementById(this.context);
    if (!contextMenu) {
      contextMenu = this.document.getElementById("downloadsContextMenu").cloneNode(true);
      contextMenu.setAttribute("closemenu", "none");
      contextMenu.setAttribute("id", this.context);
      contextMenu.removeAttribute("onpopupshown");
      contextMenu.setAttribute("onpopupshowing",
        "DownloadsSubview.updateContextMenu(document.popupNode, this);");
      contextMenu.setAttribute("onpopuphidden", "DownloadsSubview.onContextMenuHidden(this);");
      let clearButton = contextMenu.querySelector("menuitem[command='downloadsCmd_clearDownloads']");
      clearButton.hidden = false;
      clearButton.previousSibling.hidden = true;
      contextMenu.querySelector("menuitem[command='cmd_delete']")
        .setAttribute("command", "downloadsCmd_delete");
    }
    this.panelview.appendChild(contextMenu);
    this.container.setAttribute("context", this.context);

    this._downloadsData = DownloadsCommon.getData(this.window, true, true, true);
    this._downloadsData.addView(this);
  }

  destructor(event) {
    this.panelview.removeEventListener("click", DownloadsSubview.onClick);
    this.panelview.removeEventListener("ViewHiding", DownloadsSubview.onViewHiding);
    this._downloadsData.removeView(this);
    gPanelViewInstances.delete(this);
    this.destroyed = true;
  }

  /**
   * DataView handler; invoked when a batch of downloads is being passed in -
   * usually when this instance is added as a view in the constructor.
   */
  onDownloadBatchStarting() {
    this.batchFragment = this.document.createDocumentFragment();
    this.window.clearTimeout(this._batchTimeout);
  }

  /**
   * DataView handler; invoked when the view stopped feeding its current list of
   * downloads.
   */
  onDownloadBatchEnded() {
    let {window} = this;
    window.clearTimeout(this._batchTimeout);
    let waitForMs = 200;
    if (this.batchFragment.childElementCount) {
      // Prepend the batch fragment.
      this.container.insertBefore(this.batchFragment, this.container.firstChild || null);
      waitForMs = 0;
    }
    // Wait a wee bit to dispatch the event, because another batch may start
    // right away.
    this._batchTimeout = window.setTimeout(() => {
      this._updateStatsFromDisk();
      this.panelview.dispatchEvent(new window.CustomEvent("DownloadsLoaded"));
    }, waitForMs);
    this.batchFragment = null;
  }

  /**
   * DataView handler; invoked when a new download is added to the list.
   *
   * @param {Download} download
   * @param {DOMNode}  [options.insertBefore]
   */
  onDownloadAdded(download, { insertBefore } = {}) {
    let shell = new DownloadsSubview.Button(download, this.document);
    this._viewItemsForDownloads.set(download, shell);
    // Triggger the code that update all attributes to match the downloads'
    // current state.
    shell.onChanged();

    // Since newest downloads are displayed at the top, either prepend the new
    // element or insert it after the one indicated by the insertBefore option.
    if (insertBefore) {
      this._viewItemsForDownloads.get(insertBefore)
          .element.insertAdjacentElement("afterend", shell.element);
    } else {
      (this.batchFragment || this.container).prepend(shell.element);
    }
  }

  /**
   * DataView Handler; invoked when the state of a download changed.
   *
   * @param {Download} download
   */
  onDownloadChanged(download) {
    this._viewItemsForDownloads.get(download).onChanged();
  }

  /**
   * DataView handler; invoked when a download is removed.
   *
   * @param {Download} download
   */
  onDownloadRemoved(download) {
    this._viewItemsForDownloads.get(download).element.remove();
  }

  /**
   * Schedule a refresh of the downloads that were added, which is mainly about
   * checking whether the target file still exists.
   * We're doing this during idle time and in chunks.
   */
  async _updateStatsFromDisk() {
    if (this._updatingStats)
      return;

    this._updatingStats = true;

    try {
      let idleOptions = { timeout: kMaxWaitForIdleMs };
      // Start with getting an idle moment to (maybe) refresh the list of downloads.
      await new Promise(resolve => this.window.requestIdleCallback(resolve), idleOptions);
      // In the meantime, this instance could have been destroyed, so take note.
      if (this.destroyed)
        return;

      let count = 0;
      for (let button of this.container.childNodes) {
        if (this.destroyed)
          return;
        if (!button._shell)
          continue;

        await button._shell.refresh();

        // Make sure to request a new idle moment every `kRefreshBatchSize` buttons.
        if (++count % kRefreshBatchSize === 0) {
          await new Promise(resolve => this.window.requestIdleCallback(resolve, idleOptions));
        }
      }
    } catch (ex) {
      Cu.reportError(ex);
    } finally {
      this._updatingStats = false;
    }
  }

  // ----- Static methods. -----

  /**
   * Perform all tasks necessary to be able to show a Downloads Subview.
   *
   * @param  {DOMWindow} window  Global window object.
   * @return {Promise}   Will resolve when all tasks are done.
   */
  static init(window) {
    return new Promise(resolve =>
      window.DownloadsOverlayLoader.ensureOverlayLoaded(window.DownloadsPanel.kDownloadsOverlay, resolve));
  }

  /**
   * Show the Downloads subview panel and listen for events that will trigger
   * building the dynamic part of the view.
   *
   * @param {DOMNode} anchor The button that was commanded to trigger this function.
   */
  static async show(anchor) {
    let document = anchor.ownerDocument;
    let window = anchor.ownerGlobal;
    await DownloadsSubview.init(window);

    let panelview = document.getElementById("PanelUI-downloads");
    anchor.setAttribute("closemenu", "none");
    gPanelViewInstances.set(panelview, new DownloadsSubview(panelview));

    // Since the DownloadsLists are propagated asynchronously, we need to wait a
    // little to get the view propagated.
    panelview.addEventListener("ViewShowing", event => {
      event.detail.addBlocker(new Promise(resolve => {
        panelview.addEventListener("DownloadsLoaded", resolve, { once: true });
      }));
    }, { once: true });

    window.PanelUI.showSubView("PanelUI-downloads", anchor);
  }

  /**
   * Handler method; reveal the users' download directory using the OS specific
   * method.
   */
  static async onShowDownloads() {
    // Retrieve the user's default download directory.
    let preferredDir = await Downloads.getPreferredDownloadsDirectory();
    DownloadsCommon.showDirectory(new FileUtils.File(preferredDir));
  }

  /**
   * Handler method; clear the list downloads finished and old(er) downloads,
   * just like in the Library.
   *
   * @param {DOMNode} button Button that was clicked to call this method.
   */
  static onClearDownloads(button) {
    let instance = gPanelViewInstances.get(button.closest("panelview"));
    if (!instance)
      return;
    instance._downloadsData.removeFinished();
    Cc["@mozilla.org/browser/download-history;1"]
      .getService(Ci.nsIDownloadHistory)
      .removeAllDownloads();
  }

  /**
   * Just before showing the context menu, anchored to a download item, we need
   * to set the right properties to make sure the right menu-items are visible.
   *
   * @param {DOMNode} button The Button the context menu will be anchored to.
   * @param {DOMNode} menu   The context menu.
   */
  static updateContextMenu(button, menu) {
    while (!button._shell) {
      button = button.parentNode;
    }
    menu.setAttribute("state", button.getAttribute("state"));
    if (button.hasAttribute("exists"))
      menu.setAttribute("exists", button.getAttribute("exists"));
    else
      menu.removeAttribute("exists");
    menu.classList.toggle("temporary-block", button.classList.contains("temporary-block"));
    for (let menuitem of menu.getElementsByTagName("menuitem")) {
      let command = menuitem.getAttribute("command");
      if (!command)
        continue;
      if (command == "downloadsCmd_clearDownloads") {
        menuitem.disabled = !DownloadsSubview.canClearDownloads(button);
      } else {
        menuitem.disabled = !button._shell.isCommandEnabled(command);
      }
    }

    // The menu anchorNode property is not available long enough to be used elsewhere,
    // so tack it another property name.
    menu._anchorNode = button;
  }

  /**
   * Right after the context menu was hidden, perform a bit of cleanup.
   *
   * @param {DOMNode} menu The context menu.
   */
  static onContextMenuHidden(menu) {
    delete menu._anchorNode;
  }

  /**
   * Static version of DownloadsSubview#canClearDownloads().
   *
   * @param {DOMNode} button Button that we'll use to find the right
   *                         DownloadsSubview instance.
   */
  static canClearDownloads(button) {
    let instance = gPanelViewInstances.get(button.closest("panelview"));
    if (!instance)
      return false;
    return instance.canClearDownloads(instance.container);
  }

  /**
   * Handler method; invoked when the Downloads panel is hidden and should be
   * torn down & cleaned up.
   *
   * @param {DOMEvent} event
   */
  static onViewHiding(event) {
    let instance = gPanelViewInstances.get(event.target);
    if (!instance)
      return;
    instance.destructor(event);
  }

  /**
   * Handler method; invoked when anything is clicked inside the Downloads panel.
   * Depending on the context, it will find the appropriate command to invoke.
   *
   * We don't have a command dispatcher registered for this view, so we don't go
   * through the goDoCommand path like we do for the other views.
   *
   * @param {DOMMouseEvent} event
   */
  static onClick(event) {
    // Middle clicks fall through and are regarded as left clicks.
    if (event.button > 1)
      return;

    let button = event.originalTarget;
    if (!button.hasAttribute || button.classList.contains("subviewbutton-back"))
      return;

    let command = "downloadsCmd_open";
    if (button.classList.contains("action-button")) {
      button = button.parentNode;
      command = button.hasAttribute("showLabel") ? "downloadsCmd_show" : "downloadsCmd_retry";
    } else if (button.localName == "menuitem") {
      command = button.getAttribute("command");
      button = button.parentNode._anchorNode;
    }
    while (button && !button._shell && button != this.panelview &&
           (!button.hasAttribute || !button.hasAttribute("oncommand"))) {
      button = button.parentNode;
    }

    // We don't need to do anything when no button was clicked, like a separator
    // or a blank panel area. Also, when 'oncommand' is set, the button will invoke
    // its own, custom command handler.
    if (!button || button == this.panelview || button.hasAttribute("oncommand"))
      return;

    if (command == "downloadsCmd_clearDownloads") {
      DownloadsSubview.onClearDownloads(button);
    } else if (button._shell.isCommandEnabled(command)) {
      button._shell[command]();
    }
  }
}

DownloadsSubview.Button = class extends DownloadsViewUI.DownloadElementShell {
  constructor(download, document) {
    super();
    this.download = download;

    this.element = document.createElement("toolbarbutton");
    this.element._shell = this;

    this.element.classList.add("subviewbutton", "subviewbutton-iconic", "download",
      "download-state");
  }

  get browserWindow() {
    return this.element.ownerGlobal;
  }

  async refresh() {
    if (this._targetFileChecked)
      return;

    try {
      await this.download.refresh();
    } catch (ex) {
      Cu.reportError(ex);
    } finally {
      this._targetFileChecked = true;
    }
  }

  /**
   * Handle state changes of a download.
   */
  onStateChanged() {
    // Since the state changed, we may need to check the target file again.
    this._targetFileChecked = false;

    this._updateState();
  }

  /**
   * Handler method; invoked when any state attribute of a download changed.
   */
  onChanged() {
    let newState = DownloadsCommon.stateOfDownload(this.download);
    if (this._downloadState !== newState) {
      this._downloadState = newState;
      this.onStateChanged();
    } else {
      this._updateState();
    }

    // This cannot be placed within onStateChanged because when a download goes
    // from hasBlockedData to !hasBlockedData it will still remain in the same state.
    this.element.classList.toggle("temporary-block",
                                  !!this.download.hasBlockedData);
  }

  /**
   * Update the DOM representation of this download to match the current, recently
   * updated, state.
   */
  _updateState() {
    super._updateState();
    this.element.setAttribute("label", this.element.getAttribute("displayName"));
    this.element.setAttribute("tooltiptext", this.element.getAttribute("fullStatus"));

    if (this.isCommandEnabled("downloadsCmd_show")) {
      this.element.setAttribute("openLabel", kButtonLabels.open);
      this.element.setAttribute("showLabel", kButtonLabels.show);
      this.element.removeAttribute("retryLabel");
    } else if (this.isCommandEnabled("downloadsCmd_retry")) {
      this.element.setAttribute("retryLabel", kButtonLabels.retry);
      this.element.removeAttribute("openLabel");
      this.element.removeAttribute("showLabel");
    } else {
      this.element.removeAttribute("openLabel");
      this.element.removeAttribute("retryLabel");
      this.element.removeAttribute("showLabel");
    }

    this._updateVisibility();
  }

  _updateVisibility() {
    let state = this.element.getAttribute("state");
    // This view only show completed and failed downloads.
    this.element.hidden = !(state == DownloadsCommon.DOWNLOAD_FINISHED ||
      state == DownloadsCommon.DOWNLOAD_FAILED);
  }

  /**
   * Command handler; copy the download URL to the OS general clipboard.
   */
  downloadsCmd_copyLocation() {
    let clipboard = Cc["@mozilla.org/widget/clipboardhelper;1"]
                      .getService(Ci.nsIClipboardHelper);
    clipboard.copyString(this.download.source.url);
  }
};
