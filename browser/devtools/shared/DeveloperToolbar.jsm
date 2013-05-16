/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "DeveloperToolbar", "CommandUtils" ];

const NS_XHTML = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource:///modules/devtools/Commands.jsm");

const Node = Components.interfaces.nsIDOMNode;

XPCOMUtils.defineLazyModuleGetter(this, "console",
                                  "resource://gre/modules/devtools/Console.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "gcli",
                                  "resource://gre/modules/devtools/gcli.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "CmdCommands",
                                  "resource:///modules/devtools/BuiltinCommands.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PageErrorListener",
                                  "resource://gre/modules/devtools/WebConsoleUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "devtools",
                                  "resource://gre/modules/devtools/Loader.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "require",
                                  "resource://gre/modules/devtools/Require.jsm");

XPCOMUtils.defineLazyGetter(this, "prefBranch", function() {
  let prefService = Components.classes["@mozilla.org/preferences-service;1"]
          .getService(Components.interfaces.nsIPrefService);
  return prefService.getBranch(null)
          .QueryInterface(Components.interfaces.nsIPrefBranch2);
});

XPCOMUtils.defineLazyGetter(this, "toolboxStrings", function () {
  return Services.strings.createBundle("chrome://browser/locale/devtools/toolbox.properties");
});

const converters = require("gcli/converters");

/**
 * A collection of utilities to help working with commands
 */
let CommandUtils = {
  /**
   * Read a toolbarSpec from preferences
   * @param aPref The name of the preference to read
   */
  getCommandbarSpec: function CU_getCommandbarSpec(aPref) {
    let value = prefBranch.getComplexValue(aPref,
                               Components.interfaces.nsISupportsString).data;
    return JSON.parse(value);
  },

  /**
   * A toolbarSpec is an array of buttonSpecs. A buttonSpec is an array of
   * strings each of which is a GCLI command (including args if needed).
   *
   * Warning: this method uses the unload event of the window that owns the
   * buttons that are of type checkbox. this means that we don't properly
   * unregister event handlers until the window is destroyed.
   */
  createButtons: function CU_createButtons(toolbarSpec, target, document, requisition) {
    let reply = [];

    toolbarSpec.forEach(function(buttonSpec) {
      let button = document.createElement("toolbarbutton");
      reply.push(button);

      if (typeof buttonSpec == "string") {
        buttonSpec = { typed: buttonSpec };
      }
      // Ask GCLI to parse the typed string (doesn't execute it)
      requisition.update(buttonSpec.typed);

      // Ignore invalid commands
      let command = requisition.commandAssignment.value;
      if (command == null) {
        // TODO: Have a broken icon
        // button.icon = 'Broken';
        button.setAttribute("label", "X");
        button.setAttribute("tooltip", "Unknown command: " + buttonSpec.typed);
        button.setAttribute("disabled", "true");
      }
      else {
        if (command.buttonId != null) {
          button.id = command.buttonId;
        }
        if (command.buttonClass != null) {
          button.className = command.buttonClass;
        }
        if (command.tooltipText != null) {
          button.setAttribute("tooltiptext", command.tooltipText);
        }
        else if (command.description != null) {
          button.setAttribute("tooltiptext", command.description);
        }

        button.addEventListener("click", function() {
          requisition.update(buttonSpec.typed);
          //if (requisition.getStatus() == Status.VALID) {
            requisition.exec();
          /*
          }
          else {
            console.error('incomplete commands not yet supported');
          }
          */
        }, false);

        // Allow the command button to be toggleable
        if (command.state) {
          button.setAttribute("autocheck", false);
          let onChange = function(event, eventTab) {
            if (eventTab == target.tab) {
              if (command.state.isChecked(target)) {
                button.setAttribute("checked", true);
              }
              else if (button.hasAttribute("checked")) {
                button.removeAttribute("checked");
              }
            }
          };
          command.state.onChange(target, onChange);
          onChange(null, target.tab);
          document.defaultView.addEventListener("unload", function() {
            command.state.offChange(target, onChange);
          }, false);
        }
      }
    });

    requisition.update('');

    return reply;
  },

  /**
   * A helper function to create the environment object that is passed to
   * GCLI commands.
   */
  createEnvironment: function(chromeDocument, contentDocument) {
    let environment = {
      chromeDocument: chromeDocument,
      chromeWindow: chromeDocument.defaultView,

      document: contentDocument,
      window: contentDocument.defaultView
    };

    Object.defineProperty(environment, "target", {
      get: function() {
        let tab = chromeDocument.defaultView.getBrowser().selectedTab;
        return devtools.TargetFactory.forTab(tab);
      },
      enumerable: true
    });

    return environment;
  },
};

this.CommandUtils = CommandUtils;

/**
 * Due to a number of panel bugs we need a way to check if we are running on
 * Linux. See the comments for TooltipPanel and OutputPanel for further details.
 *
 * When bug 780102 is fixed all isLinux checks can be removed and we can revert
 * to using panels.
 */
XPCOMUtils.defineLazyGetter(this, "isLinux", function () {
  return OS == "Linux";
});

XPCOMUtils.defineLazyGetter(this, "OS", function () {
  let os = Components.classes["@mozilla.org/xre/app-info;1"]
           .getService(Components.interfaces.nsIXULRuntime).OS;
  return os;
});

/**
 * A component to manage the global developer toolbar, which contains a GCLI
 * and buttons for various developer tools.
 * @param aChromeWindow The browser window to which this toolbar is attached
 * @param aToolbarElement See browser.xul:<toolbar id="developer-toolbar">
 */
this.DeveloperToolbar = function DeveloperToolbar(aChromeWindow, aToolbarElement)
{
  this._chromeWindow = aChromeWindow;

  this._element = aToolbarElement;
  this._element.hidden = true;
  this._doc = this._element.ownerDocument;

  this._lastState = NOTIFICATIONS.HIDE;
  this._pendingShowCallback = undefined;
  this._pendingHide = false;
  this._errorsCount = {};
  this._warningsCount = {};
  this._errorListeners = {};
  this._errorCounterButton = this._doc
                             .getElementById("developer-toolbar-toolbox-button");
  this._errorCounterButton._defaultTooltipText =
    this._errorCounterButton.getAttribute("tooltiptext");

  try {
    CmdCommands.refreshAutoCommands(aChromeWindow);
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

/**
 * Is the toolbar open?
 */
Object.defineProperty(DeveloperToolbar.prototype, 'visible', {
  get: function DT_visible() {
    return !this._element.hidden;
  },
  enumerable: true
});

let _gSequenceId = 0;

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
 * Called from browser.xul in response to menu-click or keyboard shortcut to
 * toggle the toolbar
 */
DeveloperToolbar.prototype.focusToggle = function DT_focusToggle()
{
  if (this.visible) {
    // If we have focus then the active element is the HTML input contained
    // inside the xul input element
    let active = this._chromeWindow.document.activeElement;
    let position = this._input.compareDocumentPosition(active);
    if (position & Node.DOCUMENT_POSITION_CONTAINED_BY) {
      this.hide();
    }
    else {
      this._input.focus();
    }
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
  this.outputPanel = new OutputPanel(this, checkLoad);
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
    environment: CommandUtils.createEnvironment(this._doc, contentDocument),
    tooltipClass: 'gcliterm-tooltip',
    eval: null,
    scratchpad: null
  });

  this.display.focusManager.addMonitoredElement(this.outputPanel._frame);
  this.display.focusManager.addMonitoredElement(this._element);

  this.display.onVisibilityChange.add(this.outputPanel._visibilityChanged,
                                      this.outputPanel);
  this.display.onVisibilityChange.add(this.tooltipPanel._visibilityChanged,
                                      this.tooltipPanel);
  this.display.onOutput.add(this.outputPanel._outputChanged, this.outputPanel);

  let tabbrowser = this._chromeWindow.getBrowser();
  tabbrowser.tabContainer.addEventListener("TabSelect", this, false);
  tabbrowser.tabContainer.addEventListener("TabClose", this, false);
  tabbrowser.addEventListener("load", this, true);
  tabbrowser.addEventListener("beforeunload", this, true);

  this._initErrorsCount(tabbrowser.selectedTab);

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

  let window = aTab.linkedBrowser.contentWindow;
  let listener = new PageErrorListener(window, {
    onPageError: this._onPageError.bind(this, tabId),
  });
  listener.init();

  this._errorListeners[tabId] = listener;
  this._errorsCount[tabId] = 0;
  this._warningsCount[tabId] = 0;

  let messages = listener.getCachedMessages();
  messages.forEach(this._onPageError.bind(this, tabId));

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
  if (!(tabId in this._errorsCount) || !(tabId in this._warningsCount)) {
    this._updateErrorsCount();
    return;
  }

  this._errorListeners[tabId].destroy();
  delete this._errorListeners[tabId];
  delete this._errorsCount[tabId];
  delete this._warningsCount[tabId];

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
  if (this._lastState == NOTIFICATIONS.HIDE) {
    return;
  }

  let tabbrowser = this._chromeWindow.getBrowser();
  tabbrowser.tabContainer.removeEventListener("TabSelect", this, false);
  tabbrowser.tabContainer.removeEventListener("TabClose", this, false);
  tabbrowser.removeEventListener("load", this, true);
  tabbrowser.removeEventListener("beforeunload", this, true);

  Array.prototype.forEach.call(tabbrowser.tabs, this._stopErrorsCount, this);

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

  this._lastState = NOTIFICATIONS.HIDE;
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
        environment: CommandUtils.createEnvironment(this._doc, contentDocument),
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
 * Count a page error received for the currently selected tab. This
 * method counts the JavaScript exceptions received and CSS errors/warnings.
 *
 * @private
 * @param string aTabId the ID of the tab from where the page error comes.
 * @param object aPageError the page error object received from the
 * PageErrorListener.
 */
DeveloperToolbar.prototype._onPageError =
function DT__onPageError(aTabId, aPageError)
{
  if (aPageError.category == "CSS Parser" ||
      aPageError.category == "CSS Loader") {
    return;
  }
  if ((aPageError.flags & aPageError.warningFlag) ||
      (aPageError.flags & aPageError.strictFlag)) {
    this._warningsCount[aTabId]++;
  } else {
    this._errorsCount[aTabId]++;
  }
  this._updateErrorsCount(aTabId);
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
      if (tabId in this._errorsCount || tabId in this._warningsCount) {
        this._errorsCount[tabId] = 0;
        this._warningsCount[tabId] = 0;
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
  let warnings = this._warningsCount[tabId];
  let btn = this._errorCounterButton;
  if (errors) {
    let errorsText = toolboxStrings
                     .GetStringFromName("toolboxToggleButton.errors");
    errorsText = PluralForm.get(errors, errorsText).replace("#1", errors);

    let warningsText = toolboxStrings
                       .GetStringFromName("toolboxToggleButton.warnings");
    warningsText = PluralForm.get(warnings, warningsText).replace("#1", warnings);

    let tooltiptext = toolboxStrings
                      .formatStringFromName("toolboxToggleButton.tooltip",
                                            [errorsText, warningsText], 2);

    btn.setAttribute("error-count", errors);
    btn.setAttribute("tooltiptext", tooltiptext);
  } else {
    btn.removeAttribute("error-count");
    btn.setAttribute("tooltiptext", btn._defaultTooltipText);
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
  if (tabId in this._errorsCount || tabId in this._warningsCount) {
    this._errorsCount[tabId] = 0;
    this._warningsCount[tabId] = 0;
    this._updateErrorsCount(tabId);
  }
};

/**
 * Panel to handle command line output.
 *
 * There is a tooltip bug on Windows and OSX that prevents tooltips from being
 * positioned properly (bug 786975). There is a Gnome panel bug on Linux that
 * causes ugly focus issues (https://bugzilla.gnome.org/show_bug.cgi?id=621848).
 * We now use a tooltip on Linux and a panel on OSX & Windows.
 *
 * If a panel has no content and no height it is not shown when openPopup is
 * called on Windows and OSX (bug 692348) ... this prevents the panel from
 * appearing the first time it is shown. Setting the panel's height to 1px
 * before calling openPopup works around this issue as we resize it ourselves
 * anyway.
 *
 * @param aChromeDoc document from which we can pull the parts we need.
 * @param aInput the input element that should get focus.
 * @param aLoadCallback called when the panel is loaded properly.
 */
function OutputPanel(aDevToolbar, aLoadCallback)
{
  this._devtoolbar = aDevToolbar;
  this._input = this._devtoolbar._input;
  this._toolbar = this._devtoolbar._doc.getElementById("developer-toolbar");

  this._loadCallback = aLoadCallback;

  /*
  <tooltip|panel id="gcli-output"
         noautofocus="true"
         noautohide="true"
         class="gcli-panel">
    <html:iframe xmlns:html="http://www.w3.org/1999/xhtml"
                 id="gcli-output-frame"
                 src="chrome://browser/content/devtools/commandlineoutput.xhtml"
                 sandbox="allow-same-origin"/>
  </tooltip|panel>
  */

  // TODO: Switch back from tooltip to panel when metacity focus issue is fixed:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=780102
  this._panel = this._devtoolbar._doc.createElement(isLinux ? "tooltip" : "panel");

  this._panel.id = "gcli-output";
  this._panel.classList.add("gcli-panel");

  if (isLinux) {
    this.canHide = false;
    this._onpopuphiding = this._onpopuphiding.bind(this);
    this._panel.addEventListener("popuphiding", this._onpopuphiding, true);
  } else {
    this._panel.setAttribute("noautofocus", "true");
    this._panel.setAttribute("noautohide", "true");

    // Bug 692348: On Windows and OSX if a panel has no content and no height
    // openPopup fails to display it. Setting the height to 1px alows the panel
    // to be displayed before has content or a real height i.e. the first time
    // it is displayed.
    this._panel.setAttribute("height", "1px");
  }

  this._toolbar.parentElement.insertBefore(this._panel, this._toolbar);

  this._frame = this._devtoolbar._doc.createElementNS(NS_XHTML, "iframe");
  this._frame.id = "gcli-output-frame";
  this._frame.setAttribute("src", "chrome://browser/content/devtools/commandlineoutput.xhtml");
  this._frame.setAttribute("sandbox", "allow-same-origin");
  this._panel.appendChild(this._frame);

  this.displayedOutput = undefined;

  this._onload = this._onload.bind(this);
  this._update = this._update.bind(this);
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
 * Prevent the popup from hiding if it is not permitted via this.canHide.
 */
OutputPanel.prototype._onpopuphiding = function OP_onpopuphiding(aEvent)
{
  // TODO: When we switch back from tooltip to panel we can remove this hack:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=780102
  if (isLinux && !this.canHide) {
    aEvent.preventDefault();
  }
};

/**
 * Display the OutputPanel.
 */
OutputPanel.prototype.show = function OP_show()
{
  if (isLinux) {
    this.canHide = false;
  }

  // We need to reset the iframe size in order for future size calculations to
  // be correct
  this._frame.style.minHeight = this._frame.style.maxHeight = 0;
  this._frame.style.minWidth = 0;

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

  // Set max panel width to match any content with a max of the width of the
  // browser window.
  let maxWidth = this._panel.ownerDocument.documentElement.clientWidth;

  // Adjust max width according to OS.
  // We'd like to put this in CSS but we can't:
  //   body { width: calc(min(-5px, max-content)); }
  //   #_panel { max-width: -5px; }
  switch(OS) {
    case "Linux":
      maxWidth -= 5;
      break;
    case "Darwin":
      maxWidth -= 25;
      break;
    case "WINNT":
      maxWidth -= 5;
      break;
  }

  this.document.body.style.width = "-moz-max-content";
  let style = this._frame.contentWindow.getComputedStyle(this.document.body);
  let frameWidth = parseInt(style.width, 10);
  let width = Math.min(maxWidth, frameWidth);
  this.document.body.style.width = width + "px";

  // Set the width of the iframe.
  this._frame.style.minWidth = width + "px";
  this._panel.style.maxWidth = maxWidth + "px";

  // browserAdjustment is used to correct the panel height according to the
  // browsers borders etc.
  const browserAdjustment = 15;

  // Set max panel height to match any content with a max of the height of the
  // browser window.
  let maxHeight =
    this._panel.ownerDocument.documentElement.clientHeight - browserAdjustment;
  let height = Math.min(maxHeight, this.document.documentElement.scrollHeight);

  // Set the height of the iframe. Setting iframe.height does not work.
  this._frame.style.minHeight = this._frame.style.maxHeight = height + "px";

  // Set the height and width of the panel to match the iframe.
  this._panel.sizeTo(width, height);

  // Move the panel to the correct position in the case that it has been
  // positioned incorrectly.
  let screenX = this._input.boxObject.screenX;
  let screenY = this._toolbar.boxObject.screenY;
  this._panel.moveTo(screenX, screenY - height);
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
  this.displayedOutput.onClose.add(this.remove, this);

  if (this.displayedOutput.completed) {
    this._update();
  }
  else {
    this.displayedOutput.promise.then(this._update, this._update)
                                .then(null, console.error);
  }
};

/**
 * Called when displayed Output says it's changed or from outputChanged, which
 * happens when there is a new displayed Output.
 */
OutputPanel.prototype._update = function OP_update()
{
  // destroy has been called, bail out
  if (this._div == null) {
    return;
  }

  // Empty this._div
  while (this._div.hasChildNodes()) {
    this._div.removeChild(this._div.firstChild);
  }

  if (this.displayedOutput.data != null) {
    let requisition = this._devtoolbar.display.requisition;
    let nodePromise = converters.convert(this.displayedOutput.data,
                                         this.displayedOutput.type, 'dom',
                                         requisition.conversionContext);
    nodePromise.then(function(node) {
      while (this._div.hasChildNodes()) {
        this._div.removeChild(this._div.firstChild);
      }

      var links = node.ownerDocument.querySelectorAll('*[href]');
      for (var i = 0; i < links.length; i++) {
        links[i].setAttribute('target', '_blank');
      }

      this._div.appendChild(node);
    }.bind(this));
    this.show();
  }
};

/**
 * Detach listeners from the currently displayed Output.
 */
OutputPanel.prototype.remove = function OP_remove()
{
  if (isLinux) {
    this.canHide = true;
  }

  if (this._panel && this._panel.hidePopup) {
    this._panel.hidePopup();
  }

  if (this.displayedOutput) {
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

  this._panel.removeEventListener("popuphiding", this._onpopuphiding, true);

  this._panel.removeChild(this._frame);
  this._toolbar.parentElement.removeChild(this._panel);

  delete this._devtoolbar;
  delete this._input;
  delete this._toolbar;
  delete this._onload;
  delete this._onpopuphiding;
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
    if (isLinux) {
      this.canHide = true;
    }
    this._panel.hidePopup();
  }
};


/**
 * Panel to handle tooltips.
 *
 * There is a tooltip bug on Windows and OSX that prevents tooltips from being
 * positioned properly (bug 786975). There is a Gnome panel bug on Linux that
 * causes ugly focus issues (https://bugzilla.gnome.org/show_bug.cgi?id=621848).
 * We now use a tooltip on Linux and a panel on OSX & Windows.
 *
 * If a panel has no content and no height it is not shown when openPopup is
 * called on Windows and OSX (bug 692348) ... this prevents the panel from
 * appearing the first time it is shown. Setting the panel's height to 1px
 * before calling openPopup works around this issue as we resize it ourselves
 * anyway.
 *
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
  <tooltip|panel id="gcli-tooltip"
         type="arrow"
         noautofocus="true"
         noautohide="true"
         class="gcli-panel">
    <html:iframe xmlns:html="http://www.w3.org/1999/xhtml"
                 id="gcli-tooltip-frame"
                 src="chrome://browser/content/devtools/commandlinetooltip.xhtml"
                 flex="1"
                 sandbox="allow-same-origin"/>
  </tooltip|panel>
  */

  // TODO: Switch back from tooltip to panel when metacity focus issue is fixed:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=780102
  this._panel = aChromeDoc.createElement(isLinux ? "tooltip" : "panel");

  this._panel.id = "gcli-tooltip";
  this._panel.classList.add("gcli-panel");

  if (isLinux) {
    this.canHide = false;
    this._onpopuphiding = this._onpopuphiding.bind(this);
    this._panel.addEventListener("popuphiding", this._onpopuphiding, true);
  } else {
    this._panel.setAttribute("noautofocus", "true");
    this._panel.setAttribute("noautohide", "true");

    // Bug 692348: On Windows and OSX if a panel has no content and no height
    // openPopup fails to display it. Setting the height to 1px alows the panel
    // to be displayed before has content or a real height i.e. the first time
    // it is displayed.
    this._panel.setAttribute("height", "1px");
  }

  this._toolbar.parentElement.insertBefore(this._panel, this._toolbar);

  this._frame = aChromeDoc.createElementNS(NS_XHTML, "iframe");
  this._frame.id = "gcli-tooltip-frame";
  this._frame.setAttribute("src", "chrome://browser/content/devtools/commandlinetooltip.xhtml");
  this._frame.setAttribute("flex", "1");
  this._frame.setAttribute("sandbox", "allow-same-origin");
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
 * Prevent the popup from hiding if it is not permitted via this.canHide.
 */
TooltipPanel.prototype._onpopuphiding = function TP_onpopuphiding(aEvent)
{
  // TODO: When we switch back from tooltip to panel we can remove this hack:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=780102
  if (isLinux && !this.canHide) {
    aEvent.preventDefault();
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

  if (isLinux) {
    this.canHide = false;
  }

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
  if (isLinux) {
    this.canHide = true;
  }
  if (this._panel && this._panel.hidePopup) {
    this._panel.hidePopup();
  }
};

/**
 * Hide the TooltipPanel.
 */
TooltipPanel.prototype.destroy = function TP_destroy()
{
  this.remove();

  this._panel.removeEventListener("popuphiding", this._onpopuphiding, true);

  this._panel.removeChild(this._frame);
  this._toolbar.parentElement.removeChild(this._panel);

  delete this._connector;
  delete this._dimensions;
  delete this._input;
  delete this._onload;
  delete this._onpopuphiding;
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
    if (isLinux) {
      this.canHide = true;
    }
    this._panel.hidePopup();
  }
};
