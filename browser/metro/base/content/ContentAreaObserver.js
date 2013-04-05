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
  */

var ContentAreaObserver = {
  styles: {},
  _keyboardState: false,

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

  /*
   * Public apis
   */

  init: function init() {
    window.addEventListener("resize", this, false);

    // Observer msgs we listen for
    Services.obs.addObserver(this, "metro_softkeyboard_shown", false);
    Services.obs.addObserver(this, "metro_softkeyboard_hidden", false);

    // initialize our custom width and height styles
    this._initStyles();

    // apply default styling
    this.update();
  },

  shutdown: function shutdown() {
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

    this._disatchBrowserEvent("ViewableSizeChanged");
  },

  onBrowserCreated: function onBrowserCreated(aBrowser) {
    aBrowser.classList.add("content-width");
    aBrowser.classList.add("content-height");
  },

  /*
   * Event handling
   */

  _onKeyboardDisplayChanging: function _onKeyboardDisplayChanging(aNewState) {
    this._keyboardState = aNewState;

    this._dispatchWindowEvent("KeyboardChanged", aNewState);

    this.updateViewableArea();
  },

  observe: function cao_observe(aSubject, aTopic, aData) {
    // Note these are fired before the transition starts. Also per MS specs
    // we should not do anything "heavy" here. We have about 100ms before
    // windows just ignores the event and starts the animation.
    switch (aTopic) {
      case "metro_softkeyboard_hidden":
      case "metro_softkeyboard_shown":
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

  /*
   * Internal helpers
   */

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
    let contextUIHeight = BrowserUI.isTabsOnly ? Elements.toolbar.getBoundingClientRect().bottom : 0;
    return windowHeight - contextUIHeight;
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
