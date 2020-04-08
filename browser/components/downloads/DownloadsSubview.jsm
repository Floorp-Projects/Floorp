/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["DownloadsSubview"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Downloads",
  "resource://gre/modules/Downloads.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DownloadsCommon",
  "resource:///modules/DownloadsCommon.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "DownloadsViewUI",
  "resource:///modules/DownloadsViewUI.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PlacesUtils",
  "resource://gre/modules/PlacesUtils.jsm"
);

let gPanelViewInstances = new WeakMap();
const kRefreshBatchSize = 10;
const kMaxWaitForIdleMs = 200;
XPCOMUtils.defineLazyGetter(this, "kButtonLabels", () => {
  return {
    show:
      DownloadsCommon.strings[
        AppConstants.platform == "macosx" ? "showMacLabel" : "showLabel"
      ],
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
    this.panelview.addEventListener(
      "ViewHiding",
      DownloadsSubview.onViewHiding
    );

    this._viewItemsForDownloads = new WeakMap();

    let contextMenu = this.document.getElementById(this.context);
    if (!contextMenu) {
      contextMenu = this.document
        .getElementById("downloadsContextMenu")
        .cloneNode(true);
      contextMenu.setAttribute("closemenu", "none");
      contextMenu.setAttribute("id", this.context);
      contextMenu.removeAttribute("onpopupshown");
      contextMenu.setAttribute(
        "onpopupshowing",
        "DownloadsSubview.updateContextMenu(document.popupNode, this);"
      );
      contextMenu.setAttribute(
        "onpopuphidden",
        "DownloadsSubview.onContextMenuHidden(this);"
      );
      let clearButton = contextMenu.querySelector(
        "menuitem[command='downloadsCmd_clearDownloads']"
      );
      clearButton.hidden = false;
      clearButton.previousElementSibling.hidden = true;
      contextMenu
        .querySelector("menuitem[command='cmd_delete']")
        .setAttribute("command", "downloadsCmd_delete");
    }
    this.panelview.appendChild(contextMenu);
    this.container.setAttribute("context", this.context);

    this._downloadsData = DownloadsCommon.getData(
      this.window,
      true,
      true,
      true
    );
    this._downloadsData.addView(this);
  }

  destructor(event) {
    this.panelview.removeEventListener("click", DownloadsSubview.onClick);
    this.panelview.removeEventListener(
      "ViewHiding",
      DownloadsSubview.onViewHiding
    );
    this._downloadsData.removeView(this);
    gPanelViewInstances.delete(this);
    this.destroyed = true;
  }

  /**
   * DataView handler; invoked when a batch of downloads is being passed in -
   * usually when this instance is added as a view in the constructor.
   */
  onDownloadBatchStarting() {
    this.window.clearTimeout(this._batchTimeout);
  }

  /**
   * DataView handler; invoked when the view stopped feeding its current list of
   * downloads.
   */
  onDownloadBatchEnded() {
    let { window } = this;
    window.clearTimeout(this._batchTimeout);
    // If there are no downloads to display, wait a bit to dispatch the load
    // completion event, because another batch may start right away.
    this._batchTimeout = window.setTimeout(
      () => {
        this._updateStatsFromDisk();
        this.panelview.dispatchEvent(new window.CustomEvent("DownloadsLoaded"));
      },
      this.container.childElementCount ? 0 : 200
    );
  }

  /**
   * DataView handler; invoked when a new download is added to the list.
   *
   * @param {Download} download
   * @param {DOMNode}  [options.insertBefore]
   */
  onDownloadAdded(download, { insertBefore } = {}) {
    let element = this.document.createXULElement("hbox");
    let shell = new DownloadsSubview.Button(download, element);
    this._viewItemsForDownloads.set(download, shell);

    // Since newest downloads are displayed at the top, either prepend the new
    // element or insert it after the one indicated by the insertBefore option.
    if (insertBefore) {
      this._viewItemsForDownloads
        .get(insertBefore)
        .element.insertAdjacentElement("afterend", element);
    } else {
      this.container.prepend(element);
    }

    // After connecting to the document, trigger the code that updates all
    // attributes to match the current state of the downloads.
    shell.ensureActive();
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
    if (this._updatingStats) {
      return;
    }

    this._updatingStats = true;

    try {
      let idleOptions = { timeout: kMaxWaitForIdleMs };
      // Start with getting an idle moment to (maybe) refresh the list of downloads.
      await new Promise(
        resolve => this.window.requestIdleCallback(resolve),
        idleOptions
      );
      // In the meantime, this instance could have been destroyed, so take note.
      if (this.destroyed) {
        return;
      }

      let count = 0;
      for (let button of this.container.children) {
        if (this.destroyed) {
          return;
        }
        if (!button._shell) {
          continue;
        }

        await button._shell.refresh();

        // Make sure to request a new idle moment every `kRefreshBatchSize` buttons.
        if (++count % kRefreshBatchSize === 0) {
          await new Promise(resolve =>
            this.window.requestIdleCallback(resolve, idleOptions)
          );
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
   * Show the Downloads subview panel and listen for events that will trigger
   * building the dynamic part of the view.
   *
   * @param {DOMNode} anchor The button that was commanded to trigger this function.
   */
  static show(anchor) {
    let document = anchor.ownerDocument;
    let window = anchor.ownerGlobal;

    let panelview = document.getElementById("PanelUI-downloads");
    anchor.setAttribute("closemenu", "none");
    gPanelViewInstances.set(panelview, new DownloadsSubview(panelview));

    // Since the DownloadsLists are propagated asynchronously, we need to wait a
    // little to get the view propagated.
    panelview.addEventListener(
      "ViewShowing",
      event => {
        event.detail.addBlocker(
          new Promise(resolve => {
            panelview.addEventListener("DownloadsLoaded", resolve, {
              once: true,
            });
          })
        );
      },
      { once: true }
    );

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
    if (!instance) {
      return;
    }
    instance._downloadsData.removeFinished();
    PlacesUtils.history
      .removeVisitsByFilter({
        transition: PlacesUtils.history.TRANSITIONS.DOWNLOAD,
      })
      .catch(Cu.reportError);
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
    if (button.hasAttribute("exists")) {
      menu.setAttribute("exists", button.getAttribute("exists"));
    } else {
      menu.removeAttribute("exists");
    }
    menu.classList.toggle(
      "temporary-block",
      button.classList.contains("temporary-block")
    );
    for (let menuitem of menu.getElementsByTagName("menuitem")) {
      let command = menuitem.getAttribute("command");
      if (!command) {
        continue;
      }
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
    if (!instance) {
      return false;
    }
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
    if (!instance) {
      return;
    }
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
    if (event.button > 1) {
      return;
    }

    let button = event.target.closest(
      ".subviewbutton,toolbarbutton,menuitem,panelview"
    );
    if (!button || button.localName == "panelview") {
      return;
    }

    let item = button.closest(".subviewbutton.download");

    let command = "downloadsCmd_open";
    if (button.classList.contains("action-button")) {
      command = item.hasAttribute("canShow")
        ? "downloadsCmd_show"
        : "downloadsCmd_retry";
    } else if (button.localName == "menuitem") {
      command = button.getAttribute("command");
      if (command == "downloadsCmd_clearDownloads") {
        DownloadsSubview.onClearDownloads(button);
        return;
      }
      item = button.parentNode._anchorNode;
    }

    if (item && item._shell.isCommandEnabled(command)) {
      item._shell[command]();
    }
  }
}

/**
 * Associates each document with a pre-built DOM fragment representing the
 * download list item. This is then cloned to create each individual list item.
 * This is stored on the document to prevent leaks that would occur if a single
 * instance created by one document's DOMParser was stored globally.
 */
var gDownloadsSubviewItemFragments = new WeakMap();

DownloadsSubview.Button = class extends DownloadsViewUI.DownloadElementShell {
  constructor(download, element) {
    super();
    this.download = download;
    this.element = element;
    this.element._shell = this;

    this.element.classList.add(
      "subviewbutton",
      "subviewbutton-iconic",
      "download",
      "download-state",
      "navigable"
    );

    let hover = event => {
      if (event.originalTarget.classList.contains("action-button")) {
        this.element.classList.toggle(
          "downloadHoveringButton",
          event.type == "mouseover"
        );
      }
    };
    this.element.addEventListener("mouseover", hover);
    this.element.addEventListener("mouseout", hover);
  }

  get browserWindow() {
    return this.element.ownerGlobal;
  }

  async refresh() {
    if (this._targetFileChecked) {
      return;
    }

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
  }

  static get markup() {
    return `
      <image class="toolbarbutton-icon" validate="always"/>
      <vbox class="toolbarbutton-text" flex="1">
        <label crop="end"/>
        <label class="status-text status-full" crop="end"/>
        <label class="status-text status-open" crop="end"/>
        <label class="status-text status-retry" crop="end"/>
        <label class="status-text status-show" crop="end"/>
      </vbox>
      <toolbarbutton class="action-button"/>
    `;
  }

  // DownloadElementShell
  connect() {
    let document = this.element.ownerDocument;
    let downloadsSubviewItemFragment = gDownloadsSubviewItemFragments.get(
      document
    );
    if (!downloadsSubviewItemFragment) {
      let MozXULElement = document.defaultView.MozXULElement;
      downloadsSubviewItemFragment = MozXULElement.parseXULToFragment(
        this.markup
      );
      gDownloadsSubviewItemFragments.set(
        document,
        downloadsSubviewItemFragment
      );
    }
    this.element.appendChild(downloadsSubviewItemFragment.cloneNode(true));
    for (let [propertyName, selector] of [
      ["_downloadTypeIcon", ".toolbarbutton-icon"],
      ["_downloadTarget", "label"],
      ["_downloadStatus", ".status-full"],
      ["_downloadButton", ".action-button"],
    ]) {
      this[propertyName] = this.element.querySelector(selector);
    }

    for (let [label, selector] of [
      [kButtonLabels.open, ".status-open"],
      [kButtonLabels.retry, ".status-retry"],
      [kButtonLabels.show, ".status-show"],
    ]) {
      this.element.querySelector(selector).value = label;
    }
  }

  // DownloadElementShell
  showDisplayNameAndIcon(displayName, icon) {
    this._downloadTarget.value = displayName;
    this._downloadTypeIcon.src = icon;
  }

  // DownloadElementShell
  showProgress() {}

  // DownloadElementShell
  showStatus(status) {
    this._downloadStatus.value = status;
    this.element.tooltipText = status;
  }

  // DownloadElementShell
  showButton() {}

  // DownloadElementShell
  hideButton() {}

  // DownloadElementShell
  _updateState() {
    // This view only show completed and failed downloads.
    let state = DownloadsCommon.stateOfDownload(this.download);
    let shouldDisplay =
      state == DownloadsCommon.DOWNLOAD_FINISHED ||
      state == DownloadsCommon.DOWNLOAD_FAILED;
    this.element.hidden = !shouldDisplay;
    if (!shouldDisplay) {
      return;
    }

    super._updateState();

    if (this.isCommandEnabled("downloadsCmd_show")) {
      this.element.setAttribute("canShow", "true");
      this.element.removeAttribute("canRetry");
    } else if (this.isCommandEnabled("downloadsCmd_retry")) {
      this.element.setAttribute("canRetry", "true");
      this.element.removeAttribute("canShow");
    } else {
      this.element.removeAttribute("canRetry");
      this.element.removeAttribute("canShow");
    }
  }

  // DownloadElementShell
  _updateStateInner() {
    if (!this.element.hidden) {
      super._updateStateInner();
    }
  }

  /**
   * Command handler; copy the download URL to the OS general clipboard.
   */
  downloadsCmd_copyLocation() {
    DownloadsCommon.copyDownloadLink(this.download);
  }
};
