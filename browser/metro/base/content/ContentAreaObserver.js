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
  
  // In desktop mode avoids breaking window dims before
  // the desktop window is displayed.
  get width() { return window.innerWidth || 1366; },
  get height() { return window.innerHeight || 768; },

  get contentHeight() {
    return this._getContentHeightForWindow(this.height);
  },

  get contentTop () {
    return Elements.toolbar.getBoundingClientRect().bottom;
  },

  get viewableHeight() {
    return this._getViewableHeightForContent(this.contentHeight);
  },

  get isKeyboardOpened() { return MetroUtils.keyboardVisible; },
  get hasVirtualKeyboard() { return true; },

  init: function cao_init() {
    window.addEventListener("resize", this, false);

    let os = Services.obs;
    os.addObserver(this, "metro_softkeyboard_shown", false);
    os.addObserver(this, "metro_softkeyboard_hidden", false);

    // Create styles for the following class names.  The 'width' and 'height'
    // properties of these styles are updated whenever various parts of the UI
    // are resized.
    //
    // * window-width, window-height: The innerWidth/innerHeight of the main
    //     chrome window.
    // * content-width, content-height: The area of the window dedicated to web
    //     content; this is equal to the innerWidth/Height minus any toolbars
    //     or similar chrome.
    // * viewable-width, viewable-height: The portion of the content area that
    //     is not obscured by the on-screen keyboard.
    let stylesheet = document.styleSheets[0];
    for (let style of ["window-width", "window-height",
                       "content-height", "content-width",
                       "viewable-height", "viewable-width"]) {
      let index = stylesheet.insertRule("." + style + " {}", stylesheet.cssRules.length);
      this.styles[style] = stylesheet.cssRules[index].style;
    }
    this.update();
  },

  uninit: function cao_uninit() {
    let os = Services.obs;
    os.removeObserver(this, "metro_softkeyboard_shown");
    os.removeObserver(this, "metro_softkeyboard_hidden");
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

    let isStartup = !oldHeight && !oldWidth;
    for (let i = Browser.tabs.length - 1; i >=0; i--) {
      let tab = Browser.tabs[i];
      let oldContentWindowWidth = tab.browser.contentWindowWidth;
      tab.updateViewportSize(newWidth, newHeight); // contentWindowWidth may change here.
      
      // Don't bother updating the zoom level on startup
      if (!isStartup) {
        // If the viewport width is still the same, the page layout has not
        // changed, so we can keep keep the same content on-screen.
        if (tab.browser.contentWindowWidth == oldContentWindowWidth)
          tab.restoreViewportPosition(oldWidth, newWidth);

        tab.updateDefaultZoomLevel();
      }
    }

    // We want to keep the current focused element into view if possible
    if (!isStartup) {
      let currentElement = document.activeElement;
      let [scrollbox, scrollInterface] = ScrollUtils.getScrollboxFromElement(currentElement);
      if (scrollbox && scrollInterface && currentElement && currentElement != scrollbox) {
        // retrieve the direct child of the scrollbox
        while (currentElement && currentElement.parentNode != scrollbox)
          currentElement = currentElement.parentNode;
  
        setTimeout(function() { scrollInterface.ensureElementIsVisible(currentElement) }, 0);
      }
    }

    this.updateContentArea(newWidth, this._getContentHeightForWindow(newHeight));
    this._fire("SizeChanged");
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
    this._fire("ContentSizeChanged");
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

    this._fire("ViewableSizeChanged");
  },

  observe: function cao_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "metro_softkeyboard_shown":
      case "metro_softkeyboard_hidden": {
        let event = document.createEvent("UIEvents");
        let eventDetails = {
          opened: aTopic == "metro_softkeyboard_shown",
          details: aData
        };

        event.initUIEvent("KeyboardChanged", true, false, window, eventDetails);
        window.dispatchEvent(event);

        this.updateViewableArea();
        break;
      }
    };
  },

  handleEvent: function cao_handleEvent(anEvent) {
    switch (anEvent.type) {
      case 'resize':
        if (anEvent.target != window)
          return;

        ContentAreaObserver.update();
        break;
    }
  },

  _fire: function (aName) {
    // setTimeout(callback, 0) to ensure the resize event handler dispatch is finished
    setTimeout(function() {
      let event = document.createEvent("Events");
      event.initEvent(aName, true, false);
      Elements.browsers.dispatchEvent(event);
    }, 0);
  },

  _getContentHeightForWindow: function (windowHeight) {
    let contextUIHeight = BrowserUI.isTabsOnly ? Elements.toolbar.getBoundingClientRect().bottom : 0;
    return windowHeight - contextUIHeight;
  },

  _getViewableHeightForContent: function (contentHeight) {
    let keyboardHeight = MetroUtils.keyboardHeight;
    return contentHeight - keyboardHeight;
  }
};
