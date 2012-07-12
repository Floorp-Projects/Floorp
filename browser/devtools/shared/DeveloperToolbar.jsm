/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [ "DeveloperToolbar" ];

const NS_XHTML = "http://www.w3.org/1999/xhtml";

const WEBCONSOLE_CONTENT_SCRIPT_URL =
  "chrome://browser/content/devtools/HUDService-content.js";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource:///modules/devtools/Console.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gcli",
                                  "resource:///modules/devtools/gcli.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "GcliCommands",
                                  "resource:///modules/devtools/GcliCommands.jsm");

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
  this._errorsCount = {};
  this._webConsoleButton = this._doc
                           .getElementById("developer-toolbar-webconsole");

  try {
    GcliCommands.refreshAutoCommands(aChromeWindow);
  }
  catch (ex) {
    console.error(ex);
  }
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

DeveloperToolbar.prototype._contentMessageListeners =
  ["WebConsole:CachedMessages", "WebConsole:PageError"];

/**
 * Is the toolbar open?
 */
Object.defineProperty(DeveloperToolbar.prototype, 'visible', {
  get: function DT_visible() {
    return !this._element.hidden;
  },
  enumerable: true
});

var _gSequenceId = 0;

/**
 * Getter for a unique ID.
 */
Object.defineProperty(DeveloperToolbar.prototype, 'sequenceId', {
  get: function DT_visible() {
    return _gSequenceId++;
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
    this.show(true);
  }
};

/**
 * Called from browser.xul in response to menu-click or keyboard shortcut to
 * toggle the toolbar
 */
DeveloperToolbar.prototype.focus = function DT_focus()
{
  if (this.visible) {
    this._input.focus();
  } else {
    this.show(true);
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
DeveloperToolbar.prototype.show = function DT_show(aFocus, aCallback)
{
  if (this._lastState != NOTIFICATIONS.HIDE) {
    return;
  }

  Services.prefs.setBoolPref("devtools.toolbar.visible", true);

  this._notify(NOTIFICATIONS.LOAD);
  this._pendingShowCallback = aCallback;
  this._pendingHide = false;

  let checkLoad = function() {
    if (this.tooltipPanel && this.tooltipPanel.loaded &&
        this.outputPanel && this.outputPanel.loaded) {
      this._onload(aFocus);
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
DeveloperToolbar.prototype._onload = function DT_onload(aFocus)
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

  this.display.focusManager.addMonitoredElement(this.outputPanel._frame);
  this.display.focusManager.addMonitoredElement(this._element);

  this.display.onVisibilityChange.add(this.outputPanel._visibilityChanged, this.outputPanel);
  this.display.onVisibilityChange.add(this.tooltipPanel._visibilityChanged, this.tooltipPanel);
  this.display.onOutput.add(this.outputPanel._outputChanged, this.outputPanel);

  this._chromeWindow.getBrowser().tabContainer.addEventListener("TabSelect", this, false);
  this._chromeWindow.getBrowser().tabContainer.addEventListener("TabClose", this, false);
  this._chromeWindow.getBrowser().addEventListener("load", this, true);
  this._chromeWindow.getBrowser().addEventListener("beforeunload", this, true);

  this._initErrorsCount(this._chromeWindow.getBrowser().selectedTab);

  this._element.hidden = false;

  if (aFocus) {
    this._input.focus();
  }

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
 * Initialize the listeners needed for tracking the number of errors for a given
 * tab.
 *
 * @private
 * @param nsIDOMNode aTab the xul:tab for which you want to track the number of
 * errors.
 */
DeveloperToolbar.prototype._initErrorsCount = function DT__initErrorsCount(aTab)
{
  let tabId = aTab.linkedPanel;
  if (tabId in this._errorsCount) {
    this._updateErrorsCount();
    return;
  }

  let messageManager = aTab.linkedBrowser.messageManager;
  messageManager.loadFrameScript(WEBCONSOLE_CONTENT_SCRIPT_URL, true);

  this._errorsCount[tabId] = 0;

  this._contentMessageListeners.forEach(function(aName) {
    messageManager.addMessageListener(aName, this);
  }, this);

  let message = {
    features: ["PageError"],
    cachedMessages: ["PageError"],
  };

  this.sendMessageToTab(aTab, "WebConsole:Init", message);
  this._updateErrorsCount();
};

/**
 * Stop the listeners needed for tracking the number of errors for a given
 * tab.
 *
 * @private
 * @param nsIDOMNode aTab the xul:tab for which you want to stop tracking the
 * number of errors.
 */
DeveloperToolbar.prototype._stopErrorsCount = function DT__stopErrorsCount(aTab)
{
  let tabId = aTab.linkedPanel;
  if (!(tabId in this._errorsCount)) {
    this._updateErrorsCount();
    return;
  }

  this.sendMessageToTab(aTab, "WebConsole:Destroy", {});

  let messageManager = aTab.linkedBrowser.messageManager;
  this._contentMessageListeners.forEach(function(aName) {
    messageManager.removeMessageListener(aName, this);
  }, this);

  delete this._errorsCount[tabId];
  this._updateErrorsCount();
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

  Services.prefs.setBoolPref("devtools.toolbar.visible", false);

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
  this._chromeWindow.getBrowser().removeEventListener("beforeunload", this, true);

  let tabs = this._chromeWindow.getBrowser().tabs;
  Array.prototype.forEach.call(tabs, this._stopErrorsCount, this);

  this.display.focusManager.removeMonitoredElement(this.outputPanel._frame);
  this.display.focusManager.removeMonitoredElement(this._element);

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

      if (aEvent.type == "TabSelect") {
        this._initErrorsCount(aEvent.target);
      }
    }
  }
  else if (aEvent.type == "TabClose") {
    this._stopErrorsCount(aEvent.target);
  }
  else if (aEvent.type == "beforeunload") {
    this._onPageBeforeUnload(aEvent);
  }
};

/**
 * The handler of messages received from the nsIMessageManager.
 *
 * @param object aMessage the message received from the content process.
 */
DeveloperToolbar.prototype.receiveMessage = function DT_receiveMessage(aMessage)
{
  if (!aMessage.json || !(aMessage.json.hudId in this._errorsCount)) {
    return;
  }

  let tabId = aMessage.json.hudId;
  let errors = this._errorsCount[tabId];

  switch (aMessage.name) {
    case "WebConsole:PageError":
      this._onPageError(tabId, aMessage.json.pageError);
      break;
    case "WebConsole:CachedMessages":
      aMessage.json.messages.forEach(this._onPageError.bind(this, tabId));
      break;
  }

  if (errors != this._errorsCount[tabId]) {
    this._updateErrorsCount(tabId);
  }
};

/**
 * Send a message to the content process using the nsIMessageManager of the
 * given tab.
 *
 * @param nsIDOMNode aTab the tab you want to send a message to.
 * @param string aName the name of the message you want to send.
 * @param object aMessage the message to send.
 */
DeveloperToolbar.prototype.sendMessageToTab =
function DT_sendMessageToTab(aTab, aName, aMessage)
{
  let tabId = aTab.linkedPanel;
  aMessage.hudId = tabId;
  if (!("id" in aMessage)) {
    aMessage.id = "DevToolbar-" + this.sequenceId;
  }

  aTab.linkedBrowser.messageManager.sendAsyncMessage(aName, aMessage);
};

/**
 * Process a "WebConsole:PageError" message received from the given tab. This
 * method counts the JavaScript exceptions received.
 *
 * @private
 * @param string aTabId the ID of the tab from where the page error comes.
 * @param object aPageError the page error object received from the content
 * process.
 */
DeveloperToolbar.prototype._onPageError =
function DT__onPageError(aTabId, aPageError)
{
  if (aPageError.category == "CSS Parser" ||
      aPageError.category == "CSS Loader" ||
      (aPageError.flags & aPageError.warningFlag) ||
      (aPageError.flags & aPageError.strictFlag)) {
    return; // just a CSS or JS warning
  }

  this._errorsCount[aTabId]++;
};

/**
 * The |beforeunload| event handler. This function resets the errors count when
 * a different page starts loading.
 *
 * @private
 * @param nsIDOMEvent aEvent the beforeunload DOM event.
 */
DeveloperToolbar.prototype._onPageBeforeUnload =
function DT__onPageBeforeUnload(aEvent)
{
  let window = aEvent.target.defaultView;
  if (window.top !== window) {
    return;
  }

  let tabs = this._chromeWindow.getBrowser().tabs;
  Array.prototype.some.call(tabs, function(aTab) {
    if (aTab.linkedBrowser.contentWindow === window) {
      let tabId = aTab.linkedPanel;
      if (tabId in this._errorsCount) {
        this._errorsCount[tabId] = 0;
        this._updateErrorsCount(tabId);
      }
      return true;
    }
    return false;
  }, this);
};

/**
 * Update the page errors count displayed in the Web Console button for the
 * currently selected tab.
 *
 * @private
 * @param string [aChangedTabId] Optional. The tab ID that had its page errors
 * count changed. If this is provided and it doesn't match the currently
 * selected tab, then the button is not updated.
 */
DeveloperToolbar.prototype._updateErrorsCount =
function DT__updateErrorsCount(aChangedTabId)
{
  let tabId = this._chromeWindow.getBrowser().selectedTab.linkedPanel;
  if (aChangedTabId && tabId != aChangedTabId) {
    return;
  }

  let errors = this._errorsCount[tabId];

  if (errors) {
    this._webConsoleButton.setAttribute("error-count", errors);
  } else {
    this._webConsoleButton.removeAttribute("error-count");
  }
};

/**
 * Reset the errors counter for the given tab.
 *
 * @param nsIDOMElement aTab The xul:tab for which you want to reset the page
 * errors counters.
 */
DeveloperToolbar.prototype.resetErrorsCount =
function DT_resetErrorsCount(aTab)
{
  let tabId = aTab.linkedPanel;
  if (tabId in this._errorsCount) {
    this._errorsCount[tabId] = 0;
    this._updateErrorsCount(tabId);
  }
};

/**
 * Panel to handle command line output.
 * @param aChromeDoc document from which we can pull the parts we need.
 * @param aInput the input element that should get focus.
 * @param aLoadCallback called when the panel is loaded properly.
 */
function OutputPanel(aChromeDoc, aInput, aLoadCallback)
{
  this._input = aInput;
  this._toolbar = aChromeDoc.getElementById("developer-toolbar");

  this._loadCallback = aLoadCallback;

  /*
  <panel id="gcli-output"
         noautofocus="true"
         noautohide="true"
         class="gcli-panel">
    <html:iframe xmlns:html="http://www.w3.org/1999/xhtml"
                 id="gcli-output-frame"
                 src="chrome://browser/content/devtools/gclioutput.xhtml"
                 flex="1"/>
  </panel>
  */
  this._panel = aChromeDoc.createElement("panel");
  this._panel.id = "gcli-output";
  this._panel.classList.add("gcli-panel");
  this._panel.setAttribute("noautofocus", "true");
  this._panel.setAttribute("noautohide", "true");
  this._toolbar.parentElement.insertBefore(this._panel, this._toolbar);

  this._frame = aChromeDoc.createElementNS(NS_XHTML, "iframe");
  this._frame.id = "gcli-output-frame";
  this._frame.setAttribute("src", "chrome://browser/content/devtools/gclioutput.xhtml");
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

  this.document = this._frame.contentDocument;

  this._div = this.document.getElementById("gcli-output-root");
  this._div.classList.add('gcli-row-out');
  this._div.setAttribute('aria-live', 'assertive');

  let styles = this._toolbar.ownerDocument.defaultView
                  .getComputedStyle(this._toolbar);
  this._div.setAttribute("dir", styles.direction);

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
  // This is nasty, but displaying the panel causes it to re-flow, which can
  // change the size it should be, so we need to resize the iframe after the
  // panel has displayed
  this._panel.ownerDocument.defaultView.setTimeout(function() {
    this._resize();
  }.bind(this), 0);

  this._panel.openPopup(this._input, "before_start", 0, 0, false, false, null);
  this._resize();

  this._input.focus();
};

/**
 * Internal helper to set the height of the output panel to fit the available
 * content;
 */
OutputPanel.prototype._resize = function CLP_resize()
{
  if (this._panel == null || this.document == null || !this._panel.state == "closed") {
    return
  }

  this._frame.height = this.document.body.scrollHeight;
  this._frame.width = this._input.clientWidth + 2;
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
  this._toolbar.parentElement.removeChild(this._panel);

  delete this._input;
  delete this._toolbar;
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
  this._toolbar = aChromeDoc.getElementById("developer-toolbar");
  this._dimensions = { start: 0, end: 0 };

  this._onload = this._onload.bind(this);
  this._loadCallback = aLoadCallback;
  /*
  <panel id="gcli-tooltip"
         type="arrow"
         noautofocus="true"
         noautohide="true"
         class="gcli-panel">
    <html:iframe xmlns:html="http://www.w3.org/1999/xhtml"
                 id="gcli-tooltip-frame"
                 src="chrome://browser/content/devtools/gclitooltip.xhtml"
                 flex="1"/>
  </panel>
  */
  this._panel = aChromeDoc.createElement("panel");
  this._panel.id = "gcli-tooltip";
  this._panel.classList.add("gcli-panel");
  this._panel.setAttribute("noautofocus", "true");
  this._panel.setAttribute("noautohide", "true");
  this._toolbar.parentElement.insertBefore(this._panel, this._toolbar);

  this._frame = aChromeDoc.createElementNS(NS_XHTML, "iframe");
  this._frame.id = "gcli-tooltip-frame";
  this._frame.setAttribute("src", "chrome://browser/content/devtools/gclitooltip.xhtml");
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

  this.document = this._frame.contentDocument;
  this.hintElement = this.document.getElementById("gcli-tooltip-root");
  this._connector = this.document.getElementById("gcli-tooltip-connector");

  let styles = this._toolbar.ownerDocument.defaultView
                  .getComputedStyle(this._toolbar);
  this.hintElement.setAttribute("dir", styles.direction);

  this.loaded = true;

  if (this._loadCallback) {
    this._loadCallback();
    delete this._loadCallback;
  }
};

/**
 * Display the TooltipPanel.
 */
TooltipPanel.prototype.show = function TP_show(aDimensions)
{
  if (!aDimensions) {
    aDimensions = { start: 0, end: 0 };
  }
  this._dimensions = aDimensions;

  // This is nasty, but displaying the panel causes it to re-flow, which can
  // change the size it should be, so we need to resize the iframe after the
  // panel has displayed
  this._panel.ownerDocument.defaultView.setTimeout(function() {
    this._resize();
  }.bind(this), 0);

  this._resize();
  this._panel.openPopup(this._input, "before_start", aDimensions.start * 10, 0, false, false, null);
  this._input.focus();
};

/**
 * One option is to spend lots of time taking an average width of characters
 * in the current font, dynamically, and weighting for the frequency of use of
 * various characters, or even to render the given string off screen, and then
 * measure the width.
 * Or we could do this...
 */
const AVE_CHAR_WIDTH = 4.5;

/**
 * Display the TooltipPanel.
 */
TooltipPanel.prototype._resize = function TP_resize()
{
  if (this._panel == null || this.document == null || !this._panel.state == "closed") {
    return
  }

  let offset = 10 + Math.floor(this._dimensions.start * AVE_CHAR_WIDTH);
  this._panel.style.marginLeft = offset + "px";

  /*
  // Bug 744906: UX review - Not sure if we want this code to fatten connector
  // with param width
  let width = Math.floor(this._dimensions.end * AVE_CHAR_WIDTH);
  width = Math.min(width, 100);
  width = Math.max(width, 10);
  this._connector.style.width = width + "px";
  */

  this._frame.height = this.document.body.scrollHeight;
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
  this._toolbar.parentElement.removeChild(this._panel);

  delete this._connector;
  delete this._dimensions;
  delete this._input;
  delete this._onload;
  delete this._panel;
  delete this._frame;
  delete this._toolbar;
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
    this.show(aEvent.dimensions);
  } else {
    this._panel.hidePopup();
  }
};
