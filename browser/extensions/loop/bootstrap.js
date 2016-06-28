/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* exported startup, shutdown, install, uninstall */var _typeof = typeof Symbol === "function" && typeof Symbol.iterator === "symbol" ? function (obj) {return typeof obj;} : function (obj) {return obj && typeof Symbol === "function" && obj.constructor === Symbol ? "symbol" : typeof obj;};function _toConsumableArray(arr) {if (Array.isArray(arr)) {for (var i = 0, arr2 = Array(arr.length); i < arr.length; i++) {arr2[i] = arr[i];}return arr2;} else {return Array.from(arr);}}var _Components = 

Components;var Ci = _Components.interfaces;var Cu = _Components.utils;var Cc = _Components.classes;

var kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
var kBrowserSharingNotificationId = "loop-sharing-notification";

var CURSOR_MIN_DELTA = 3;
var CURSOR_MIN_INTERVAL = 100;
var CURSOR_CLICK_DELAY = 1000;
// Due to bug 1051238 frame scripts are cached forever, so we can't update them
// as a restartless add-on. The Math.random() is the work around for this.
var FRAME_SCRIPT = "chrome://loop/content/modules/tabFrame.js?" + Math.random();

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils", 
"resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI", 
"resource:///modules/CustomizableUI.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task", 
"resource://gre/modules/Task.jsm");

// See LOG_LEVELS in Console.jsm. Common examples: "All", "Info", "Warn", & "Error".
var PREF_LOG_LEVEL = "loop.debug.loglevel";

XPCOMUtils.defineLazyGetter(this, "log", function () {
  var ConsoleAPI = Cu.import("resource://gre/modules/Console.jsm", {}).ConsoleAPI;
  var consoleOptions = { 
    maxLogLevelPref: PREF_LOG_LEVEL, 
    prefix: "Loop" };

  return new ConsoleAPI(consoleOptions);});


/**
 * This window listener gets loaded into each browser.xul window and is used
 * to provide the required loop functions for the window.
 */
var WindowListener = { 
  // Records the add-on version once we know it.
  addonVersion: "unknown", 

  /**
   * Sets up the chrome integration within browser windows for Loop.
   *
   * @param {Object} window The window to inject the integration into.
   */
  setupBrowserUI: function setupBrowserUI(window) {
    var document = window.document;var 
    gBrowser = window.gBrowser;var gURLBar = window.gURLBar;
    var xhrClass = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"];
    var FileReader = window.FileReader;
    var menuItem = null;
    var isSlideshowOpen = false;
    var titleChangedListener = null;

    // the "exported" symbols
    var LoopUI = { 
      /**
       * @var {XULWidgetSingleWrapper} toolbarButton Getter for the Loop toolbarbutton
       *                                             instance for this window. This should
       *                                             not be used in the hidden window.
       */
      get toolbarButton() {
        delete this.toolbarButton;
        return this.toolbarButton = CustomizableUI.getWidget("loop-button").forWindow(window);}, 


      /**
       * @var {XULElement} panel Getter for the Loop panel element.
       */
      get panel() {
        delete this.panel;
        return this.panel = document.getElementById("loop-notification-panel");}, 


      /**
       * @var {XULElement|null} browser Getter for the Loop panel browser element.
       *                                Will be NULL if the panel hasn't loaded yet.
       */
      get browser() {
        var browser = document.querySelector("#loop-notification-panel > #loop-panel-iframe");
        if (browser) {
          delete this.browser;
          this.browser = browser;}

        return browser;}, 


      get isSlideshowOpen() {
        return isSlideshowOpen;}, 


      set isSlideshowOpen(aOpen) {
        isSlideshowOpen = aOpen;
        this.updateToolbarState();}, 

      /**
       * @return {Object} Getter for the Loop constants
       */
      get constants() {var _this = this;
        if (!this._constants) {
          // GetAllConstants is synchronous even though it's using a callback.
          this.LoopAPI.sendMessageToHandler({ 
            name: "GetAllConstants" }, 
          function (result) {
            _this._constants = result;});}



        return this._constants;}, 


      get mm() {
        return window.getGroupMessageManager("browsers");}, 


      /**
       * @return {Promise}
       */
      promiseDocumentVisible: function promiseDocumentVisible(aDocument) {
        if (!aDocument.hidden) {
          return Promise.resolve(aDocument);}


        return new Promise(function (resolve) {
          aDocument.addEventListener("visibilitychange", function onVisibilityChanged() {
            aDocument.removeEventListener("visibilitychange", onVisibilityChanged);
            resolve(aDocument);});});}, 




      /**
       * Toggle between opening or hiding the Loop panel.
       *
       * @param {DOMEvent} [event] Optional event that triggered the call to this
       *                           function.
       * @return {Promise}
       */
      togglePanel: function togglePanel(event) {var _this2 = this;
        if (!this.panel) {var _ret = function () {
            // We're on the hidden window! What fun!
            var obs = function obs(win) {
              Services.obs.removeObserver(obs, "browser-delayed-startup-finished");
              win.LoopUI.togglePanel(event);};

            Services.obs.addObserver(obs, "browser-delayed-startup-finished", false);
            return { v: window.OpenBrowserWindow() };}();if ((typeof _ret === "undefined" ? "undefined" : _typeof(_ret)) === "object") return _ret.v;}

        if (this.panel.state == "open") {
          return new Promise(function (resolve) {
            _this2.panel.hidePopup();
            resolve();});}



        if (this.isSlideshowOpen) {
          return Promise.resolve();}


        return this.openPanel(event).then(function (mm) {
          if (mm) {
            mm.sendAsyncMessage("Social:EnsureFocusElement");}}).

        catch(function (err) {
          Cu.reportError(err);});}, 



      /**
       * Called when a closing room has just been created, so we offer the
       * user the chance to modify the name. For that we need to open the panel.
       * Showing the proper layout is done on panel.jsx
       */
      renameRoom: function renameRoom() {
        this.openPanel();}, 


      /**
       * Opens the panel for Loop and sizes it appropriately.
       *
       * @param {event}  event   The event opening the panel, used to anchor
       *                         the panel to the button which triggers it.
       * @return {Promise}
       */
      openPanel: function openPanel(event) {var _this3 = this;
        if (PrivateBrowsingUtils.isWindowPrivate(window)) {
          return Promise.reject();}


        return new Promise(function (resolve) {
          var callback = function callback(iframe) {
            var mm = iframe.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;
            if (!("messageManager" in iframe)) {
              iframe.messageManager = mm;}


            if (!_this3._panelInitialized) {
              _this3.hookWindowCloseForPanelClose(iframe);
              _this3._panelInitialized = true;}


            mm.sendAsyncMessage("Social:WaitForDocumentVisible");
            mm.addMessageListener("Social:DocumentVisible", function onDocumentVisible() {
              mm.removeMessageListener("Social:DocumentVisible", onDocumentVisible);
              resolve(mm);});


            var buckets = _this3.constants.LOOP_MAU_TYPE;
            _this3.LoopAPI.sendMessageToHandler({ 
              name: "TelemetryAddValue", 
              data: ["LOOP_ACTIVITY_COUNTER", buckets.OPEN_PANEL] });};



          // Used to clear the temporary "login" state from the button.
          Services.obs.notifyObservers(null, "loop-status-changed", null);

          _this3.shouldResumeTour().then(function (resume) {
            if (resume) {
              // Assume the conversation with the visitor wasn't open since we would
              // have resumed the tour as soon as the visitor joined if it was (and
              // the pref would have been set to false already.
              _this3.MozLoopService.resumeTour("waiting");
              resolve(null);
              return;}


            _this3.LoopAPI.initialize();

            var anchor = event ? event.target : _this3.toolbarButton.anchor;
            _this3.PanelFrame.showPopup(
            window, 
            anchor, 
            "loop", // Notification Panel Type
            null, // Origin
            "about:looppanel", // Source
            null, // Size
            callback);});});}, 




      /**
       * Wrapper for openPanel - to support Firefox 46 and 45.
       *
       * @param {event}  event   The event opening the panel, used to anchor
       *                         the panel to the button which triggers it.
       * @return {Promise}
       */
      openCallPanel: function openCallPanel(event) {
        return this.openPanel(event);}, 


      /**
       * Method to know whether actions to open the panel should instead resume the tour.
       *
       * We need the panel to be opened via UITour so that it gets @noautohide.
       *
       * @return {Promise} resolving with a {Boolean} of whether the tour should be resumed instead of
       *                   opening the panel.
       */
      shouldResumeTour: Task.async(function* () {
        // Resume the FTU tour if this is the first time a room was joined by
        // someone else since the tour.
        if (!Services.prefs.getBoolPref("loop.gettingStarted.resumeOnFirstJoin")) {
          return false;}


        if (!this.LoopRooms.participantsCount) {
          // Nobody is in the rooms
          return false;}


        var roomsWithNonOwners = yield this.roomsWithNonOwners();
        if (!roomsWithNonOwners.length) {
          // We were the only one in a room but we want to know about someone else joining.
          return false;}


        return true;}), 


      /**
       * @return {Promise} resolved with an array of Rooms with participants (excluding owners)
       */
      roomsWithNonOwners: function roomsWithNonOwners() {var _this4 = this;
        return new Promise(function (resolve) {
          _this4.LoopRooms.getAll(function (error, rooms) {
            var roomsWithNonOwners = [];var _iteratorNormalCompletion = true;var _didIteratorError = false;var _iteratorError = undefined;try {
              for (var _iterator = rooms[Symbol.iterator](), _step; !(_iteratorNormalCompletion = (_step = _iterator.next()).done); _iteratorNormalCompletion = true) {var room = _step.value;
                if (!("participants" in room)) {
                  continue;}

                var numNonOwners = room.participants.filter(function (participant) {return !participant.owner;}).length;
                if (!numNonOwners) {
                  continue;}

                roomsWithNonOwners.push(room);}} catch (err) {_didIteratorError = true;_iteratorError = err;} finally {try {if (!_iteratorNormalCompletion && _iterator.return) {_iterator.return();}} finally {if (_didIteratorError) {throw _iteratorError;}}}

            resolve(roomsWithNonOwners);});});}, 




      /**
       * Triggers the initialization of the loop service if necessary.
       * Also adds appropraite observers for the UI.
       */
      init: function init() {var _this5 = this;
        // This is a promise for test purposes, but we don't want to be logging
        // expected errors to the console, so we catch them here.
        this.MozLoopService.initialize(WindowListener.addonVersion).catch(function (ex) {
          if (!ex.message || 
          !ex.message.contains("not enabled") && 
          !ex.message.contains("not needed")) {
            console.error(ex);}});



        // If we're in private browsing mode, then don't add the menu item,
        // also don't add the listeners as we don't want to update the button.
        if (PrivateBrowsingUtils.isWindowPrivate(window)) {
          return;}


        this.addMenuItem();

        // Don't do the rest if this is for the hidden window - we don't
        // have a toolbar there.
        if (window == Services.appShell.hiddenDOMWindow) {
          return;}


        // Load the frame script into any tab, plus any that get created in the
        // future.
        this.mm.loadFrameScript(FRAME_SCRIPT, true);

        // Cleanup when the window unloads.
        window.addEventListener("unload", function () {
          Services.obs.removeObserver(_this5, "loop-status-changed");});


        Services.obs.addObserver(this, "loop-status-changed", false);

        this.maybeAddCopyPanel();
        this.updateToolbarState();}, 


      /**
       * Adds a menu item to the browsers' Tools menu that open the Loop panel
       * when selected.
       */
      addMenuItem: function addMenuItem() {var _this6 = this;
        var menu = document.getElementById("menu_ToolsPopup");
        if (!menu || menuItem) {
          return;}


        menuItem = document.createElementNS(kNSXUL, "menuitem");
        menuItem.setAttribute("id", "menu_openLoop");
        menuItem.setAttribute("label", this._getString("loopMenuItem_label"));
        menuItem.setAttribute("accesskey", this._getString("loopMenuItem_accesskey"));

        menuItem.addEventListener("command", function () {return _this6.togglePanel();});

        menu.insertBefore(menuItem, document.getElementById("sync-setup"));}, 


      /**
       * Removes the menu item from the browsers' Tools menu.
       */
      removeMenuItem: function removeMenuItem() {
        if (menuItem) {
          menuItem.parentNode.removeChild(menuItem);}}, 



      /**
       * Maybe add the copy panel if it's not throttled and passes other checks.
       * @return {Promise} Resolved when decided and maybe panel-added.
       */
      maybeAddCopyPanel: function maybeAddCopyPanel() {var _this7 = this;
        // Don't bother adding the copy panel if we're in private browsing or
        // the user wants to never see it again or we've shown it enough times.
        if (PrivateBrowsingUtils.isWindowPrivate(window) || 
        Services.prefs.getBoolPref("loop.copy.shown") || 
        Services.prefs.getIntPref("loop.copy.showLimit") <= 0) {
          return Promise.resolve();}


        return Throttler.check("loop.copy").then(function () {return _this7.addCopyPanel();});}, 


      /**
       * Hook into the location bar copy command to open up the copy panel.
       * @param {Function} onClickHandled Optional callback for finished clicks.
       */
      addCopyPanel: function addCopyPanel(onClickHandled) {var _this8 = this, _arguments = arguments;
        // Make a copy of the loop panel as a starting point for the copy panel.
        var copy = this.panel.cloneNode(false);
        copy.id = "loop-copy-notification-panel";
        this.panel.parentNode.appendChild(copy);

        // Record a telemetry copy panel action.
        var addTelemetry = function addTelemetry(bucket) {
          _this8.LoopAPI.sendMessageToHandler({ 
            data: ["LOOP_COPY_PANEL_ACTIONS", _this8.constants.COPY_PANEL[bucket]], 
            name: "TelemetryAddValue" });};



        // Handle events from the copy panel iframe content.
        var onIframe = function onIframe(iframe) {
          // Watch for events from the copy panel when loaded.
          iframe.addEventListener("DOMContentLoaded", function onLoad() {
            iframe.removeEventListener("DOMContentLoaded", onLoad);

            // Size the panel to fit the rendered content adjusting for borders.
            iframe.contentWindow.requestAnimationFrame(function () {
              var height = iframe.contentDocument.documentElement.offsetHeight;
              height += copy.boxObject.height - iframe.boxObject.height;
              copy.style.height = height + "px";});


            // Hide the copy panel then show the loop panel.
            iframe.contentWindow.addEventListener("CopyPanelClick", function (event) {
              iframe.parentNode.hidePopup();

              // Show the Loop panel if the user wants it.
              var _event$detail = event.detail;var accept = _event$detail.accept;var stop = _event$detail.stop;
              if (accept) {
                LoopUI.openPanel();}


              // Stop showing the panel if the user says so.
              if (stop) {
                LoopUI.removeCopyPanel();
                Services.prefs.setBoolPref("loop.copy.shown", true);}


              // Generate the appropriate NO_AGAIN, NO_NEVER, YES_AGAIN,
              // YES_NEVER probe based on the user's action.
              var probe = (accept ? "YES" : "NO") + "_" + (stop ? "NEVER" : "AGAIN");
              addTelemetry(probe);

              // For testing, indicate that handling the click has finished.
              try {
                onClickHandled(event.detail);} 
              catch (ex) {
                // Do nothing.
              }});});};




        // Override the default behavior of the copy command.
        var controller = gURLBar._copyCutController;
        controller._doCommand = controller.doCommand;
        controller.doCommand = function () {
          // Do the normal behavior first.
          controller._doCommand.apply(controller, _arguments);

          // Remove the panel if the user has seen it enough times.
          var showLimit = Services.prefs.getIntPref("loop.copy.showLimit");
          if (showLimit <= 0) {
            LoopUI.removeCopyPanel();
            return;}


          // Don't bother prompting the user if already sharing.
          if (_this8.MozLoopService.screenShareActive) {
            return;}


          // Update various counters.
          Services.prefs.setIntPref("loop.copy.showLimit", showLimit - 1);
          addTelemetry("SHOWN");

          // Open up the copy panel at the loop button.
          LoopUI.PanelFrame.showPopup(window, LoopUI.toolbarButton.anchor, "loop-copy", 
          null, "chrome://loop/content/panels/copy.html", null, onIframe);};}, 



      /**
       * Removes the copy panel copy hook and the panel.
       */
      removeCopyPanel: function removeCopyPanel() {
        var controller = gURLBar && gURLBar._copyCutController;
        if (controller && controller._doCommand) {
          controller.doCommand = controller._doCommand;
          delete controller._doCommand;}


        var copy = document.getElementById("loop-copy-notification-panel");
        if (copy) {
          copy.parentNode.removeChild(copy);}}, 



      // Implements nsIObserver
      observe: function observe(subject, topic, data) {
        if (topic != "loop-status-changed") {
          return;}

        this.updateToolbarState(data);}, 


      /**
       * Updates the toolbar/menu-button state to reflect Loop status. This should
       * not be called from the hidden window.
       *
       * @param {string} [aReason] Some states are only shown if
       *                           a related reason is provided.
       *
       *                 aReason="login": Used after a login is completed
       *                   successfully. This is used so the state can be
       *                   temporarily shown until the next state change.
       */
      updateToolbarState: function updateToolbarState() {var _this9 = this;var aReason = arguments.length <= 0 || arguments[0] === undefined ? null : arguments[0];
        if (!this.toolbarButton.node) {
          return;}

        var state = "";
        var mozL10nId = "loop-call-button3";
        var suffix = ".tooltiptext";
        if (this.MozLoopService.errors.size) {
          state = "error";
          mozL10nId += "-error";} else 
        if (this.isSlideshowOpen) {
          state = "slideshow";
          suffix = ".label";} else 
        if (this.MozLoopService.screenShareActive) {
          state = "action";
          mozL10nId += "-screensharing";} else 
        if (aReason == "login" && this.MozLoopService.userProfile) {
          state = "active";
          mozL10nId += "-active";
          suffix += "2";} else 
        if (this.MozLoopService.doNotDisturb) {
          state = "disabled";
          mozL10nId += "-donotdisturb";} else 
        if (this.MozLoopService.roomsParticipantsCount > 0) {
          state = "active";
          this.roomsWithNonOwners().then(function (roomsWithNonOwners) {
            if (roomsWithNonOwners.length > 0) {
              mozL10nId += "-participantswaiting";} else 
            {
              mozL10nId += "-active";}


            suffix += "2";
            _this9.updateTooltiptext(mozL10nId + suffix);
            _this9.toolbarButton.node.setAttribute("state", state);});

          return;} else 
        {
          suffix += "2";}


        this.toolbarButton.node.setAttribute("state", state);
        this.updateTooltiptext(mozL10nId + suffix);}, 


      /**
       * Updates the tootltiptext to reflect Loop status. This should not be called
       * from the hidden window.
       *
       * @param {string} [mozL10nId] l10n ID that refelct the current
       *                           Loop status.
       */
      updateTooltiptext: function updateTooltiptext(mozL10nId) {
        this.toolbarButton.node.setAttribute("tooltiptext", mozL10nId);
        var tooltiptext = CustomizableUI.getLocalizedProperty(this.toolbarButton, "tooltiptext");
        this.toolbarButton.node.setAttribute("tooltiptext", tooltiptext);}, 


      /**
       * Show a desktop notification when 'do not disturb' isn't enabled.
       *
       * @param {Object} options Set of options that may tweak the appearance and
       *                         behavior of the notification.
       *                         Option params:
       *                         - {String}   title       Notification title message
       *                         - {String}   [message]   Notification body text
       *                         - {String}   [icon]      Notification icon
       *                         - {String}   [sound]     Sound to play
       *                         - {String}   [selectTab] Tab to select when the panel
       *                                                  opens
       *                         - {Function} [onclick]   Callback to invoke when
       *                                                  the notification is clicked.
       *                                                  Opens the panel by default.
       */
      showNotification: function showNotification(options) {var _this10 = this;
        if (this.MozLoopService.doNotDisturb) {
          return;}


        if (!options.title) {
          throw new Error("Missing title, can not display notification");}


        var notificationOptions = { 
          body: options.message || "" };

        if (options.icon) {
          notificationOptions.icon = options.icon;}

        if (options.sound) {
          // This will not do anything, until bug bug 1105222 is resolved.
          notificationOptions.mozbehavior = { 
            soundFile: "" };

          this.playSound(options.sound);}


        var notification = new window.Notification(options.title, notificationOptions);
        notification.addEventListener("click", function () {
          if (window.closed) {
            return;}


          try {
            window.focus();} 
          catch (ex) {}
          // Do nothing.


          // We need a setTimeout here, otherwise the panel won't show after the
          // window received focus.
          window.setTimeout(function () {
            if (typeof options.onclick == "function") {
              options.onclick();} else 
            {
              // Open the Loop panel as a default action.
              _this10.openPanel(null, options.selectTab || null);}}, 

          0);});}, 



      /**
       * Play a sound in this window IF there's no sound playing yet.
       *
       * @param {String} name Name of the sound, like 'ringtone' or 'room-joined'
       */
      playSound: function playSound(name) {var _this11 = this;
        if (this.ActiveSound || this.MozLoopService.doNotDisturb) {
          return;}


        this.activeSound = new window.Audio();
        this.activeSound.src = "chrome://loop/content/shared/sounds/" + name + ".ogg";
        this.activeSound.load();
        this.activeSound.play();

        this.activeSound.addEventListener("ended", function () {
          _this11.activeSound = undefined;}, 
        false);}, 


      /**
       * Start listening to selected tab changes and notify any content page that's
       * listening to 'BrowserSwitch' push messages.  Also sets up a "joined"
       * and "left" listener for LoopRooms so that we can toggle the infobar
       * sharing messages when people come and go.
       *
       * @param {(String)} roomToken  The current room that the link generator is connecting to.
       */
      startBrowserSharing: function startBrowserSharing(roomToken) {var _this12 = this;
        if (!this._listeningToTabSelect) {
          gBrowser.tabContainer.addEventListener("TabSelect", this);
          this._listeningToTabSelect = true;

          titleChangedListener = this.handleDOMTitleChanged.bind(this);

          this._roomsListener = this.handleRoomJoinedOrLeft.bind(this);

          this.LoopRooms.on("joined", this._roomsListener);
          this.LoopRooms.on("left", this._roomsListener);

          // Watch for title changes as opposed to location changes as more
          // metadata about the page is available when this event fires.
          this.mm.addMessageListener("loop@mozilla.org:DOMTitleChanged", 
          titleChangedListener);

          this._browserSharePaused = false;

          // Add this event to the parent gBrowser to avoid adding and removing
          // it for each individual tab's browsers.
          gBrowser.addEventListener("mousemove", this);
          gBrowser.addEventListener("click", this);}


        this._currentRoomToken = roomToken;
        this._maybeShowBrowserSharingInfoBar(roomToken);

        // Get the first window Id for the listener.
        var browser = gBrowser.selectedBrowser;
        return new Promise(function (resolve) {
          if (browser.outerWindowID) {
            resolve(browser.outerWindowID);
            return;}


          browser.messageManager.addMessageListener("Browser:Init", function initListener() {
            browser.messageManager.removeMessageListener("Browser:Init", initListener);
            resolve(browser.outerWindowID);});}).

        then(function (outerWindowID) {return (
            _this12.LoopAPI.broadcastPushMessage("BrowserSwitch", outerWindowID));});}, 


      /**
       * Stop listening to selected tab changes.
       */
      stopBrowserSharing: function stopBrowserSharing() {
        if (!this._listeningToTabSelect) {
          return;}


        this._hideBrowserSharingInfoBar();
        gBrowser.tabContainer.removeEventListener("TabSelect", this);
        this.LoopRooms.off("joined", this._roomsListener);
        this.LoopRooms.off("left", this._roomsListener);

        if (titleChangedListener) {
          this.mm.removeMessageListener("loop@mozilla.org:DOMTitleChanged", 
          titleChangedListener);
          titleChangedListener = null;}


        // Remove shared pointers related events
        gBrowser.removeEventListener("mousemove", this);
        gBrowser.removeEventListener("click", this);
        this.removeRemoteCursor();

        this._listeningToTabSelect = false;
        this._browserSharePaused = false;
        this._currentRoomToken = null;}, 


      /**
       *  If sharing is active, paints and positions the remote cursor
       *  over the screen
       *
       *  @param cursorData Object with the correct position for the cursor
       *                    {
       *                      ratioX: position on the X axis (percentage value)
       *                      ratioY: position on the Y axis (percentage value)
       *                    }
       */
      addRemoteCursor: function addRemoteCursor(cursorData) {
        if (this._browserSharePaused || !this._listeningToTabSelect) {
          return;}


        var browser = gBrowser.selectedBrowser;
        var cursor = document.getElementById("loop-remote-cursor");
        if (!cursor) {
          // Create a container to keep the pointer inside.
          // This allows us to hide the overflow when out of bounds.
          var cursorContainer = document.createElement("div");
          cursorContainer.setAttribute("id", "loop-remote-cursor-container");

          cursor = document.createElement("img");
          cursor.setAttribute("id", "loop-remote-cursor");
          cursorContainer.appendChild(cursor);
          // Note that browser.parent is a xul:stack so container will use
          // 100% of space if no other constrains added.
          browser.parentNode.appendChild(cursorContainer);}


        // Update the cursor's position with CSS.
        cursor.style.left = 
        Math.abs(cursorData.ratioX * browser.boxObject.width) + "px";
        cursor.style.top = 
        Math.abs(cursorData.ratioY * browser.boxObject.height) + "px";}, 


      /**
       *  Adds the ripple effect animation to the cursor to show a click on the
       *  remote end of the conversation.
       *  Will only add it when:
       *  - A click is received (cursorData = true)
       *  - Sharing is active (this._listeningToTabSelect = true)
       *  - Remote cursor is being painted (cursor != undefined)
       *
       *  @param clickData bool click event
       */
      clickRemoteCursor: function clickRemoteCursor(clickData) {
        if (!clickData || !this._listeningToTabSelect) {
          return;}


        var class_name = "clicked";
        var cursor = document.getElementById("loop-remote-cursor");
        if (!cursor) {
          return;}


        cursor.classList.add(class_name);

        // after the proper time, we get rid of the animation
        window.setTimeout(function () {
          cursor.classList.remove(class_name);}, 
        CURSOR_CLICK_DELAY);}, 


      /**
       *  Removes the remote cursor from the screen
       */
      removeRemoteCursor: function removeRemoteCursor() {
        var cursor = document.getElementById("loop-remote-cursor");

        if (cursor) {
          cursor.parentNode.removeChild(cursor);}}, 



      /**
       * Helper function to fetch a localized string via the MozLoopService API.
       * It's currently inconveniently wrapped inside a string of stringified JSON.
       *
       * @param  {String} key The element id to get strings for.
       * @return {String}
       */
      _getString: function _getString(key) {
        var str = this.MozLoopService.getStrings(key);
        if (str) {
          str = JSON.parse(str).textContent;}

        return str;}, 


      /**
       * Set correct strings for infobar notification based on if paused or empty.
       */

      _setInfoBarStrings: function _setInfoBarStrings(nonOwnerParticipants, sharePaused) {
        var message = void 0;
        if (nonOwnerParticipants) {
          // More than just the owner in the room.
          message = this._getString(
          sharePaused ? "infobar_screenshare_stop_sharing_message2" : 
          "infobar_screenshare_browser_message3");} else 

        {
          // Just the owner in the room.
          message = this._getString(
          sharePaused ? "infobar_screenshare_stop_no_guest_message" : 
          "infobar_screenshare_no_guest_message");}

        var label = this._getString(
        sharePaused ? "infobar_button_restart_label2" : "infobar_button_stop_label2");
        var accessKey = this._getString(
        sharePaused ? "infobar_button_restart_accesskey" : "infobar_button_stop_accesskey");

        return { message: message, label: label, accesskey: accessKey };}, 


      /**
       * Indicates if tab sharing is paused.
       * Set by tab pause button, startBrowserSharing and stopBrowserSharing.
       * Defaults to false as link generator(owner) enters room we are sharing tabs.
       */
      _browserSharePaused: false, 

      /**
       * Stores details about the last notification.
       *
       * @type {Object}
       */
      _lastNotification: {}, 

      /**
       * Used to determine if the browser sharing info bar is currently being
       * shown or not.
       */
      _showingBrowserSharingInfoBar: function _showingBrowserSharingInfoBar() {
        var browser = gBrowser.selectedBrowser;
        var box = gBrowser.getNotificationBox(browser);
        var notification = box.getNotificationWithValue(kBrowserSharingNotificationId);

        return !!notification;}, 


      /**
       * Shows an infobar notification at the top of the browser window that warns
       * the user that their browser tabs are being broadcasted through the current
       * conversation.
       * @param  {String} currentRoomToken Room we are currently joined.
       * @return {void}
       */
      _maybeShowBrowserSharingInfoBar: function _maybeShowBrowserSharingInfoBar(currentRoomToken) {var _this13 = this;
        var participantsCount = this.LoopRooms.getNumParticipants(currentRoomToken);

        if (this._showingBrowserSharingInfoBar()) {
          // When we first open the room, there will be one or zero partipicants
          // in the room. The notification box changes when there's more than one,
          // so work that out here.
          var notAlone = participantsCount > 1;
          var previousNotAlone = this._lastNotification.participantsCount <= 1;

          // If we're not actually changing the notification bar, then don't
          // re-display it. This avoids the bar sliding in twice.
          if (notAlone !== previousNotAlone && 
          this._browserSharePaused === this._lastNotification.paused) {
            return;}


          this._hideBrowserSharingInfoBar();}


        var initStrings = this._setInfoBarStrings(participantsCount > 1, this._browserSharePaused);

        var box = gBrowser.getNotificationBox();
        var bar = box.appendNotification(
        initStrings.message, // label
        kBrowserSharingNotificationId, // value
        // Icon defined in browser theme CSS.
        null, // image
        box.PRIORITY_WARNING_LOW, // priority
        [{ // buttons (Pause, Stop)
          label: initStrings.label, 
          accessKey: initStrings.accesskey, 
          isDefault: false, 
          callback: function callback(event, buttonInfo, buttonNode) {
            _this13._browserSharePaused = !_this13._browserSharePaused;
            var guestPresent = _this13.LoopRooms.getNumParticipants(_this13._currentRoomToken) > 1;
            var stringObj = _this13._setInfoBarStrings(guestPresent, _this13._browserSharePaused);
            bar.label = stringObj.message;
            bar.classList.toggle("paused", _this13._browserSharePaused);
            buttonNode.label = stringObj.label;
            buttonNode.accessKey = stringObj.accesskey;
            LoopUI.MozLoopService.toggleBrowserSharing(_this13._browserSharePaused);
            if (_this13._browserSharePaused) {
              // if paused we stop sharing remote cursors
              _this13.removeRemoteCursor();}

            return true;}, 

          type: "pause" }, 

        { 
          label: this._getString("infobar_button_disconnect_label"), 
          accessKey: this._getString("infobar_button_disconnect_accesskey"), 
          isDefault: true, 
          callback: function callback() {
            _this13.removeRemoteCursor();
            _this13._hideBrowserSharingInfoBar();
            LoopUI.MozLoopService.hangupAllChatWindows();}, 

          type: "stop" }]);



        // Sets 'paused' class if needed.
        bar.classList.toggle("paused", !!this._browserSharePaused);

        // Keep showing the notification bar until the user explicitly closes it.
        bar.persistence = -1;

        this._lastNotification.participantsCount = participantsCount;
        this._lastNotification.paused = this._browserSharePaused;}, 


      /**
       * Hides the infobar, permanantly if requested.
       *
       * @param   {Object}  browser Optional link to the browser we want to
       *                    remove the infobar from. If not present, defaults
       *                    to current browser instance.
       * @return  {Boolean} |true| if the infobar was hidden here.
       */
      _hideBrowserSharingInfoBar: function _hideBrowserSharingInfoBar(browser) {
        browser = browser || gBrowser.selectedBrowser;
        var box = gBrowser.getNotificationBox(browser);
        var notification = box.getNotificationWithValue(kBrowserSharingNotificationId);
        var removed = false;
        if (notification) {
          box.removeNotification(notification);
          removed = true;}


        return removed;}, 


      /**
       * Broadcast 'BrowserSwitch' event.
       */
      _notifyBrowserSwitch: function _notifyBrowserSwitch() {
        // Get the first window Id for the listener.
        this.LoopAPI.broadcastPushMessage("BrowserSwitch", 
        gBrowser.selectedBrowser.outerWindowID);}, 


      /**
       * Handles updating of the sharing infobar when the room participants
       * change.
       */
      handleRoomJoinedOrLeft: function handleRoomJoinedOrLeft() {
        // Don't attempt to show it if we're not actively sharing.
        if (!this._listeningToTabSelect) {
          return;}

        this._maybeShowBrowserSharingInfoBar(this._currentRoomToken);}, 


      /**
       * Handles events from the frame script.
       *
       * @param {Object} message The message received from the frame script.
       */
      handleDOMTitleChanged: function handleDOMTitleChanged(message) {
        if (!this._listeningToTabSelect || this._browserSharePaused) {
          return;}


        if (gBrowser.selectedBrowser == message.target) {
          // Get the new title of the shared tab
          this._notifyBrowserSwitch();}}, 



      /**
       * Handles events from gBrowser.
       */
      handleEvent: function handleEvent(event) {

        switch (event.type) {
          case "TabSelect":{
              var wasVisible = false;
              // Hide the infobar from the previous tab.
              if (event.detail.previousTab) {
                wasVisible = this._hideBrowserSharingInfoBar(
                event.detail.previousTab.linkedBrowser);
                // And remove the cursor.
                this.removeRemoteCursor();}


              // We've changed the tab, so get the new window id.
              this._notifyBrowserSwitch();

              if (wasVisible) {
                // If the infobar was visible before, we should show it again after the
                // switch.
                this._maybeShowBrowserSharingInfoBar(this._currentRoomToken);}

              break;}

          case "mousemove":
            this.handleMousemove(event);
            break;
          case "click":
            this.handleMouseClick(event);
            break;}}, 



      /**
       * Handles mousemove events from gBrowser and send a broadcast message
       * with all the data needed for sending link generator cursor position
       * through the sdk.
       */
      handleMousemove: function handleMousemove(event) {
        // Won't send events if not sharing (paused or not started).
        if (this._browserSharePaused || !this._listeningToTabSelect) {
          return;}


        // Only update every so often.
        var now = Date.now();
        if (now - this.lastCursorTime < CURSOR_MIN_INTERVAL) {
          return;}

        this.lastCursorTime = now;

        // Skip the update if cursor is out of bounds or didn't move much.
        var browserBox = gBrowser.selectedBrowser.boxObject;
        var deltaX = event.screenX - browserBox.screenX;
        var deltaY = event.screenY - browserBox.screenY;
        if (deltaX < 0 || deltaX > browserBox.width || 
        deltaY < 0 || deltaY > browserBox.height || 
        Math.abs(deltaX - this.lastCursorX) < CURSOR_MIN_DELTA && 
        Math.abs(deltaY - this.lastCursorY) < CURSOR_MIN_DELTA) {
          return;}

        this.lastCursorX = deltaX;
        this.lastCursorY = deltaY;

        this.LoopAPI.broadcastPushMessage("CursorPositionChange", { 
          ratioX: deltaX / browserBox.width, 
          ratioY: deltaY / browserBox.height });}, 



      /**
       * Handles mouse click events from gBrowser and send a broadcast message
       * with all the data needed for sending link generator cursor click position
       * through the sdk.
       */
      handleMouseClick: function handleMouseClick() {
        // We want to stop sending events if sharing is paused.
        if (this._browserSharePaused) {
          return;}


        this.LoopAPI.broadcastPushMessage("CursorClick");}, 


      /**
       * Fetch the favicon of the currently selected tab in the format of a data-uri.
       *
       * @param  {Function} callback Function to be invoked with an error object as
       *                             its first argument when an error occurred or
       *                             a string as second argument when the favicon
       *                             has been fetched.
       */
      getFavicon: function getFavicon(callback) {
        var pageURI = gBrowser.selectedTab.linkedBrowser.currentURI.spec;
        // If the tab page’s url starts with http(s), fetch icon.
        if (!/^https?:/.test(pageURI)) {
          callback();
          return;}


        this.PlacesUtils.promiseFaviconLinkUrl(pageURI).then(function (uri) {
          // We XHR the favicon to get a File object, which we can pass to the FileReader
          // object. The FileReader turns the File object into a data-uri.
          var xhr = xhrClass.createInstance(Ci.nsIXMLHttpRequest);
          xhr.open("get", uri.spec, true);
          xhr.responseType = "blob";
          xhr.overrideMimeType("image/x-icon");
          xhr.onload = function () {
            if (xhr.status != 200) {
              callback(new Error("Invalid status code received for favicon XHR: " + xhr.status));
              return;}


            var reader = new FileReader();
            reader.onload = reader.onload = function () {return callback(null, reader.result);};
            reader.onerror = callback;
            reader.readAsDataURL(xhr.response);};

          xhr.onerror = callback;
          xhr.send();}).
        catch(function (err) {
          callback(err || new Error("No favicon found"));});} };




    XPCOMUtils.defineLazyModuleGetter(LoopUI, "hookWindowCloseForPanelClose", "resource://gre/modules/MozSocialAPI.jsm");
    XPCOMUtils.defineLazyModuleGetter(LoopUI, "LoopAPI", "chrome://loop/content/modules/MozLoopAPI.jsm");
    XPCOMUtils.defineLazyModuleGetter(LoopUI, "LoopRooms", "chrome://loop/content/modules/LoopRooms.jsm");
    XPCOMUtils.defineLazyModuleGetter(LoopUI, "MozLoopService", "chrome://loop/content/modules/MozLoopService.jsm");
    XPCOMUtils.defineLazyModuleGetter(LoopUI, "PanelFrame", "resource:///modules/PanelFrame.jsm");
    XPCOMUtils.defineLazyModuleGetter(LoopUI, "PlacesUtils", "resource://gre/modules/PlacesUtils.jsm");

    LoopUI.init();
    window.LoopUI = LoopUI;

    // Export the Throttler to allow tests to overwrite parts of it.
    window.LoopThrottler = Throttler;}, 


  /**
   * Take any steps to remove UI or anything from the browser window
   * document.getElementById() etc. will work here.
   *
   * @param {Object} window The window to remove the integration from.
   */
  tearDownBrowserUI: function tearDownBrowserUI(window) {
    if (window.LoopUI) {
      window.LoopUI.removeCopyPanel();
      window.LoopUI.removeMenuItem();

      // This stops the frame script being loaded to new tabs, but doesn't
      // remove it from existing tabs (there's no way to do that).
      window.LoopUI.mm.removeDelayedFrameScript(FRAME_SCRIPT);

      // XXX Bug 1229352 - Add in tear-down of the panel.
    }}, 


  // nsIWindowMediatorListener functions.
  onOpenWindow: function onOpenWindow(xulWindow) {
    // A new window has opened.
    var domWindow = xulWindow.QueryInterface(Ci.nsIInterfaceRequestor).
    getInterface(Ci.nsIDOMWindow);

    // Wait for it to finish loading.
    domWindow.addEventListener("load", function listener() {
      domWindow.removeEventListener("load", listener, false);

      // If this is a browser window then setup its UI.
      if (domWindow.document.documentElement.getAttribute("windowtype") == "navigator:browser") {
        WindowListener.setupBrowserUI(domWindow);}}, 

    false);}, 


  onCloseWindow: function onCloseWindow() {}, 


  onWindowTitleChange: function onWindowTitleChange() {} };



/**
 * Provide a way to throttle functionality using DNS to distribute 3 numbers for
 * various distributions channels. DNS is used to scale distribution of the
 * numbers as an A record pointing to a loopback address (127.*.*.*). Prefs are
 * used to control behavior (what domain to check) and keep state (a ticket
 * number to track if it needs to initialize, to wait for its turn, or is
 * completed).
 */
var Throttler = { 
  // Each 8-bit block of the IP address allows for 0% rollout (value 0) to 100%
  // rollout (value 255).
  TICKET_LIMIT: 255, 

  // Allow the DNS service to be overwritten for testing.
  _dns: Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService), 

  /**
   * Check if a given feature should be throttled or not.
   * @param {string} [prefPrefix] Start of the preference name for the feature.
   * @return {Promise} Resolved on success, and rejected on throttled.
   */
  check: function check(prefPrefix) {var _this14 = this;
    return new Promise(function (resolve, reject) {
      // Initialize the ticket (0-254) if it doesn't have a valid value yet.
      var prefTicket = prefPrefix + ".ticket";
      var ticket = Services.prefs.getIntPref(prefTicket);
      if (ticket < 0) {
        ticket = Math.floor(Math.random() * _this14.TICKET_LIMIT);
        Services.prefs.setIntPref(prefTicket, ticket);}

      // Short circuit if the special ticket value indicates we're good to go.
      else if (ticket >= _this14.TICKET_LIMIT) {
          resolve();
          return;}


      // Handle responses from the DNS resolution service request.
      var onDNS = function onDNS(request, record) {
        // Failed to get A-record, so skip for now.
        if (record === null) {
          reject();
          return;}


        // Ensure we have a special loopback value before checking other blocks.
        var ipBlocks = record.getNextAddrAsString().split(".");
        if (ipBlocks[0] !== "127") {
          reject();
          return;}


        // Use a specific part of the A-record IP address depending on the
        // channel. I.e., 127.[release/other].[beta].[aurora/nightly].
        var index = 1;
        switch (Services.prefs.getCharPref("app.update.channel")) {
          case "beta":
            index = 2;
            break;
          case "aurora":
          case "nightly":
            index = 3;
            break;}


        // Select the 1 out of 4 parts of the "."-separated IP address to check
        // if the 8-bit threshold (0-255) exceeds the ticket (0-254).
        if (ticket < ipBlocks[index]) {
          // Remember that we're good to go to avoid future DNS checks.
          Services.prefs.setIntPref(prefTicket, _this14.TICKET_LIMIT);
          resolve();} else 

        {
          reject();}};



      // Look up the DNS A-record of a throttler hostname to decide to show.
      _this14._dns.asyncResolve(Services.prefs.getCharPref(prefPrefix + ".throttler"), 
      _this14._dns.RESOLVE_DISABLE_IPV6, onDNS, Services.tm.mainThread);});} };




/**
 * Creates the loop button on the toolbar. Due to loop being a system-addon
 * CustomizableUI already has a placement location for the button, so that
 * we can be on the toolbar.
 */
function createLoopButton() {
  CustomizableUI.createWidget({ 
    id: "loop-button", 
    type: "custom", 
    label: "loop-call-button3.label", 
    tooltiptext: "loop-call-button3.tooltiptext2", 
    privateBrowsingTooltiptext: "loop-call-button3-pb.tooltiptext", 
    defaultArea: CustomizableUI.AREA_NAVBAR, 
    removable: true, 
    onBuild: function onBuild(aDocument) {
      // If we're not supposed to see the button, return zip.
      if (!Services.prefs.getBoolPref("loop.enabled")) {
        return null;}


      var isWindowPrivate = PrivateBrowsingUtils.isWindowPrivate(aDocument.defaultView);

      var node = aDocument.createElementNS(kNSXUL, "toolbarbutton");
      node.setAttribute("id", this.id);
      node.classList.add("toolbarbutton-1");
      node.classList.add("chromeclass-toolbar-additional");
      node.classList.add("badged-button");
      node.setAttribute("label", CustomizableUI.getLocalizedProperty(this, "label"));
      if (isWindowPrivate) {
        node.setAttribute("disabled", "true");}

      var tooltiptext = isWindowPrivate ? 
      CustomizableUI.getLocalizedProperty(this, "privateBrowsingTooltiptext", 
      [CustomizableUI.getLocalizedProperty(this, "label")]) : 
      CustomizableUI.getLocalizedProperty(this, "tooltiptext");
      node.setAttribute("tooltiptext", tooltiptext);
      node.setAttribute("removable", "true");
      node.addEventListener("command", function (event) {
        aDocument.defaultView.LoopUI.togglePanel(event);});


      return node;} });}




/**
 * Loads the default preferences from the prefs file. This loads the preferences
 * into the default branch, so they don't appear as user preferences.
 */
function loadDefaultPrefs() {
  var branch = Services.prefs.getDefaultBranch("");
  Services.scriptloader.loadSubScript("chrome://loop/content/preferences/prefs.js", { 
    pref: function pref(key, val) {
      // If a previously set default pref exists don't overwrite it.  This can
      // happen for ESR or distribution.ini.
      if (branch.getPrefType(key) != branch.PREF_INVALID) {
        return;}

      switch (typeof val === "undefined" ? "undefined" : _typeof(val)) {
        case "boolean":
          branch.setBoolPref(key, val);
          break;
        case "number":
          branch.setIntPref(key, val);
          break;
        case "string":
          branch.setCharPref(key, val);
          break;}} });




  if (Services.vc.compare(Services.appinfo.version, "47.0a1") < 0) {
    branch.setBoolPref("loop.remote.autostart", false);}}



/**
 * Called when the add-on is started, e.g. when installed or when Firefox starts.
 */
function startup(data) {
  // Record the add-on version for when the UI is initialised.
  WindowListener.addonVersion = data.version;

  loadDefaultPrefs();
  if (!Services.prefs.getBoolPref("loop.enabled")) {
    return;}


  createLoopButton();

  // Attach to hidden window (for OS X).
  if (AppConstants.platform == "macosx") {
    try {
      WindowListener.setupBrowserUI(Services.appShell.hiddenDOMWindow);} 
    catch (ex) {(function () {
        // Hidden window didn't exist, so wait until startup is done.
        var topic = "browser-delayed-startup-finished";
        Services.obs.addObserver(function observer() {
          Services.obs.removeObserver(observer, topic);
          WindowListener.setupBrowserUI(Services.appShell.hiddenDOMWindow);}, 
        topic, false);})();}}



  // Attach to existing browser windows, for modifying UI.
  var wm = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);
  var windows = wm.getEnumerator("navigator:browser");
  while (windows.hasMoreElements()) {
    var domWindow = windows.getNext().QueryInterface(Ci.nsIDOMWindow);
    WindowListener.setupBrowserUI(domWindow);}


  // Wait for any new browser windows to open.
  wm.addListener(WindowListener);

  // Load our stylesheets.
  var styleSheetService = Cc["@mozilla.org/content/style-sheet-service;1"].
  getService(Components.interfaces.nsIStyleSheetService);
  var sheets = [
  "chrome://loop-shared/skin/loop.css", 
  "chrome://loop/skin/platform.css"];var _iteratorNormalCompletion2 = true;var _didIteratorError2 = false;var _iteratorError2 = undefined;try {



    for (var _iterator2 = sheets[Symbol.iterator](), _step2; !(_iteratorNormalCompletion2 = (_step2 = _iterator2.next()).done); _iteratorNormalCompletion2 = true) {var sheet = _step2.value;
      var styleSheetURI = Services.io.newURI(sheet, null, null);
      styleSheetService.loadAndRegisterSheet(styleSheetURI, 
      styleSheetService.AUTHOR_SHEET);}} catch (err) {_didIteratorError2 = true;_iteratorError2 = err;} finally {try {if (!_iteratorNormalCompletion2 && _iterator2.return) {_iterator2.return();}} finally {if (_didIteratorError2) {throw _iteratorError2;}}}}



/**
 * Called when the add-on is shutting down, could be for re-installation
 * or just uninstall.
 */
function shutdown(data, reason) {
  // Close any open chat windows
  Cu.import("resource:///modules/Chat.jsm");
  var isLoopURL = function isLoopURL(_ref) {var src = _ref.src;return (/^about:loopconversation#/.test(src));};
  [].concat(_toConsumableArray(Chat.chatboxes)).filter(isLoopURL).forEach(function (chatbox) {
    chatbox.content.contentWindow.close();});


  // Detach from hidden window (for OS X).
  if (AppConstants.platform == "macosx") {
    WindowListener.tearDownBrowserUI(Services.appShell.hiddenDOMWindow);}


  // Detach from browser windows.
  var wm = Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);
  var windows = wm.getEnumerator("navigator:browser");
  while (windows.hasMoreElements()) {
    var domWindow = windows.getNext().QueryInterface(Ci.nsIDOMWindow);
    WindowListener.tearDownBrowserUI(domWindow);}


  // Stop waiting for browser windows to open.
  wm.removeListener(WindowListener);

  // If the app is shutting down, don't worry about cleaning up, just let
  // it fade away...
  if (reason == APP_SHUTDOWN) {
    return;}


  CustomizableUI.destroyWidget("loop-button");

  // Unload stylesheets.
  var styleSheetService = Cc["@mozilla.org/content/style-sheet-service;1"].
  getService(Components.interfaces.nsIStyleSheetService);
  var sheets = ["chrome://loop/content/addon/css/loop.css", 
  "chrome://loop/skin/platform.css"];var _iteratorNormalCompletion3 = true;var _didIteratorError3 = false;var _iteratorError3 = undefined;try {
    for (var _iterator3 = sheets[Symbol.iterator](), _step3; !(_iteratorNormalCompletion3 = (_step3 = _iterator3.next()).done); _iteratorNormalCompletion3 = true) {var sheet = _step3.value;
      var styleSheetURI = Services.io.newURI(sheet, null, null);
      if (styleSheetService.sheetRegistered(styleSheetURI, 
      styleSheetService.AUTHOR_SHEET)) {
        styleSheetService.unregisterSheet(styleSheetURI, 
        styleSheetService.AUTHOR_SHEET);}}



    // Unload modules.
  } catch (err) {_didIteratorError3 = true;_iteratorError3 = err;} finally {try {if (!_iteratorNormalCompletion3 && _iterator3.return) {_iterator3.return();}} finally {if (_didIteratorError3) {throw _iteratorError3;}}}Cu.unload("chrome://loop/content/modules/MozLoopAPI.jsm");
  Cu.unload("chrome://loop/content/modules/LoopRooms.jsm");
  Cu.unload("chrome://loop/content/modules/MozLoopService.jsm");}


function install() {}

function uninstall() {}
