/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /*
  * ContentAreaObserver manages tracking the viewable area within the browser.
  * It also handles certain tasks like positioning of input elements within
  * content when the viewable area changes.
  *
  * ContentAreaObserver creates styles that content can apply and also fires
  * events when things change. The 'width' and 'height' properties of the
  * styles are updated whenever various parts of the UI are resized.
  *
  *  styles: window-width, window-height
  *  events: SizeChanged
  *
  *  The innerWidth/innerHeight of the main chrome window.
  *
  *  styles: content-width, content-height
  *  events: ContentSizeChanged
  *
  *  The area of the window dedicated to web content; this is equal to the
  *  innerWidth/Height minus any toolbars or similar chrome.
  *
  *  styles: viewable-width, viewable-height
  *  events: ViewableSizeChanged
  *
  *  The portion of the content area that is not obscured by the on-screen
  *  keyboard.
  *
  * ContentAreaObserver also manages soft keyboard related events. Events
  * fired include:
  *
  *  KeyboardChanged - fired after the visibility of the soft keyboard changes
  *  and after any view related changes are made to accomidate view dim
  *  changes as a result.
  *
  *  MozDeckOffsetChanging, MozDeckOffsetChanged - interim events fired when
  *  the browser deck is being repositioned to move focused elements out of
  *  the way of the keyboard.
  */

var ContentAreaObserver = {
  styles: {},
  _shiftAmount: 0,
  _deckTransitioning: false,

  /*
   * Properties
   */

  get width() {
    return window.innerWidth || 1366;
  },

  get height() {
    return window.innerHeight || 768;
  },

  get contentHeight() {
    return this._getContentHeightForWindow(this.height);
  },

  get contentTop () {
    return Elements.toolbar.getBoundingClientRect().bottom;
  },

  get viewableHeight() {
    return this._getViewableHeightForContent(this.contentHeight);
  },

  get isKeyboardOpened() {
    return MetroUtils.keyboardVisible;
  },

  get isKeyboardTransitioning() {
    return this._deckTransitioning;
  },

  /*
   * Public apis
   */

  init: function init() {
    window.addEventListener("resize", this, false);

    // Message manager msgs we listen for
    messageManager.addMessageListener("Content:RepositionInfoResponse", this);

    // Observer msgs we listen for
    Services.obs.addObserver(this, "metro_softkeyboard_shown", false);
    Services.obs.addObserver(this, "metro_softkeyboard_hidden", false);

    // setup initial values for browser form repositioning
    this._shiftBrowserDeck(0);

    // initialize our custom width and height styles
    this._initStyles();

    // apply default styling
    this.update();
  },

  shutdown: function shutdown() {
    messageManager.removeMessageListener("Content:RepositionInfoResponse", this);
    Services.obs.removeObserver(this, "metro_softkeyboard_shown");
    Services.obs.removeObserver(this, "metro_softkeyboard_hidden");
  },

  update: function cao_update (width, height) {
    let oldWidth = parseInt(this.styles["window-width"].width);
    let oldHeight = parseInt(this.styles["window-height"].height);

    let newWidth = width || this.width;
    let newHeight = height || this.height;

    if (newHeight == oldHeight && newWidth == oldWidth)
      return;

    this.styles["window-width"].width = newWidth + "px";
    this.styles["window-width"].maxWidth = newWidth + "px";
    this.styles["window-height"].height = newHeight + "px";
    this.styles["window-height"].maxHeight = newHeight + "px";

    this.updateContentArea(newWidth, this._getContentHeightForWindow(newHeight));
    this._disatchBrowserEvent("SizeChanged");
  },

  updateContentArea: function cao_updateContentArea (width, height) {
    let oldHeight = parseInt(this.styles["content-height"].height);
    let oldWidth = parseInt(this.styles["content-width"].width);

    let newWidth = width || this.width;
    let newHeight = height || this.contentHeight;

    if (Browser.selectedBrowser) {
      let notificationBox = Browser.getNotificationBox();

      // If a notification and navbar are visible together,
      // make the notification appear above the navbar.
      if (ContextUI.navbarVisible && !notificationBox.notificationsHidden &&
          notificationBox.allNotifications.length != 0) {
        newHeight -= Elements.navbar.getBoundingClientRect().height;
      }
    }

    if (newHeight == oldHeight && newWidth == oldWidth)
      return;

    this.styles["content-height"].height = newHeight + "px";
    this.styles["content-height"].maxHeight = newHeight + "px";
    this.styles["content-width"].width = newWidth + "px";
    this.styles["content-width"].maxWidth = newWidth + "px";

    this.updateViewableArea(newWidth, this._getViewableHeightForContent(newHeight));
    this._disatchBrowserEvent("ContentSizeChanged");
  },

  updateViewableArea: function cao_updateViewableArea (width, height) {
    let oldHeight = parseInt(this.styles["viewable-height"].height);
    let oldWidth = parseInt(this.styles["viewable-width"].width);

    let newWidth = width || this.width;
    let newHeight = height || this.viewableHeight;

    if (newHeight == oldHeight && newWidth == oldWidth)
      return;

    this.styles["viewable-height"].height = newHeight + "px";
    this.styles["viewable-height"].maxHeight = newHeight + "px";
    this.styles["viewable-width"].width = newWidth + "px";
    this.styles["viewable-width"].maxWidth = newWidth + "px";

    this.updateAppBarPosition();

    // Update the back/tab button states. If the keyboard is up
    // these are hidden.
    BrowserUI._updateButtons();

    this._disatchBrowserEvent("ViewableSizeChanged");
  },

  updateAppBarPosition: function updateAppBarPosition(aForceDown) {
    // Adjust the app and find bar position above the soft keyboard
    let keyboardHeight = aForceDown ? 0 : MetroUtils.keyboardHeight;
    Elements.navbar.style.bottom = keyboardHeight + "px";
    Elements.contextappbar.style.bottom = keyboardHeight + "px";
    Elements.findbar.style.bottom = keyboardHeight + "px";
  },

  /*
   * Called by BrowserUI right before we blur the nav bar edit. We use
   * this to get a head start on shuffling app bars around before the
   * soft keyboard transitions down.
   */
  navBarWillBlur: function navBarWillBlur() {
    this.updateAppBarPosition(true);
  },

  onBrowserCreated: function onBrowserCreated(aBrowser) {
    let notificationBox = aBrowser.parentNode.parentNode;
    notificationBox.classList.add("content-width");
    notificationBox.classList.add("content-height");
  },

  /*
   * Event handling
   */

  _onKeyboardDisplayChanging: function _onKeyboardDisplayChanging(aNewState) {
    if (aNewState) {
      Elements.stack.setAttribute("keyboardVisible", true);
    } else {
      Elements.stack.removeAttribute("keyboardVisible");
    }

    this.updateViewableArea();

    if (!aNewState) {
      this._shiftBrowserDeck(0);
      return;
    }

    // Request info about the target form element to see if we
    // need to reposition the browser above the keyboard.
    Browser.selectedBrowser.messageManager.sendAsyncMessage("Browser:RepositionInfoRequest", {
      viewHeight: this.viewableHeight,
    });
  },

  _onRepositionResponse: function _onRepositionResponse(aJsonMsg) {
    if (!aJsonMsg.reposition || !this.isKeyboardOpened) {
      this._shiftBrowserDeck(0);
      return;
    }
    this._shiftBrowserDeck(aJsonMsg.raiseContent);
  },

  observe: function cao_observe(aSubject, aTopic, aData) {
    // Note these are fired before the transition starts. Also per MS specs
    // we should not do anything "heavy" here. We have about 100ms before
    // windows just ignores the event and starts the animation.
    switch (aTopic) {
      case "metro_softkeyboard_hidden":
      case "metro_softkeyboard_shown":
        // Mark that we are in a transition state. Auto-complete and other
        // popups will wait for an event before displaying anything.
        this._deckTransitioning = true;
        // This also insures tap events are delivered before we fire
        // this event. Form repositioning requires the selection handler
        // be running before we send this.
        let self = this;
        let state = aTopic == "metro_softkeyboard_shown";
        Util.executeSoon(function () {
          self._onKeyboardDisplayChanging(state);
        });
        break;
    }
  },

  handleEvent: function cao_handleEvent(aEvent) {
    switch (aEvent.type) {
      case 'resize':
        if (aEvent.target != window)
          return;
        ContentAreaObserver.update();
        break;
    }
  },

  receiveMessage: function sh_receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "Content:RepositionInfoResponse":
        this._onRepositionResponse(aMessage.json);
        break;
    }
  },

  /*
   * Internal helpers
   */

  _shiftBrowserDeck: function _shiftBrowserDeck(aAmount) {
    if (aAmount == 0) {
      this._deckTransitioning = false;
      this._dispatchWindowEvent("KeyboardChanged", this.isKeyboardOpened);
    }

    if (this._shiftAmount == aAmount)
      return;

    this._shiftAmount = aAmount;
    this._dispatchWindowEvent("MozDeckOffsetChanging", aAmount);

    // Elements.browsers is the deck all browsers sit in
    let self = this;
    Elements.browsers.addEventListener("transitionend", function () {
      Elements.browsers.removeEventListener("transitionend", arguments.callee, true);
      self._deckTransitioning = false;
      self._dispatchWindowEvent("MozDeckOffsetChanged", aAmount);
      self._dispatchWindowEvent("KeyboardChanged", self.isKeyboardOpened);
    }, true);

    // selectAtPoint bug 858471
    //Elements.browsers.style.transform = "translateY(" + (-1 * aAmount) + "px)";
    Elements.browsers.style.marginTop = "" + (-1 * aAmount) + "px";
  },

  _dispatchWindowEvent: function _dispatchWindowEvent(aEventName, aDetail) {
    let event = document.createEvent("UIEvents");
    event.initUIEvent(aEventName, true, false, window, aDetail);
    window.dispatchEvent(event);
  },

  _disatchBrowserEvent: function (aName, aDetail) {
    setTimeout(function() {
      let event = document.createEvent("Events");
      event.initEvent(aName, true, false);
      Elements.browsers.dispatchEvent(event);
    }, 0);
  },

  _initStyles: function _initStyles() {
    let stylesheet = document.styleSheets[0];
    for (let style of ["window-width", "window-height",
                       "content-height", "content-width",
                       "viewable-height", "viewable-width"]) {
      let index = stylesheet.insertRule("." + style + " {}", stylesheet.cssRules.length);
      this.styles[style] = stylesheet.cssRules[index].style;
    }
  },

  _getContentHeightForWindow: function (windowHeight) {
    return windowHeight;
  },

  _getViewableHeightForContent: function (contentHeight) {
    let keyboardHeight = MetroUtils.keyboardHeight;
    return contentHeight - keyboardHeight;
  },

  _debugDumpDims: function _debugDumpDims() {
    let width = parseInt(this.styles["window-width"].width);
    let height = parseInt(this.styles["window-height"].height);
    Util.dumpLn("window:", width, height);
    width = parseInt(this.styles["content-width"].width);
    height = parseInt(this.styles["content-height"].height);
    Util.dumpLn("content:", width, height);
    width = parseInt(this.styles["viewable-width"].width);
    height = parseInt(this.styles["viewable-height"].height);
    Util.dumpLn("viewable:", width, height);
  }
};
