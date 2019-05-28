  <binding id="tabbrowser-tab"
           extends="chrome://global/content/bindings/tabbox.xml#tab">
    <content context="tabContextMenu">
      <xul:stack class="tab-stack" flex="1">
        <xul:vbox xbl:inherits="selected=visuallyselected,fadein,multiselected"
                  class="tab-background">
          <xul:hbox xbl:inherits="selected=visuallyselected,multiselected,before-multiselected"
                    class="tab-line"/>
          <xul:spacer flex="1" class="tab-background-inner"/>
          <xul:hbox class="tab-bottom-line"/>
        </xul:vbox>
        <xul:hbox xbl:inherits="pinned,bursting,notselectedsinceload"
                  anonid="tab-loading-burst"
                  class="tab-loading-burst"/>
        <xul:hbox xbl:inherits="pinned,selected=visuallyselected,titlechanged,attention"
                  class="tab-content" align="center">
          <xul:hbox xbl:inherits="fadein,pinned,busy,progress,selected=visuallyselected"
                    anonid="tab-throbber"
                    class="tab-throbber"
                    layer="true"/>
          <xul:hbox xbl:inherits="fadein,pinned,busy,progress,selected=visuallyselected,pendingicon"
                    anonid="tab-icon-pending"
                    class="tab-icon-pending"/>
          <xul:image xbl:inherits="src=image,triggeringprincipal=iconloadingprincipal,requestcontextid,fadein,pinned,selected=visuallyselected,busy,crashed,sharing"
                     anonid="tab-icon-image"
                     class="tab-icon-image"
                     validate="never"
                     role="presentation"/>
          <xul:image xbl:inherits="sharing,selected=visuallyselected,pinned"
                     anonid="sharing-icon"
                     class="tab-sharing-icon-overlay"
                     role="presentation"/>
          <xul:image xbl:inherits="crashed,busy,soundplaying,soundplaying-scheduledremoval,pinned,muted,blocked,selected=visuallyselected,activemedia-blocked"
                     anonid="overlay-icon"
                     class="tab-icon-overlay"
                     role="presentation"/>
          <xul:hbox class="tab-label-container"
                    xbl:inherits="pinned,selected=visuallyselected,labeldirection"
                    onoverflow="this.setAttribute('textoverflow', 'true');"
                    onunderflow="this.removeAttribute('textoverflow');"
                    flex="1">
            <xul:label class="tab-text tab-label" anonid="tab-label"
                       xbl:inherits="xbl:text=label,accesskey,fadein,pinned,selected=visuallyselected,attention"
                       role="presentation"/>
          </xul:hbox>
          <xul:image xbl:inherits="pictureinpicture"
                     class="tab-icon-pip"
                     role="presentation"/>
          <xul:image xbl:inherits="soundplaying,soundplaying-scheduledremoval,pinned,muted,blocked,selected=visuallyselected,activemedia-blocked,pictureinpicture"
                     anonid="soundplaying-icon"
                     class="tab-icon-sound"
                     role="presentation"/>
          <xul:image anonid="close-button"
                     xbl:inherits="fadein,pinned,selected=visuallyselected"
                     class="tab-close-button close-icon"
                     role="presentation"/>
        </xul:hbox>
      </xul:stack>
    </content>

    <implementation>
      <constructor><![CDATA[
        if (!("_lastAccessed" in this)) {
          this.updateLastAccessed();
        }
      ]]></constructor>

      <property name="_visuallySelected">
        <setter>
          <![CDATA[
          if (val == (this.getAttribute("visuallyselected") == "true")) {
            return val;
          }

          if (val) {
            this.setAttribute("visuallyselected", "true");
          } else {
            this.removeAttribute("visuallyselected");
          }
          gBrowser._tabAttrModified(this, ["visuallyselected"]);

          return val;
          ]]>
        </setter>
      </property>

      <property name="_selected">
        <setter>
          <![CDATA[
          // in e10s we want to only pseudo-select a tab before its rendering is done, so that
          // the rest of the system knows that the tab is selected, but we don't want to update its
          // visual status to selected until after we receive confirmation that its content has painted.
          if (val)
            this.setAttribute("selected", "true");
          else
            this.removeAttribute("selected");

          // If we're non-e10s we should update the visual selection as well at the same time,
          // *or* if we're e10s and the visually selected tab isn't changing, in which case the
          // tab switcher code won't run and update anything else (like the before- and after-
          // selected attributes).
          if (!gMultiProcessBrowser || (val && this.hasAttribute("visuallyselected"))) {
            this._visuallySelected = val;
          }

          return val;
        ]]>
        </setter>
      </property>
      <field name="_selectedOnFirstMouseDown">false</field>

      <property name="pinned" readonly="true">
        <getter>
          return this.getAttribute("pinned") == "true";
        </getter>
      </property>
      <property name="hidden" readonly="true">
        <getter>
          return this.getAttribute("hidden") == "true";
        </getter>
      </property>
      <property name="muted" readonly="true">
        <getter>
          return this.getAttribute("muted") == "true";
        </getter>
      </property>
      <property name="multiselected" readonly="true">
        <getter>
          return this.getAttribute("multiselected") == "true";
        </getter>
      </property>
      <property name="beforeMultiselected" readonly="true">
        <getter>
          return this.getAttribute("before-multiselected") == "true";
        </getter>
      </property>
      <!--
      Describes how the tab ended up in this mute state. May be any of:

       - undefined: The tabs mute state has never changed.
       - null: The mute state was last changed through the UI.
       - Any string: The ID was changed through an extension API. The string
                     must be the ID of the extension which changed it.
      -->
      <field name="muteReason">undefined</field>

      <property name="userContextId" readonly="true">
        <getter>
          return this.hasAttribute("usercontextid")
                   ? parseInt(this.getAttribute("usercontextid"))
                   : 0;
        </getter>
      </property>

      <property name="soundPlaying" readonly="true">
        <getter>
          return this.getAttribute("soundplaying") == "true";
        </getter>
      </property>

      <property name="pictureinpicture" readonly="true">
        <getter>
          return this.getAttribute("pictureinpicture") == "true";
        </getter>
      </property>

      <property name="activeMediaBlocked" readonly="true">
        <getter>
          return this.getAttribute("activemedia-blocked") == "true";
        </getter>
      </property>

      <property name="isEmpty" readonly="true">
        <getter>
          // Determines if a tab is "empty", usually used in the context of determining
          // if it's ok to close the tab.
          if (this.hasAttribute("busy"))
            return false;

          if (this.hasAttribute("customizemode"))
            return false;

          let browser = this.linkedBrowser;
          if (!isBlankPageURL(browser.currentURI.spec))
            return false;

          if (!checkEmptyPageOrigin(browser))
            return false;

          if (browser.canGoForward || browser.canGoBack)
            return false;

          return true;
        </getter>
      </property>

      <property name="lastAccessed">
        <getter>
          return this._lastAccessed == Infinity ? Date.now() : this._lastAccessed;
        </getter>
      </property>
      <method name="updateLastAccessed">
        <parameter name="aDate"/>
        <body><![CDATA[
          this._lastAccessed = this.selected ? Infinity : (aDate || Date.now());
        ]]></body>
      </method>

      <field name="mOverCloseButton">false</field>
      <property name="_overPlayingIcon" readonly="true">
        <getter><![CDATA[
          let iconVisible = this.hasAttribute("soundplaying") ||
                            this.hasAttribute("muted") ||
                            this.hasAttribute("activemedia-blocked");
          let soundPlayingIcon =
            document.getAnonymousElementByAttribute(this, "anonid", "soundplaying-icon");
          let overlayIcon =
            document.getAnonymousElementByAttribute(this, "anonid", "overlay-icon");

          return soundPlayingIcon && soundPlayingIcon.matches(":hover") ||
                 (overlayIcon && overlayIcon.matches(":hover") && iconVisible);
        ]]></getter>
      </property>
      <field name="mCorrespondingMenuitem">null</field>

      <!--
      While it would make sense to track this in a field, the field will get nuked
      once the node is gone from the DOM, which causes us to think the tab is not
      closed, which causes us to make wrong decisions. So we use an expando instead.
      <field name="closing">false</field>
      -->

      <method name="_mouseenter">
        <body><![CDATA[
          if (this.hidden || this.closing) {
            return;
          }

          let tabContainer = this.parentNode;
          let visibleTabs = tabContainer._getVisibleTabs();
          let tabIndex = visibleTabs.indexOf(this);

          if (this.selected)
            tabContainer._handleTabSelect();

          if (tabIndex == 0) {
            tabContainer._beforeHoveredTab = null;
          } else {
            let candidate = visibleTabs[tabIndex - 1];
            let separatedByScrollButton =
              tabContainer.getAttribute("overflow") == "true" &&
              candidate.pinned && !this.pinned;
            if (!candidate.selected && !separatedByScrollButton) {
              tabContainer._beforeHoveredTab = candidate;
              candidate.setAttribute("beforehovered", "true");
            }
          }

          if (tabIndex == visibleTabs.length - 1) {
            tabContainer._afterHoveredTab = null;
          } else {
            let candidate = visibleTabs[tabIndex + 1];
            if (!candidate.selected) {
              tabContainer._afterHoveredTab = candidate;
              candidate.setAttribute("afterhovered", "true");
            }
          }

          tabContainer._hoveredTab = this;
          if (this.linkedPanel && !this.selected) {
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
        ]]></body>
      </method>

      <method name="_mouseleave">
        <body><![CDATA[
          let tabContainer = this.parentNode;
          if (tabContainer._beforeHoveredTab) {
            tabContainer._beforeHoveredTab.removeAttribute("beforehovered");
            tabContainer._beforeHoveredTab = null;
          }
          if (tabContainer._afterHoveredTab) {
            tabContainer._afterHoveredTab.removeAttribute("afterhovered");
            tabContainer._afterHoveredTab = null;
          }

          tabContainer._hoveredTab = null;
          if (this.linkedPanel && !this.selected) {
            this.linkedBrowser.unselectedTabHover(false);
            this.cancelUnselectedTabHoverTimer();
          }
        ]]></body>
      </method>

      <method name="startUnselectedTabHoverTimer">
        <body><![CDATA[
          // Only record data when we need to.
          if (!this.linkedBrowser.shouldHandleUnselectedTabHover) {
            return;
          }

          if (!TelemetryStopwatch.running("HOVER_UNTIL_UNSELECTED_TAB_OPENED", this)) {
            TelemetryStopwatch.start("HOVER_UNTIL_UNSELECTED_TAB_OPENED", this);
          }

          if (this._hoverTabTimer) {
            clearTimeout(this._hoverTabTimer);
            this._hoverTabTimer = null;
          }
        ]]></body>
      </method>

      <method name="cancelUnselectedTabHoverTimer">
        <body><![CDATA[
          // Since we're listening "mouseout" event, instead of "mouseleave".
          // Every time the cursor is moving from the tab to its child node (icon),
          // it would dispatch "mouseout"(for tab) first and then dispatch
          // "mouseover" (for icon, eg: close button, speaker icon) soon.
          // It causes we would cancel present TelemetryStopwatch immediately
          // when cursor is moving on the icon, and then start a new one.
          // In order to avoid this situation, we could delay cancellation and
          // remove it if we get "mouseover" within very short period.
          this._hoverTabTimer = setTimeout(() => {
            if (TelemetryStopwatch.running("HOVER_UNTIL_UNSELECTED_TAB_OPENED", this)) {
              TelemetryStopwatch.cancel("HOVER_UNTIL_UNSELECTED_TAB_OPENED", this);
            }
          }, 100);
        ]]></body>
      </method>

      <method name="finishUnselectedTabHoverTimer">
        <body><![CDATA[
          // Stop timer when the tab is opened.
          if (TelemetryStopwatch.running("HOVER_UNTIL_UNSELECTED_TAB_OPENED", this)) {
            TelemetryStopwatch.finish("HOVER_UNTIL_UNSELECTED_TAB_OPENED", this);
          }
        ]]></body>
      </method>

      <method name="toggleMuteAudio">
        <parameter name="aMuteReason"/>
        <body>
        <![CDATA[
          let browser = this.linkedBrowser;
          let modifiedAttrs = [];
          let hist = Services.telemetry.getHistogramById("TAB_AUDIO_INDICATOR_USED");

          if (this.hasAttribute("activemedia-blocked")) {
            this.removeAttribute("activemedia-blocked");
            modifiedAttrs.push("activemedia-blocked");

            browser.resumeMedia();
            hist.add(3 /* unblockByClickingIcon */);
          } else {
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
              this.setAttribute("muted", "true");
              hist.add(0 /* mute */);
            }
            this.muteReason = aMuteReason || null;
            modifiedAttrs.push("muted");
          }
          gBrowser._tabAttrModified(this, modifiedAttrs);
        ]]>
        </body>
      </method>

      <method name="setUserContextId">
        <parameter name="aUserContextId"/>
        <body>
        <![CDATA[
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
        ]]>
        </body>
      </method>
    </implementation>

    <handlers>
      <handler event="mouseover"><![CDATA[
        if (event.originalTarget.getAttribute("anonid") == "close-button") {
          this.mOverCloseButton = true;
        }

        this._mouseenter();
      ]]></handler>
      <handler event="mouseout"><![CDATA[
        if (event.originalTarget.getAttribute("anonid") == "close-button") {
          this.mOverCloseButton = false;
        }

        this._mouseleave();
      ]]></handler>

      <handler event="dragstart" phase="capturing">
        this.style.MozUserFocus = "";
      </handler>

      <handler event="dragstart"><![CDATA[
        if (this.mOverCloseButton) {
          event.stopPropagation();
        }
      ]]></handler>

      <handler event="mousedown" phase="capturing">
      <![CDATA[
        let tabContainer = this.parentNode;

        if (tabContainer._closeTabByDblclick &&
            event.button == 0 &&
            event.detail == 1) {
          this._selectedOnFirstMouseDown = this.selected;
        }

        if (this.selected) {
          this.style.MozUserFocus = "ignore";
        } else if (event.originalTarget.classList.contains("tab-close-button") ||
                   event.originalTarget.classList.contains("tab-icon-sound") ||
                   event.originalTarget.classList.contains("tab-icon-overlay")) {
            // Prevent tabbox.xml from selecting the tab.
            event.stopPropagation();
        }

        if (event.button == 1) {
          gBrowser.warmupTab(gBrowser._findTabToBlurTo(this));
        }

        if (event.button == 0 && tabContainer._multiselectEnabled) {
          let shiftKey = event.shiftKey;
          let accelKey = event.getModifierState("Accel");
          if (shiftKey) {
            const lastSelectedTab = gBrowser.lastMultiSelectedTab;
            if (!accelKey) {
              gBrowser.selectedTab = lastSelectedTab;

              // Make sure selection is cleared when tab-switch doesn't happen.
              gBrowser.clearMultiSelectedTabs(false);
            }
            gBrowser.addRangeToMultiSelectedTabs(lastSelectedTab, this);

            // Prevent tabbox.xml from selecting the tab.
            event.stopPropagation();
          } else if (accelKey) {
            // Ctrl (Cmd for mac) key is pressed
            if (this.multiselected) {
              gBrowser.removeFromMultiSelectedTabs(this, true);
            } else if (this != gBrowser.selectedTab) {
              gBrowser.addToMultiSelectedTabs(this, false);
              gBrowser.lastMultiSelectedTab = this;
            }

            // Prevent tabbox.xml from selecting the tab.
            event.stopPropagation();
          } else if (!this.selected && this.multiselected) {
            gBrowser.lockClearMultiSelectionOnce();
          }
        }
      ]]>
      </handler>
      <handler event="mouseup">
        // Make sure that clear-selection is released.
        // Otherwise selection using Shift key may be broken.
        gBrowser.unlockClearMultiSelection();

        this.style.MozUserFocus = "";
      </handler>

      <handler event="click" button="0"><![CDATA[
        if (event.getModifierState("Accel") || event.shiftKey) {
          return;
        }

        if (gBrowser.multiSelectedTabsCount > 0 &&
            !event.originalTarget.classList.contains("tab-close-button") &&
            !event.originalTarget.classList.contains("tab-icon-sound") &&
            !event.originalTarget.classList.contains("tab-icon-overlay")) {
          // Tabs were previously multi-selected and user clicks on a tab
          // without holding Ctrl/Cmd Key

          // Force positional attributes to update when the
          // target (of the click) is the "active" tab.
          let updatePositionalAttr = gBrowser.selectedTab == this;

          gBrowser.clearMultiSelectedTabs(updatePositionalAttr);
        }

        if (event.originalTarget.classList.contains("tab-icon-sound") ||
            (event.originalTarget.classList.contains("tab-icon-overlay") &&
             (event.originalTarget.hasAttribute("soundplaying") ||
              event.originalTarget.hasAttribute("muted") ||
              event.originalTarget.hasAttribute("activemedia-blocked")))) {
          if (this.multiselected) {
            gBrowser.toggleMuteAudioOnMultiSelectedTabs(this);
          } else {
            this.toggleMuteAudio();
          }
          return;
        }

        if (event.originalTarget.getAttribute("anonid") == "close-button") {
          if (this.multiselected) {
            gBrowser.removeMultiSelectedTabs();
          } else {
            gBrowser.removeTab(this, {
              animate: true,
              byMouse: event.mozInputSource == MouseEvent.MOZ_SOURCE_MOUSE,
            });
          }
          // This enables double-click protection for the tab container
          // (see tabbrowser-tabs 'click' handler).
          gBrowser.tabContainer._blockDblClick = true;
        }
      ]]></handler>

      <handler event="dblclick" button="0" phase="capturing"><![CDATA[
        // for the one-close-button case
        if (event.originalTarget.getAttribute("anonid") == "close-button") {
          event.stopPropagation();
        }

        let tabContainer = this.parentNode;
        if (tabContainer._closeTabByDblclick &&
            this._selectedOnFirstMouseDown &&
            this.selected &&
            !(event.originalTarget.classList.contains("tab-icon-sound") ||
              event.originalTarget.classList.contains("tab-icon-overlay"))) {
          gBrowser.removeTab(this, {
            animate: true,
            byMouse: event.mozInputSource == MouseEvent.MOZ_SOURCE_MOUSE,
          });
        }
      ]]></handler>

      <handler event="animationend">
      <![CDATA[
        if (event.originalTarget.getAttribute("anonid") == "tab-loading-burst") {
          this.removeAttribute("bursting");
        }
      ]]>
      </handler>
    </handlers>
  </binding>
