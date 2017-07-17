/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

module.metadata = {
  "stability": "unstable"
};

const { Cc, Ci } = require("chrome");
const { Services } = require("resource://gre/modules/Services.jsm");
lazyRequire(this, "../timers", "setTimeout");
lazyRequire(this, "../system", "platform");
lazyRequire(this, "../window/utils", "getMostRecentBrowserWindow", "getOwnerBrowserWindow",
            "getScreenPixelsPerCSSPixel");

lazyRequire(this, "../frame/utils", { "create": "createFrame" }, "swapFrameLoaders", "getDocShell");
lazyRequire(this, "../addon/window", { "window": "addonWindow" });
lazyRequire(this, "../lang/type", "isNil");
lazyRequire(this, '../self', "data");

lazyRequireModule(this, "../system/events", "events");


const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

function calculateRegion({ position, width, height, defaultWidth, defaultHeight }, rect) {
  position = position || {};

  let x, y;

  let hasTop = !isNil(position.top);
  let hasRight = !isNil(position.right);
  let hasBottom = !isNil(position.bottom);
  let hasLeft = !isNil(position.left);
  let hasWidth = !isNil(width);
  let hasHeight = !isNil(height);

  // if width is not specified by constructor or show's options, then get
  // the default width
  if (!hasWidth)
    width = defaultWidth;

  // if height is not specified by constructor or show's options, then get
  // the default height
  if (!hasHeight)
    height = defaultHeight;

  // default position is centered
  x = (rect.right - width) / 2;
  y = (rect.top + rect.bottom - height) / 2;

  if (hasTop) {
    y = rect.top + position.top;

    if (hasBottom && !hasHeight)
      height = rect.bottom - position.bottom - y;
  }
  else if (hasBottom) {
    y = rect.bottom - position.bottom - height;
  }

  if (hasLeft) {
    x = position.left;

    if (hasRight && !hasWidth)
      width = rect.right - position.right - x;
  }
  else if (hasRight) {
    x = rect.right - width - position.right;
  }

  return {x: x, y: y, width: width, height: height};
}

function open(panel, options, anchor) {
  // Wait for the XBL binding to be constructed
  if (!panel.openPopup) setTimeout(open, 50, panel, options, anchor);
  else display(panel, options, anchor);
}
exports.open = open;

function isOpen(panel) {
  return panel.state === "open"
}
exports.isOpen = isOpen;

function isOpening(panel) {
  return panel.state === "showing"
}
exports.isOpening = isOpening

function close(panel) {
  // Sometimes "TypeError: panel.hidePopup is not a function" is thrown
  // when quitting the host application while a panel is visible.  To suppress
  // these errors, check for "hidePopup" in panel before calling it.
  // It's not clear if there's an issue or it's expected behavior.
  // See Bug 1151796.

  return panel.hidePopup && panel.hidePopup();
}
exports.close = close


function resize(panel, width, height) {
  // Resize the iframe instead of using panel.sizeTo
  // because sizeTo doesn't work with arrow panels
  if (panel.firstChild) {
    panel.firstChild.style.width = width + "px";
    panel.firstChild.style.height = height + "px";
  }
}
exports.resize = resize

function display(panel, options, anchor) {
  let document = panel.ownerDocument;

  let x, y;
  let { width, height, defaultWidth, defaultHeight } = options;

  let popupPosition = null;

  // Panel XBL has some SDK incompatible styling decisions. We shim panel
  // instances until proper fix for Bug 859504 is shipped.
  shimDefaultStyle(panel);

  if (!anchor) {
    // The XUL Panel doesn't have an arrow, so the margin needs to be reset
    // in order to, be positioned properly
    panel.style.margin = "0";

    let viewportRect = document.defaultView.gBrowser.getBoundingClientRect();

    ({x, y, width, height} = calculateRegion(options, viewportRect));
  }
  else {
    // The XUL Panel has an arrow, so the margin needs to be reset
    // to the default value.
    panel.style.margin = "";
    let { CustomizableUI, window } = anchor.ownerGlobal;

    // In Australis, widgets may be positioned in an overflow panel or the
    // menu panel.
    // In such cases clicking this widget will hide the overflow/menu panel,
    // and the widget's panel will show instead.
    // If `CustomizableUI` is not available, it means the anchor is not in a
    // chrome browser window, and therefore there is no need for this check.
    if (CustomizableUI) {
      let node = anchor;
      ({anchor} = CustomizableUI.getWidget(anchor.id).forWindow(window));

      // if `node` is not the `anchor` itself, it means the widget is
      // positioned in a panel, therefore we have to hide it before show
      // the widget's panel in the same anchor
      if (node !== anchor)
        CustomizableUI.hidePanelForNode(anchor);
    }

    width = width || defaultWidth;
    height = height || defaultHeight;

    // Open the popup by the anchor.
    let rect = anchor.getBoundingClientRect();

    let zoom = getScreenPixelsPerCSSPixel(window);
    let screenX = rect.left + window.mozInnerScreenX * zoom;
    let screenY = rect.top + window.mozInnerScreenY * zoom;

    // Set up the vertical position of the popup relative to the anchor
    // (always display the arrow on anchor center)
    let horizontal, vertical;
    if (screenY > window.screen.availHeight / 2 + height)
      vertical = "top";
    else
      vertical = "bottom";

    if (screenY > window.screen.availWidth / 2 + width)
      horizontal = "left";
    else
      horizontal = "right";

    let verticalInverse = vertical == "top" ? "bottom" : "top";
    popupPosition = vertical + "center " + verticalInverse + horizontal;

    // Allow panel to flip itself if the panel can't be displayed at the
    // specified position (useful if we compute a bad position or if the
    // user moves the window and panel remains visible)
    panel.setAttribute("flip", "both");
  }

  if (!panel.viewFrame) {
    panel.viewFrame = document.importNode(panel.backgroundFrame, false);
    panel.appendChild(panel.viewFrame);

    let {privateBrowsingId} = getDocShell(panel.viewFrame).getOriginAttributes();
    let principal = Services.scriptSecurityManager.createNullPrincipal({privateBrowsingId});
    getDocShell(panel.viewFrame).createAboutBlankContentViewer(principal);
  }

  // Resize the iframe instead of using panel.sizeTo
  // because sizeTo doesn't work with arrow panels
  panel.firstChild.style.width = width + "px";
  panel.firstChild.style.height = height + "px";

  panel.openPopup(anchor, popupPosition, x, y);
}
exports.display = display;

// This utility function is just a workaround until Bug 859504 has shipped.
function shimDefaultStyle(panel) {
  let document = panel.ownerDocument;
  // Please note that `panel` needs to be part of document in order to reach
  // it's anonymous nodes. One of the anonymous node has a big padding which
  // doesn't work well since panel frame needs to fill all of the panel.
  // XBL binding is a not the best option as it's applied asynchronously, and
  // makes injected frames behave in strange way. Also this feels a lot
  // cheaper to do.
  ["panel-inner-arrowcontent", "panel-arrowcontent"].forEach(function(value) {
    let node = document.getAnonymousElementByAttribute(panel, "class", value);
      if (node) node.style.padding = 0;
  });
}

function show(panel, options, anchor) {
  // Prevent the panel from getting focus when showing up
  // if focus is set to false
  panel.setAttribute("noautofocus", !options.focus);

  let window = anchor && getOwnerBrowserWindow(anchor);
  let { document } = window ? window : getMostRecentBrowserWindow();
  attach(panel, document);

  open(panel, options, anchor);
}
exports.show = show

function onPanelClick(event) {
  let { target, metaKey, ctrlKey, shiftKey, button } = event;
  let accel = platform === "darwin" ? metaKey : ctrlKey;
  let isLeftClick = button === 0;
  let isMiddleClick = button === 1;

  if ((isLeftClick && (accel || shiftKey)) || isMiddleClick) {
    let link = target.closest('a');

    if (link && link.href)
       getMostRecentBrowserWindow().openUILink(link.href, event)
  }
}

function setupPanelFrame(frame) {
  frame.setAttribute("flex", 1);
  frame.setAttribute("transparent", "transparent");
  frame.setAttribute("autocompleteenabled", true);
  frame.setAttribute("tooltip", "aHTMLTooltip");
}

function make(document, options) {
  document = document || getMostRecentBrowserWindow().document;
  let panel = document.createElementNS(XUL_NS, "panel");
  panel.setAttribute("type", "arrow");
  panel.setAttribute("sdkscriptenabled", options.allowJavascript);

  // The panel needs to be attached to a browser window in order for us
  // to copy browser styles to the content document when it loads.
  attach(panel, document);

  let frameOptions =  {
    allowJavascript: options.allowJavascript,
    allowPlugins: true,
    allowAuth: true,
    allowWindowControl: false,
    // Need to override `nodeName` to use `iframe` as `browsers` save session
    // history and in consequence do not dispatch "inner-window-destroyed"
    // notifications.
    browser: false,
  };

  let backgroundFrame = createFrame(addonWindow, frameOptions);
  setupPanelFrame(backgroundFrame);

  getDocShell(backgroundFrame).inheritPrivateBrowsingId = false;

  function onPopupShowing({type, target}) {
    if (target === this) {
      let attrs = getDocShell(backgroundFrame).getOriginAttributes();
      getDocShell(panel.viewFrame).setOriginAttributes(attrs);

      swapFrameLoaders(backgroundFrame, panel.viewFrame);
    }
  }

  function onPopupHiding({type, target}) {
    if (target === this) {
      swapFrameLoaders(backgroundFrame, panel.viewFrame);

      panel.viewFrame.remove();
      panel.viewFrame = null;
    }
  }

  function onContentReady({target, type}) {
    if (target === getContentDocument(panel)) {
      style(panel);
      events.emit(type, { subject: panel });
    }
  }

  function onContentLoad({target, type}) {
    if (target === getContentDocument(panel))
      events.emit(type, { subject: panel });
  }

  function onContentChange({subject: document, type}) {
    if (document === getContentDocument(panel) && document.defaultView)
      events.emit(type, { subject: panel });
  }

  function onPanelStateChange({target, type}) {
    if (target === this)
      events.emit(type, { subject: panel })
  }

  panel.addEventListener("popupshowing", onPopupShowing);
  panel.addEventListener("popuphiding", onPopupHiding);
  for (let event of ["popupshowing", "popuphiding", "popupshown", "popuphidden"])
    panel.addEventListener(event, onPanelStateChange);

  panel.addEventListener("click", onPanelClick);

  // Panel content document can be either in panel `viewFrame` or in
  // a `backgroundFrame` depending on panel state. Listeners are set
  // on both to avoid setting and removing listeners on panel state changes.

  panel.addEventListener("DOMContentLoaded", onContentReady, true);
  backgroundFrame.addEventListener("DOMContentLoaded", onContentReady, true);

  panel.addEventListener("load", onContentLoad, true);
  backgroundFrame.addEventListener("load", onContentLoad, true);

  events.on("document-element-inserted", onContentChange);

  panel.backgroundFrame = backgroundFrame;
  panel.viewFrame = null;

  // Store event listener on the panel instance so that it won't be GC-ed
  // while panel is alive.
  panel.onContentChange = onContentChange;

  return panel;
}
exports.make = make;

function attach(panel, document) {
  document = document || getMostRecentBrowserWindow().document;
  let container = document.getElementById("mainPopupSet");
  if (container !== panel.parentNode) {
    detach(panel);
    document.getElementById("mainPopupSet").appendChild(panel);
  }
}
exports.attach = attach;

function detach(panel) {
  if (panel.parentNode) panel.remove();
}
exports.detach = detach;

function dispose(panel) {
  panel.backgroundFrame.remove();
  panel.backgroundFrame = null;
  events.off("document-element-inserted", panel.onContentChange);
  panel.onContentChange = null;
  detach(panel);
}
exports.dispose = dispose;

function style(panel) {
  /**
  Injects default OS specific panel styles into content document that is loaded
  into given panel. Optionally `document` of the browser window can be
  given to inherit styles from it, by default it will use either panel owner
  document or an active browser's document. It should not matter though unless
  Firefox decides to style windows differently base on profile or mode like
  chrome for example.
  **/

  try {
    let document = panel.ownerDocument;
    let contentDocument = getContentDocument(panel);
    let window = document.defaultView;
    let node = document.getAnonymousElementByAttribute(panel, "class",
                                                       "panel-arrowcontent");

    let { color, fontFamily, fontSize, fontWeight } = window.getComputedStyle(node);

    let style = contentDocument.createElement("style");
    style.id = "sdk-panel-style";
    style.textContent = "body { " +
      "color: " + color + ";" +
      "font-family: " + fontFamily + ";" +
      "font-weight: " + fontWeight + ";" +
      "font-size: " + fontSize + ";" +
    "}";

    let container = contentDocument.head ? contentDocument.head :
                    contentDocument.documentElement;

    if (container.firstChild)
      container.insertBefore(style, container.firstChild);
    else
      container.appendChild(style);
  }
  catch (error) {
    console.error("Unable to apply panel style");
    console.exception(error);
  }
}
exports.style = style;

var getContentFrame = panel => panel.viewFrame || panel.backgroundFrame;
exports.getContentFrame = getContentFrame;

function getContentDocument(panel) {
  return getContentFrame(panel).contentDocument;
}
exports.getContentDocument = getContentDocument;

function setURL(panel, url) {
  let frame = getContentFrame(panel);
  let webNav = getDocShell(frame).QueryInterface(Ci.nsIWebNavigation);

  webNav.loadURI(url ? data.url(url) : "about:blank", 0, null, null, null);
}

exports.setURL = setURL;

function allowContextMenu(panel, allow) {
  if (allow) {
    panel.setAttribute("context", "contentAreaContextMenu");
  }
  else {
    panel.removeAttribute("context");
  }
}
exports.allowContextMenu = allowContextMenu;
