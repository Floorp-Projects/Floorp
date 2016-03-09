/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// A module for working with panels with iframes shared across windows.

this.EXPORTED_SYMBOLS = ["PanelFrame"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CustomizableUI", "resource:///modules/CustomizableUI.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DynamicResizeWatcher", "resource:///modules/Social.jsm");

// The minimum sizes for the auto-resize panel code.
const PANEL_MIN_HEIGHT = 100;
const PANEL_MIN_WIDTH = 330;

var PanelFrameInternal = {
  /**
   * Helper function to get and hold a single instance of a DynamicResizeWatcher.
   */
  get _dynamicResizer() {
    delete this._dynamicResizer;
    this._dynamicResizer = new DynamicResizeWatcher();
    return this._dynamicResizer;
  },

  /**
   * Status panels are one-per button per-process, we swap the docshells between
   * windows when necessary.
   *
   * @param {DOMWindow} aWindow The window in which to show the popup.
   * @param {PanelUI} aPanelUI The panel UI object that represents the application menu.
   * @param {DOMElement} aButton The button element that is pressed to show the popup.
   * @param {String} aType The type of panel this is, e.g. "social" or "loop".
   * @param {String} aOrigin Optional, the origin to use for the iframe.
   * @param {String} aSrc The url to load into the iframe.
   * @param {String} aSize The initial size of the panel (width and height are the same
   *                       if specified).
   */
  _attachNotificatonPanel: function(aWindow, aParent, aButton, aType, aOrigin, aSrc, aSize) {
    aParent.hidden = false;
    let notificationFrameId = aOrigin ? aType + "-status-" + aOrigin : aType + "-panel-iframe";
    let doc = aWindow.document;
    let frame = doc.getElementById(notificationFrameId);

    // If the button was customized to a new location, destroy the
    // iframe and start fresh.
    if (frame && frame.parentNode != aParent) {
      frame.parentNode.removeChild(frame);
      frame = null;
    }

    if (!frame) {
      let {width, height} = aSize ? aSize : {width: PANEL_MIN_WIDTH, height: PANEL_MIN_HEIGHT};
      frame = doc.createElement("browser");
      let attrs = {
        "type": "content",
        "mozbrowser": "true",
        // All frames use social-panel-frame as the class.
        "class": "social-panel-frame",
        "id": notificationFrameId,
        "tooltip": "aHTMLTooltip",
        "context": "contentAreaContextMenu",
        "flex": "1",

        // work around bug 793057 - by making the panel roughly the final size
        // we are more likely to have the anchor in the correct position.
        "style": "width: " + width + "px; height: " + height + "px;",
        "dynamicresizer": !aSize,

        "origin": aOrigin,
        "src": aSrc
      };
      if (aType == "social") {
        attrs["message"] = "true";
        attrs["messagemanagergroup"] = aType;
      }
      if (aType == "loop") {
        attrs.message = true;
        attrs.messagemanagergroup = "social";
        attrs.autocompletepopup = "PopupAutoComplete";
      }
      for (let [k, v] of Iterator(attrs)) {
        frame.setAttribute(k, v);
      }
      aParent.appendChild(frame);
    } else {
      frame.setAttribute("origin", aOrigin);
      frame.setAttribute("src", aSrc);
    }
    aButton.setAttribute("notificationFrameId", notificationFrameId);
  }
};

/**
 * The exported PanelFrame object
 */
var PanelFrame = {
  /**
   * Shows a popup in a pop-up panel, or in a sliding panel view in the application menu.
   * It will move the iframe to different DOM locations depending on where it needs to be
   * shown, enabling one iframe to be used for the entire session.
   *
   * @param {DOMWindow} aWindow The window in which to show the popup.
   * @param {PanelUI} aPanelUI The panel UI object that represents the application menu.
   * @param {DOMElement} aToolbarButton The button element that is pressed to show the popup.
   * @param {String} aType The type of panel this is, e.g. "social" or "loop".
   * @param {String} aOrigin Optional, the origin to use for the iframe.
   * @param {String} aSrc The url to load into the iframe.
   * @param {String} aSize The initial size of the panel (width and height are the same
   *                       if specified).
   * @param {Function} aCallback Optional, callback to be called with the iframe when it is
   *                             set up.
   */
  showPopup: function(aWindow, aToolbarButton, aType, aOrigin, aSrc, aSize, aCallback) {
    // if we're overflowed, our anchor needs to be the overflow button
    let widgetGroup = CustomizableUI.getWidget(aToolbarButton.getAttribute("id"));
    let widget = widgetGroup.forWindow(aWindow);
    // if we're a slice in the hamburger, our anchor will be the menu button,
    // this panel will replace the menu panel when the button is clicked on
    let anchorBtn = widget.anchor;

    let panel = aWindow.document.getElementById(aType + "-notification-panel");
    PanelFrameInternal._attachNotificatonPanel(aWindow, panel, aToolbarButton, aType, aOrigin, aSrc, aSize);

    let notificationFrameId = aToolbarButton.getAttribute("notificationFrameId");
    let notificationFrame = aWindow.document.getElementById(notificationFrameId);

    // the xbl bindings for the iframe probably don't exist yet, so we can't
    // access iframe.messageManager directly - but can get at it with this dance.
    let mm = notificationFrame.QueryInterface(Ci.nsIFrameLoaderOwner).frameLoader.messageManager;

    // Clear dimensions on all browsers so the panel size will
    // only use the selected browser.
    let frameIter = panel.firstElementChild;
    while (frameIter) {
      frameIter.collapsed = (frameIter != notificationFrame);
      frameIter = frameIter.nextElementSibling;
    }

    function dispatchPanelEvent(name) {
      mm.sendAsyncMessage("Social:CustomEvent", { name: name });
    }

    // we only use a dynamic resizer when we're located the toolbar.
    let dynamicResizer;
    if (notificationFrame.getAttribute("dynamicresizer") == "true") {
      dynamicResizer = PanelFrameInternal._dynamicResizer;
    }
    panel.addEventListener("popuphidden", function onpopuphiding() {
      panel.removeEventListener("popuphidden", onpopuphiding);
      anchorBtn.removeAttribute("open");
      if (dynamicResizer)
        dynamicResizer.stop();
      notificationFrame.docShellIsActive = false;
      dispatchPanelEvent(aType + "FrameHide");
    });

    panel.addEventListener("popupshowing", function onpopupshowing() {
      panel.removeEventListener("popupshowning", onpopupshowing);
      // This attribute is needed on both the button and the
      // containing toolbaritem since the buttons on OS X have
      // moz-appearance:none, while their container gets
      // moz-appearance:toolbarbutton due to the way that toolbar buttons
      // get combined on OS X.
      anchorBtn.setAttribute("open", "true");
    });

    panel.addEventListener("popupshown", function onpopupshown() {
      panel.removeEventListener("popupshown", onpopupshown);

      mm.sendAsyncMessage("WaitForDOMContentLoaded");
      mm.addMessageListener("DOMContentLoaded", function onloaded() {
        mm.removeMessageListener("DOMContentLoaded", onloaded);
        mm = notificationFrame.messageManager;
        notificationFrame.docShellIsActive = true;
        if (dynamicResizer)
          dynamicResizer.start(panel, notificationFrame);
        dispatchPanelEvent(aType + "FrameShow");
      });
    });

    let anchor = aWindow.document.getAnonymousElementByAttribute(anchorBtn, "class", "toolbarbutton-icon");
    // Bug 849216 - open the popup asynchronously so we avoid the auto-rollup
    // handling from preventing it being opened in some cases.
    Services.tm.mainThread.dispatch(function() {
      panel.openPopup(anchor, "bottomcenter topright", 0, 0, false, false);
    }, Ci.nsIThread.DISPATCH_NORMAL);

    if (aCallback)
      aCallback(notificationFrame);
  }
};
