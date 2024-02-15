/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{
  class MozTabbrowserTab extends MozElements.MozTab {
    static markup = `
      <stack class="tab-stack" flex="1">
        <vbox class="tab-background">
          <hbox class="tab-context-line"/>
          <hbox class="tab-loading-burst" flex="1"/>
        </vbox>
        <hbox class="tab-content" align="center">
          <stack class="tab-icon-stack">
            <hbox class="tab-throbber"/>
            <hbox class="tab-icon-pending"/>
            <html:img class="tab-icon-image" role="presentation" decoding="sync" />
            <image class="tab-sharing-icon-overlay" role="presentation"/>
            <image class="tab-icon-overlay" role="presentation"/>
          </stack>
          <vbox class="tab-label-container"
                onoverflow="this.setAttribute('textoverflow', 'true');"
                onunderflow="this.removeAttribute('textoverflow');"
                align="start"
                pack="center"
                flex="1">
            <label class="tab-text tab-label" role="presentation"/>
            <hbox class="tab-secondary-label">
              <label class="tab-icon-sound-label tab-icon-sound-playing-label" data-l10n-id="browser-tab-audio-playing2" role="presentation"/>
              <label class="tab-icon-sound-label tab-icon-sound-muted-label" data-l10n-id="browser-tab-audio-muted2" role="presentation"/>
              <label class="tab-icon-sound-label tab-icon-sound-blocked-label" data-l10n-id="browser-tab-audio-blocked" role="presentation"/>
              <label class="tab-icon-sound-label tab-icon-sound-pip-label" data-l10n-id="browser-tab-audio-pip" role="presentation"/>
              <label class="tab-icon-sound-label tab-icon-sound-tooltip-label" role="presentation"/>
            </hbox>
          </vbox>
          <image class="tab-close-button close-icon" role="presentation"/>
        </hbox>
      </stack>
      `;

    constructor() {
      super();

      this.addEventListener("mouseover", this);
      this.addEventListener("mouseout", this);
      this.addEventListener("dragstart", this, true);
      this.addEventListener("dragstart", this);
      this.addEventListener("mousedown", this);
      this.addEventListener("mouseup", this);
      this.addEventListener("click", this);
      this.addEventListener("dblclick", this, true);
      this.addEventListener("animationend", this);
      this.addEventListener("focus", this);
      this.addEventListener("AriaFocus", this);

      this._hover = false;
      this._selectedOnFirstMouseDown = false;

      /**
       * Describes how the tab ended up in this mute state. May be any of:
       *
       * - undefined: The tabs mute state has never changed.
       * - null: The mute state was last changed through the UI.
       * - Any string: The ID was changed through an extension API. The string
       * must be the ID of the extension which changed it.
       */
      this.muteReason = undefined;

      this.mOverCloseButton = false;

      this.mCorrespondingMenuitem = null;

      this.closing = false;
    }

    static get inheritedAttributes() {
      return {
        ".tab-background": "selected=visuallyselected,fadein,multiselected",
        ".tab-line": "selected=visuallyselected,multiselected",
        ".tab-loading-burst": "pinned,bursting,notselectedsinceload",
        ".tab-content":
          "pinned,selected=visuallyselected,titlechanged,attention",
        ".tab-icon-stack":
          "sharing,pictureinpicture,crashed,busy,soundplaying,soundplaying-scheduledremoval,pinned,muted,blocked,selected=visuallyselected,activemedia-blocked,indicator-replaces-favicon",
        ".tab-throbber":
          "fadein,pinned,busy,progress,selected=visuallyselected",
        ".tab-icon-pending":
          "fadein,pinned,busy,progress,selected=visuallyselected,pendingicon",
        ".tab-icon-image":
          "src=image,triggeringprincipal=iconloadingprincipal,requestcontextid,fadein,pinned,selected=visuallyselected,busy,crashed,sharing,pictureinpicture",
        ".tab-sharing-icon-overlay": "sharing,selected=visuallyselected,pinned",
        ".tab-icon-overlay":
          "sharing,pictureinpicture,crashed,busy,soundplaying,soundplaying-scheduledremoval,pinned,muted,blocked,selected=visuallyselected,activemedia-blocked,indicator-replaces-favicon",
        ".tab-label-container":
          "pinned,selected=visuallyselected,labeldirection",
        ".tab-label":
          "text=label,accesskey,fadein,pinned,selected=visuallyselected,attention",
        ".tab-label-container .tab-secondary-label":
          "soundplaying,soundplaying-scheduledremoval,pinned,muted,blocked,selected=visuallyselected,activemedia-blocked,pictureinpicture",
        ".tab-close-button": "fadein,pinned,selected=visuallyselected",
      };
    }

    connectedCallback() {
      this.initialize();
    }

    initialize() {
      if (this._initialized) {
        return;
      }

      this.textContent = "";
      this.appendChild(this.constructor.fragment);
      this.initializeAttributeInheritance();
      this.setAttribute("context", "tabContextMenu");
      this._initialized = true;

      if (!("_lastAccessed" in this)) {
        this.updateLastAccessed();
      }
    }

    get owner() {
      let owner = this._owner?.deref();
      if (owner && !owner.closing) {
        return owner;
      }
      return null;
    }

    set owner(owner) {
      if (owner) {
        this._owner = new WeakRef(owner);
      } else {
        this._owner = null;
      }
    }

    get container() {
      return gBrowser.tabContainer;
    }

    set attention(val) {
      if (val == this.hasAttribute("attention")) {
        return;
      }

      this.toggleAttribute("attention", val);
      gBrowser._tabAttrModified(this, ["attention"]);
    }

    set undiscardable(val) {
      if (val == this.hasAttribute("undiscardable")) {
        return;
      }

      this.toggleAttribute("undiscardable", val);
      gBrowser._tabAttrModified(this, ["undiscardable"]);
    }

    set _visuallySelected(val) {
      if (val == this.hasAttribute("visuallyselected")) {
        return;
      }

      this.toggleAttribute("visuallyselected", val);
      gBrowser._tabAttrModified(this, ["visuallyselected"]);
    }

    set _selected(val) {
      // in e10s we want to only pseudo-select a tab before its rendering is done, so that
      // the rest of the system knows that the tab is selected, but we don't want to update its
      // visual status to selected until after we receive confirmation that its content has painted.
      if (val) {
        this.setAttribute("selected", "true");
      } else {
        this.removeAttribute("selected");
      }

      // If we're non-e10s we need to update the visual selection at the same
      // time, otherwise AsyncTabSwitcher will take care of this.
      if (!gMultiProcessBrowser) {
        this._visuallySelected = val;
      }
    }

    get pinned() {
      return this.hasAttribute("pinned");
    }

    get hidden() {
      // This getter makes `hidden` read-only
      return super.hidden;
    }

    get muted() {
      return this.hasAttribute("muted");
    }

    get multiselected() {
      return this.hasAttribute("multiselected");
    }

    get userContextId() {
      return this.hasAttribute("usercontextid")
        ? parseInt(this.getAttribute("usercontextid"))
        : 0;
    }

    get soundPlaying() {
      return this.hasAttribute("soundplaying");
    }

    get pictureinpicture() {
      return this.hasAttribute("pictureinpicture");
    }

    get activeMediaBlocked() {
      return this.hasAttribute("activemedia-blocked");
    }

    get undiscardable() {
      return this.hasAttribute("undiscardable");
    }

    get isEmpty() {
      // Determines if a tab is "empty", usually used in the context of determining
      // if it's ok to close the tab.
      if (this.hasAttribute("busy")) {
        return false;
      }

      if (this.hasAttribute("customizemode")) {
        return false;
      }

      let browser = this.linkedBrowser;
      if (!isBlankPageURL(browser.currentURI.spec)) {
        return false;
      }

      if (!BrowserUIUtils.checkEmptyPageOrigin(browser)) {
        return false;
      }

      if (browser.canGoForward || browser.canGoBack) {
        return false;
      }

      return true;
    }

    get lastAccessed() {
      return this._lastAccessed == Infinity ? Date.now() : this._lastAccessed;
    }

    get lastSeenActive() {
      const isForegroundWindow =
        this.ownerGlobal ==
        BrowserWindowTracker.getTopWindow({ allowPopups: true });
      // the timestamp for the selected tab in the active window is always now
      if (isForegroundWindow && this.selected) {
        return Date.now();
      }
      if (this._lastSeenActive) {
        return this._lastSeenActive;
      }
      // Use the application start time as the fallback value
      return Services.startup.getStartupInfo().start.getTime();
    }

    get _overPlayingIcon() {
      return this.overlayIcon?.matches(":hover");
    }

    get overlayIcon() {
      return this.querySelector(".tab-icon-overlay");
    }

    get throbber() {
      return this.querySelector(".tab-throbber");
    }

    get iconImage() {
      return this.querySelector(".tab-icon-image");
    }

    get sharingIcon() {
      return this.querySelector(".tab-sharing-icon-overlay");
    }

    get textLabel() {
      return this.querySelector(".tab-label");
    }

    get closeButton() {
      return this.querySelector(".tab-close-button");
    }

    updateLastAccessed(aDate) {
      this._lastAccessed = this.selected ? Infinity : aDate || Date.now();
    }

    updateLastSeenActive() {
      this._lastSeenActive = Date.now();
    }

    updateLastUnloadedByTabUnloader() {
      this._lastUnloaded = Date.now();
      Services.telemetry.scalarAdd("browser.engagement.tab_unload_count", 1);
    }

    recordTimeFromUnloadToReload() {
      if (!this._lastUnloaded) {
        return;
      }

      const diff_in_msec = Date.now() - this._lastUnloaded;
      Services.telemetry
        .getHistogramById("TAB_UNLOAD_TO_RELOAD")
        .add(diff_in_msec / 1000);
      Services.telemetry.scalarAdd("browser.engagement.tab_reload_count", 1);
      delete this._lastUnloaded;
    }

    on_mouseover(event) {
      if (event.target.classList.contains("tab-close-button")) {
        this.mOverCloseButton = true;
      }
      if (this._overPlayingIcon) {
        const selectedTabs = gBrowser.selectedTabs;
        const contextTabInSelection = selectedTabs.includes(this);
        const affectedTabsLength = contextTabInSelection
          ? selectedTabs.length
          : 1;
        let stringID;
        if (this.hasAttribute("activemedia-blocked")) {
          stringID = "browser-tab-unblock";
        } else {
          stringID = this.linkedBrowser.audioMuted
            ? "browser-tab-unmute"
            : "browser-tab-mute";
        }
        this.setSecondaryTabTooltipLabel(stringID, {
          count: affectedTabsLength,
        });
      }
      this._mouseenter();
    }

    on_mouseout(event) {
      if (event.target.classList.contains("tab-close-button")) {
        this.mOverCloseButton = false;
      }
      if (event.target == this.overlayIcon) {
        this.setSecondaryTabTooltipLabel(null);
      }
      this._mouseleave();
    }

    on_dragstart(event) {
      // We use "failed" drag end events that weren't cancelled by the user
      // to detach tabs. Ensure that we do not show the drag image returning
      // to its point of origin when this happens, as it makes the drag
      // finishing feel very slow.
      event.dataTransfer.mozShowFailAnimation = false;
      if (event.eventPhase == Event.CAPTURING_PHASE) {
        this.style.MozUserFocus = "";
      } else if (
        this.mOverCloseButton ||
        gSharedTabWarning.willShowSharedTabWarning(this)
      ) {
        event.stopPropagation();
      }
    }

    on_mousedown(event) {
      let eventMaySelectTab = true;
      let tabContainer = this.container;

      if (
        tabContainer._closeTabByDblclick &&
        event.button == 0 &&
        event.detail == 1
      ) {
        this._selectedOnFirstMouseDown = this.selected;
      }

      if (this.selected) {
        this.style.MozUserFocus = "ignore";
      } else if (
        event.target.classList.contains("tab-close-button") ||
        event.target.classList.contains("tab-icon-overlay")
      ) {
        eventMaySelectTab = false;
      }

      if (event.button == 1) {
        gBrowser.warmupTab(gBrowser._findTabToBlurTo(this));
      }

      if (event.button == 0) {
        let shiftKey = event.shiftKey;
        let accelKey = event.getModifierState("Accel");
        if (shiftKey) {
          eventMaySelectTab = false;
          const lastSelectedTab = gBrowser.lastMultiSelectedTab;
          if (!accelKey) {
            gBrowser.selectedTab = lastSelectedTab;

            // Make sure selection is cleared when tab-switch doesn't happen.
            gBrowser.clearMultiSelectedTabs();
          }
          gBrowser.addRangeToMultiSelectedTabs(lastSelectedTab, this);
        } else if (accelKey) {
          // Ctrl (Cmd for mac) key is pressed
          eventMaySelectTab = false;
          if (this.multiselected) {
            gBrowser.removeFromMultiSelectedTabs(this);
          } else if (this != gBrowser.selectedTab) {
            gBrowser.addToMultiSelectedTabs(this);
            gBrowser.lastMultiSelectedTab = this;
          }
        } else if (!this.selected && this.multiselected) {
          gBrowser.lockClearMultiSelectionOnce();
        }
      }

      if (gSharedTabWarning.willShowSharedTabWarning(this)) {
        eventMaySelectTab = false;
      }

      if (eventMaySelectTab) {
        super.on_mousedown(event);
      }
    }

    on_mouseup(event) {
      // Make sure that clear-selection is released.
      // Otherwise selection using Shift key may be broken.
      gBrowser.unlockClearMultiSelection();

      this.style.MozUserFocus = "";
    }

    on_click(event) {
      if (event.button != 0) {
        return;
      }

      if (event.getModifierState("Accel") || event.shiftKey) {
        return;
      }

      if (
        gBrowser.multiSelectedTabsCount > 0 &&
        !event.target.classList.contains("tab-close-button") &&
        !event.target.classList.contains("tab-icon-overlay")
      ) {
        // Tabs were previously multi-selected and user clicks on a tab
        // without holding Ctrl/Cmd Key
        gBrowser.clearMultiSelectedTabs();
      }

      if (event.target.classList.contains("tab-icon-overlay")) {
        if (this.activeMediaBlocked) {
          if (this.multiselected) {
            gBrowser.resumeDelayedMediaOnMultiSelectedTabs(this);
          } else {
            this.resumeDelayedMedia();
          }
        } else if (this.soundPlaying || this.muted) {
          if (this.multiselected) {
            gBrowser.toggleMuteAudioOnMultiSelectedTabs(this);
          } else {
            this.toggleMuteAudio();
          }
        }
        return;
      }

      if (event.target.classList.contains("tab-close-button")) {
        if (this.multiselected) {
          gBrowser.removeMultiSelectedTabs();
        } else {
          gBrowser.removeTab(this, {
            animate: true,
            triggeringEvent: event,
          });
        }
        // This enables double-click protection for the tab container
        // (see tabbrowser-tabs 'click' handler).
        gBrowser.tabContainer._blockDblClick = true;
      }
    }

    on_dblclick(event) {
      if (event.button != 0) {
        return;
      }

      // for the one-close-button case
      if (event.target.classList.contains("tab-close-button")) {
        event.stopPropagation();
      }

      let tabContainer = this.container;
      if (
        tabContainer._closeTabByDblclick &&
        this._selectedOnFirstMouseDown &&
        this.selected &&
        !event.target.classList.contains("tab-icon-overlay")
      ) {
        gBrowser.removeTab(this, {
          animate: true,
          triggeringEvent: event,
        });
      }
    }

    on_animationend(event) {
      if (event.target.classList.contains("tab-loading-burst")) {
        this.removeAttribute("bursting");
      }
    }

    _mouseenter() {
      if (this.hidden || this.closing) {
        return;
      }
      this._hover = true;

      if (this.selected) {
        this.container._handleTabSelect();
      } else if (this.linkedPanel) {
        this.linkedBrowser.unselectedTabHover(true);
        this.startUnselectedTabHoverTimer();
      }

      // Prepare connection to host beforehand.
      SessionStore.speculativeConnectOnTabHover(this);

      let tabToWarm = this;
      if (this.mOverCloseButton) {
        tabToWarm = gBrowser._findTabToBlurTo(this);
      }
      gBrowser.warmupTab(tabToWarm);

      this.dispatchEvent(new CustomEvent("TabHoverStart", { bubbles: true }));
    }

    _mouseleave() {
      if (!this._hover) {
        return;
      }
      this._hover = false;
      if (this.linkedPanel && !this.selected) {
        this.linkedBrowser.unselectedTabHover(false);
        this.cancelUnselectedTabHoverTimer();
      }
      this.dispatchEvent(new CustomEvent("TabHoverEnd", { bubbles: true }));
    }

    setSecondaryTabTooltipLabel(l10nID, l10nArgs) {
      this.querySelector(".tab-secondary-label").toggleAttribute(
        "showtooltip",
        l10nID
      );

      const tooltipEl = this.querySelector(".tab-icon-sound-tooltip-label");

      if (l10nArgs) {
        tooltipEl.setAttribute("data-l10n-args", JSON.stringify(l10nArgs));
      } else {
        tooltipEl.removeAttribute("data-l10n-args");
      }
      if (l10nID) {
        tooltipEl.setAttribute("data-l10n-id", l10nID);
      } else {
        tooltipEl.removeAttribute("data-l10n-id");
      }
      // TODO(Itiel): Maybe simplify this when bug 1830989 lands
    }

    startUnselectedTabHoverTimer() {
      // Only record data when we need to.
      if (!this.linkedBrowser.shouldHandleUnselectedTabHover) {
        return;
      }

      if (
        !TelemetryStopwatch.running("HOVER_UNTIL_UNSELECTED_TAB_OPENED", this)
      ) {
        TelemetryStopwatch.start("HOVER_UNTIL_UNSELECTED_TAB_OPENED", this);
      }

      if (this._hoverTabTimer) {
        clearTimeout(this._hoverTabTimer);
        this._hoverTabTimer = null;
      }
    }

    cancelUnselectedTabHoverTimer() {
      // Since we're listening "mouseout" event, instead of "mouseleave".
      // Every time the cursor is moving from the tab to its child node (icon),
      // it would dispatch "mouseout"(for tab) first and then dispatch
      // "mouseover" (for icon, eg: close button, speaker icon) soon.
      // It causes we would cancel present TelemetryStopwatch immediately
      // when cursor is moving on the icon, and then start a new one.
      // In order to avoid this situation, we could delay cancellation and
      // remove it if we get "mouseover" within very short period.
      this._hoverTabTimer = setTimeout(() => {
        if (
          TelemetryStopwatch.running("HOVER_UNTIL_UNSELECTED_TAB_OPENED", this)
        ) {
          TelemetryStopwatch.cancel("HOVER_UNTIL_UNSELECTED_TAB_OPENED", this);
        }
      }, 100);
    }

    finishUnselectedTabHoverTimer() {
      // Stop timer when the tab is opened.
      if (
        TelemetryStopwatch.running("HOVER_UNTIL_UNSELECTED_TAB_OPENED", this)
      ) {
        TelemetryStopwatch.finish("HOVER_UNTIL_UNSELECTED_TAB_OPENED", this);
      }
    }

    resumeDelayedMedia() {
      if (this.activeMediaBlocked) {
        Services.telemetry
          .getHistogramById("TAB_AUDIO_INDICATOR_USED")
          .add(3 /* unblockByClickingIcon */);
        this.removeAttribute("activemedia-blocked");
        this.linkedBrowser.resumeMedia();
        gBrowser._tabAttrModified(this, ["activemedia-blocked"]);
      }
    }

    toggleMuteAudio(aMuteReason) {
      let browser = this.linkedBrowser;
      let hist = Services.telemetry.getHistogramById(
        "TAB_AUDIO_INDICATOR_USED"
      );

      if (browser.audioMuted) {
        if (this.linkedPanel) {
          // "Lazy Browser" should not invoke its unmute method
          browser.unmute();
        }
        this.removeAttribute("muted");
        hist.add(1 /* unmute */);
      } else {
        if (this.linkedPanel) {
          // "Lazy Browser" should not invoke its mute method
          browser.mute();
        }
        this.toggleAttribute("muted", true);
        hist.add(0 /* mute */);
      }
      this.muteReason = aMuteReason || null;

      gBrowser._tabAttrModified(this, ["muted"]);
    }

    setUserContextId(aUserContextId) {
      if (aUserContextId) {
        if (this.linkedBrowser) {
          this.linkedBrowser.setAttribute("usercontextid", aUserContextId);
        }
        this.setAttribute("usercontextid", aUserContextId);
      } else {
        if (this.linkedBrowser) {
          this.linkedBrowser.removeAttribute("usercontextid");
        }
        this.removeAttribute("usercontextid");
      }

      ContextualIdentityService.setTabStyle(this);
    }

    updateA11yDescription() {
      let prevDescTab = gBrowser.tabContainer.querySelector(
        "tab[aria-describedby]"
      );
      if (prevDescTab) {
        // We can only have a description for the focused tab.
        prevDescTab.removeAttribute("aria-describedby");
      }
      let desc = document.getElementById("tabbrowser-tab-a11y-desc");
      desc.textContent = gBrowser.getTabTooltip(this, false);
      this.setAttribute("aria-describedby", "tabbrowser-tab-a11y-desc");
    }

    on_focus(event) {
      this.updateA11yDescription();
    }

    on_AriaFocus(event) {
      this.updateA11yDescription();
    }
  }

  customElements.define("tabbrowser-tab", MozTabbrowserTab, {
    extends: "tab",
  });
}
