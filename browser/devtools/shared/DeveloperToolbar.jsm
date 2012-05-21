/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [ "DeveloperToolbar" ];

const NS_XHTML = "http://www.w3.org/1999/xhtml";
const URI_GCLIBLANK = "chrome://browser/content/devtools/gcliblank.xhtml";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "gcli", function() {
  let obj = {};
  Components.utils.import("resource:///modules/devtools/gcli.jsm", obj);
  Components.utils.import("resource:///modules/devtools/GcliCommands.jsm", {});
  return obj.gcli;
});


/**
 * A component to manage the global developer toolbar, which contains a GCLI
 * and buttons for various developer tools.
 * @param aChromeWindow The browser window to which this toolbar is attached
 * @param aToolbarElement See browser.xul:<toolbar id="developer-toolbar">
 */
function DeveloperToolbar(aChromeWindow, aToolbarElement)
{
  this._chromeWindow = aChromeWindow;

  this._element = aToolbarElement;
  this._element.hidden = true;
  this._doc = this._element.ownerDocument;

  this._lastState = NOTIFICATIONS.HIDE;
  this._pendingShowCallback = undefined;
  this._pendingHide = false;
}

/**
 * Inspector notifications dispatched through the nsIObserverService
 */
const NOTIFICATIONS = {
  /** DeveloperToolbar.show() has been called, and we're working on it */
  LOAD: "developer-toolbar-load",

  /** DeveloperToolbar.show() has completed */
  SHOW: "developer-toolbar-show",

  /** DeveloperToolbar.hide() has been called */
  HIDE: "developer-toolbar-hide"
};

/**
 * Attach notification constants to the object prototype so tests etc can
 * use them without needing to import anything
 */
DeveloperToolbar.prototype.NOTIFICATIONS = NOTIFICATIONS;

/**
 * Is the toolbar open?
 */
Object.defineProperty(DeveloperToolbar.prototype, 'visible', {
  get: function DT_visible() {
    return !this._element.hidden;
  },
  enumerable: true
});

/**
 * Called from browser.xul in response to menu-click or keyboard shortcut to
 * toggle the toolbar
 */
DeveloperToolbar.prototype.toggle = function DT_toggle()
{
  if (this.visible) {
    this.hide();
  } else {
    this.show();
    this._input.focus();
  }
};

/**
 * Even if the user has not clicked on 'Got it' in the intro, we only show it
 * once per session.
 * Warning this is slightly messed up because this.DeveloperToolbar is not the
 * same as this.DeveloperToolbar when in browser.js context.
 */
DeveloperToolbar.introShownThisSession = false;

/**
 * Show the developer toolbar
 * @param aCallback show events can be asynchronous. If supplied aCallback will
 * be called when the DeveloperToolbar is visible
 */
DeveloperToolbar.prototype.show = function DT_show(aCallback)
{
  if (this._lastState != NOTIFICATIONS.HIDE) {
    return;
  }

  this._notify(NOTIFICATIONS.LOAD);
  this._pendingShowCallback = aCallback;
  this._pendingHide = false;

  let checkLoad = function() {
    if (this.tooltipPanel && this.tooltipPanel.loaded &&
        this.outputPanel && this.outputPanel.loaded) {
      this._onload();
    }
  }.bind(this);

  this._input = this._doc.querySelector(".gclitoolbar-input-node");
  this.tooltipPanel = new TooltipPanel(this._doc, this._input, checkLoad);
  this.outputPanel = new OutputPanel(this._doc, this._input, checkLoad);
};

/**
 * Initializing GCLI can only be done when we've got content windows to write
 * to, so this needs to be done asynchronously.
 */
DeveloperToolbar.prototype._onload = function DT_onload()
{
  this._doc.getElementById("Tools:DevToolbar").setAttribute("checked", "true");

  let contentDocument = this._chromeWindow.getBrowser().contentDocument;

  this.display = gcli.createDisplay({
    contentDocument: contentDocument,
    chromeDocument: this._doc,
    chromeWindow: this._chromeWindow,

    hintElement: this.tooltipPanel.hintElement,
    inputElement: this._input,
    completeElement: this._doc.querySelector(".gclitoolbar-complete-node"),
    backgroundElement: this._doc.querySelector(".gclitoolbar-stack-node"),
    outputDocument: this.outputPanel.document,

    environment: {
      chromeDocument: this._doc,
      contentDocument: contentDocument
    },

    tooltipClass: 'gcliterm-tooltip',
    eval: null,
    scratchpad: null
  });

  this.display.onVisibilityChange.add(this.outputPanel._visibilityChanged, this.outputPanel);
  this.display.onVisibilityChange.add(this.tooltipPanel._visibilityChanged, this.tooltipPanel);
  this.display.onOutput.add(this.outputPanel._outputChanged, this.outputPanel);

  this._chromeWindow.getBrowser().tabContainer.addEventListener("TabSelect", this, false);
  this._chromeWindow.getBrowser().addEventListener("load", this, true); 

  this._element.hidden = false;

  this._notify(NOTIFICATIONS.SHOW);
  if (this._pendingShowCallback) {
    this._pendingShowCallback.call();
    this._pendingShowCallback = undefined;
  }

  // If a hide event happened while we were loading, then we need to hide.
  // We could make this check earlier, but then cleanup would be complex so
  // we're being inefficient for now.
  if (this._pendingHide) {
    this.hide();
    return;
  }

  if (!DeveloperToolbar.introShownThisSession) {
    this.display.maybeShowIntro();
    DeveloperToolbar.introShownThisSession = true;
  }
};

/**
 * Hide the developer toolbar.
 */
DeveloperToolbar.prototype.hide = function DT_hide()
{
  if (this._lastState == NOTIFICATIONS.HIDE) {
    return;
  }

  if (this._lastState == NOTIFICATIONS.LOAD) {
    this._pendingHide = true;
    return;
  }

  this._element.hidden = true;

  this._doc.getElementById("Tools:DevToolbar").setAttribute("checked", "false");
  this.destroy();

  this._notify(NOTIFICATIONS.HIDE);
};

/**
 * Hide the developer toolbar
 */
DeveloperToolbar.prototype.destroy = function DT_destroy()
{
  this._chromeWindow.getBrowser().tabContainer.removeEventListener("TabSelect", this, false);
  this._chromeWindow.getBrowser().removeEventListener("load", this, true); 

  this.display.onVisibilityChange.remove(this.outputPanel._visibilityChanged, this.outputPanel);
  this.display.onVisibilityChange.remove(this.tooltipPanel._visibilityChanged, this.tooltipPanel);
  this.display.onOutput.remove(this.outputPanel._outputChanged, this.outputPanel);
  this.display.destroy();
  this.outputPanel.destroy();
  this.tooltipPanel.destroy();
  delete this._input;

  // We could "delete this.display" etc if we have hard-to-track-down memory
  // leaks as a belt-and-braces approach, however this prevents our DOM node
  // hunter from looking in all the nooks and crannies, so it's better if we
  // can be leak-free without
  /*
  delete this.display;
  delete this.outputPanel;
  delete this.tooltipPanel;
  */
};

/**
 * Utility for sending notifications
 * @param aTopic a NOTIFICATION constant
 */
DeveloperToolbar.prototype._notify = function DT_notify(aTopic)
{
  this._lastState = aTopic;

  let data = { toolbar: this };
  data.wrappedJSObject = data;
  Services.obs.notifyObservers(data, aTopic, null);
};

/**
 * Update various parts of the UI when the current tab changes
 * @param aEvent
 */
DeveloperToolbar.prototype.handleEvent = function DT_handleEvent(aEvent)
{
  if (aEvent.type == "TabSelect" || aEvent.type == "load") {
    this._chromeWindow.HUDConsoleUI.refreshCommand();
    this._chromeWindow.DebuggerUI.refreshCommand();

    if (this.visible) {
      let contentDocument = this._chromeWindow.getBrowser().contentDocument;

      this.display.reattach({
        contentDocument: contentDocument,
        chromeWindow: this._chromeWindow,
        environment: {
          chromeDocument: this._doc,
          contentDocument: contentDocument
        },
      });
    }
  }
};

/**
 * Add class="gcli-panel-inner-arrowcontent" to a panel's
 * |<xul:box class="panel-inner-arrowcontent">| so we can alter the styling
 * without complex CSS expressions.
 * @param aPanel The panel to affect
 */
function getContentBox(aPanel)
{
  let container = aPanel.ownerDocument.getAnonymousElementByAttribute(
          aPanel, "anonid", "container");
  return container.querySelector(".panel-inner-arrowcontent");
}

/**
 * Helper function to calculate the sum of the vertical padding and margins
 * between a nested node |aNode| and an ancestor |aRoot|. Iff all of the
 * children of aRoot are 'only-childs' until you get to aNode then to avoid
 * scroll-bars, the 'correct' height of aRoot is verticalSpacing + aNode.height.
 * @param aNode The child node whose height is known.
 * @param aRoot The parent height whose height we can affect.
 * @return The sum of the vertical padding/margins in between aNode and aRoot.
 */
function getVerticalSpacing(aNode, aRoot)
{
  let win = aNode.ownerDocument.defaultView;

  function pxToNum(styles, property) {
    return parseInt(styles.getPropertyValue(property).replace(/px$/, ''), 10);
  }

  let vertSpacing = 0;
  do {
    let styles = win.getComputedStyle(aNode);
    vertSpacing += pxToNum(styles, "padding-top");
    vertSpacing += pxToNum(styles, "padding-bottom");
    vertSpacing += pxToNum(styles, "margin-top");
    vertSpacing += pxToNum(styles, "margin-bottom");
    vertSpacing += pxToNum(styles, "border-top-width");
    vertSpacing += pxToNum(styles, "border-bottom-width");

    let prev = aNode.previousSibling;
    while (prev != null) {
      vertSpacing += prev.clientHeight;
      prev = prev.previousSibling;
    }

    let next = aNode.nextSibling;
    while (next != null) {
      vertSpacing += next.clientHeight;
      next = next.nextSibling;
    }

    aNode = aNode.parentNode;
  } while (aNode !== aRoot);

  return vertSpacing + 9;
}

/**
 * Panel to handle command line output.
 * @param aChromeDoc document from which we can pull the parts we need.
 * @param aInput the input element that should get focus.
 * @param aLoadCallback called when the panel is loaded properly.
 */
function OutputPanel(aChromeDoc, aInput, aLoadCallback)
{
  this._input = aInput;
  this._anchor = aChromeDoc.getElementById("developer-toolbar");

  this._loadCallback = aLoadCallback;

  /*
  <panel id="gcli-output"
         type="arrow"
         noautofocus="true"
         noautohide="true"
         class="gcli-panel">
    <iframe id="gcli-output-frame"
            src=URI_GCLIBLANK
            flex="1"/>
  </panel>
  */
  this._panel = aChromeDoc.createElement("panel");
  this._panel.id = "gcli-output";
  this._panel.classList.add("gcli-panel");
  this._panel.setAttribute("type", "arrow");
  this._panel.setAttribute("noautofocus", "true");
  this._panel.setAttribute("noautohide", "true");
  this._anchor.parentElement.insertBefore(this._panel, this._anchor);

  this._frame = aChromeDoc.createElement("iframe");
  this._frame.id = "gcli-output-frame";
  this._frame.setAttribute("src", URI_GCLIBLANK);
  this._frame.setAttribute("flex", "1");
  this._panel.appendChild(this._frame);

  this.displayedOutput = undefined;

  this._onload = this._onload.bind(this);
  this._frame.addEventListener("load", this._onload, true);

  this.loaded = false;
}

/**
 * Wire up the element from the iframe, and inform the _loadCallback.
 */
OutputPanel.prototype._onload = function OP_onload()
{
  this._frame.removeEventListener("load", this._onload, true);
  delete this._onload;

  this._content = getContentBox(this._panel);
  this._content.classList.add("gcli-panel-inner-arrowcontent");

  this.document = this._frame.contentDocument;
  this.document.body.classList.add("gclichrome-output");

  this._div = this.document.querySelector("div");
  this._div.classList.add('gcli-row-out');
  this._div.setAttribute('aria-live', 'assertive');

  this.loaded = true;
  if (this._loadCallback) {
    this._loadCallback();
    delete this._loadCallback;
  }
};

/**
 * Display the OutputPanel.
 */
OutputPanel.prototype.show = function OP_show()
{
  this._panel.ownerDocument.defaultView.setTimeout(function() {
    this._resize();
  }.bind(this), 0);

  this._resize();
  this._panel.openPopup(this._anchor, "before_end", -300, 0, false, false, null);

  this._input.focus();
};

/**
 * Internal helper to set the height of the output panel to fit the available
 * content;
 */
OutputPanel.prototype._resize = function CLP_resize()
{
  let vertSpacing = getVerticalSpacing(this._content, this._panel);
  let idealHeight = this.document.body.scrollHeight + vertSpacing;
  this._panel.sizeTo(400, Math.min(idealHeight, 500));
};

/**
 * Called by GCLI when a command is executed.
 */
OutputPanel.prototype._outputChanged = function OP_outputChanged(aEvent)
{
  if (aEvent.output.hidden) {
    return;
  }

  this.remove();

  this.displayedOutput = aEvent.output;
  this.update();

  this.displayedOutput.onChange.add(this.update, this);
  this.displayedOutput.onClose.add(this.remove, this);
};

/**
 * Called when displayed Output says it's changed or from outputChanged, which
 * happens when there is a new displayed Output.
 */
OutputPanel.prototype.update = function OP_update()
{
  if (this.displayedOutput.data == null) {
    while (this._div.hasChildNodes()) {
      this._div.removeChild(this._div.firstChild);
    }
  } else {
    this.displayedOutput.toDom(this._div);
    this.show();
  }
};

/**
 * Detach listeners from the currently displayed Output.
 */
OutputPanel.prototype.remove = function OP_remove()
{
  this._panel.hidePopup();

  if (this.displayedOutput) {
    this.displayedOutput.onChange.remove(this.update, this);
    this.displayedOutput.onClose.remove(this.remove, this);
    delete this.displayedOutput;
  }
};

/**
 * Detach listeners from the currently displayed Output.
 */
OutputPanel.prototype.destroy = function OP_destroy()
{
  this.remove();

  this._panel.removeChild(this._frame);
  this._anchor.parentElement.removeChild(this._panel);

  delete this._input;
  delete this._anchor;
  delete this._panel;
  delete this._frame;
  delete this._content;
  delete this._div;
  delete this.document;
};

/**
 * Called by GCLI to indicate that we should show or hide one either the
 * tooltip panel or the output panel.
 */
OutputPanel.prototype._visibilityChanged = function OP_visibilityChanged(aEvent)
{
  if (aEvent.outputVisible === true) {
    // this.show is called by _outputChanged
  } else {
    this._panel.hidePopup();
  }
};


/**
 * Panel to handle tooltips.
 * @param aChromeDoc document from which we can pull the parts we need.
 * @param aInput the input element that should get focus.
 * @param aLoadCallback called when the panel is loaded properly.
 */
function TooltipPanel(aChromeDoc, aInput, aLoadCallback)
{
  this._input = aInput;
  this._anchor = aChromeDoc.getElementById("developer-toolbar");

  this._onload = this._onload.bind(this);
  this._loadCallback = aLoadCallback;
  /*
  <panel id="gcli-tooltip"
         type="arrow"
         noautofocus="true"
         noautohide="true"
         class="gcli-panel">
    <iframe id="gcli-tooltip-frame"
            src=URI_GCLIBLANK
            flex="1"/>
  </panel>
  */
  this._panel = aChromeDoc.createElement("panel");
  this._panel.id = "gcli-tooltip";
  this._panel.classList.add("gcli-panel");
  this._panel.setAttribute("type", "arrow");
  this._panel.setAttribute("noautofocus", "true");
  this._panel.setAttribute("noautohide", "true");
  this._anchor.parentElement.insertBefore(this._panel, this._anchor);

  this._frame = aChromeDoc.createElement("iframe");
  this._frame.id = "gcli-tooltip-frame";
  this._frame.setAttribute("src", URI_GCLIBLANK);
  this._frame.setAttribute("flex", "1");
  this._panel.appendChild(this._frame);

  this._frame.addEventListener("load", this._onload, true);
  this.loaded = false;
}

/**
 * Wire up the element from the iframe, and inform the _loadCallback.
 */
TooltipPanel.prototype._onload = function TP_onload()
{
  this._frame.removeEventListener("load", this._onload, true);

  this._content = getContentBox(this._panel);
  this._content.classList.add("gcli-panel-inner-arrowcontent");

  this.document = this._frame.contentDocument;
  this.document.body.classList.add("gclichrome-tooltip");

  this.hintElement = this.document.querySelector("div");

  this.loaded = true;

  if (this._loadCallback) {
    this._loadCallback();
    delete this._loadCallback;
  }
};

/**
 * Display the TooltipPanel.
 */
TooltipPanel.prototype.show = function TP_show()
{
  let vertSpacing = getVerticalSpacing(this._content, this._panel);
  let idealHeight = this.document.body.scrollHeight + vertSpacing;
  this._panel.sizeTo(350, Math.min(idealHeight, 500));
  this._panel.openPopup(this._anchor, "before_start", 0, 0, false, false, null);

  this._input.focus();
};

/**
 * Hide the TooltipPanel.
 */
TooltipPanel.prototype.remove = function TP_remove()
{
  this._panel.hidePopup();
};

/**
 * Hide the TooltipPanel.
 */
TooltipPanel.prototype.destroy = function TP_destroy()
{
  this.remove();

  this._panel.removeChild(this._frame);
  this._anchor.parentElement.removeChild(this._panel);

  delete this._input;
  delete this._onload;
  delete this._panel;
  delete this._frame;
  delete this._anchor;
  delete this._content;
  delete this.document;
  delete this.hintElement;
};

/**
 * Called by GCLI to indicate that we should show or hide one either the
 * tooltip panel or the output panel.
 */
TooltipPanel.prototype._visibilityChanged = function TP_visibilityChanged(aEvent)
{
  if (aEvent.tooltipVisible === true) {
    this.show();
  } else {
    this._panel.hidePopup();
  }
};
