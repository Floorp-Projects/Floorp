/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, Ci, Cu } = require("chrome");
const promise = require("promise");
const defer = require("devtools/shared/defer");
const Services = require("Services");
const { TargetFactory } = require("devtools/client/framework/target");
const Telemetry = require("devtools/client/shared/telemetry");
const {ViewHelpers} = require("devtools/client/shared/widgets/view-helpers");

const NS_XHTML = "http://www.w3.org/1999/xhtml";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

loader.lazyImporter(this, "PluralForm", "resource://gre/modules/PluralForm.jsm");

loader.lazyGetter(this, "prefBranch", function () {
  return Services.prefs.getBranch(null)
                    .QueryInterface(Ci.nsIPrefBranch2);
});
loader.lazyGetter(this, "toolboxStrings", function () {
  return Services.strings.createBundle("chrome://devtools/locale/toolbox.properties");
});

loader.lazyRequireGetter(this, "gcliInit", "devtools/shared/gcli/commands/index");
loader.lazyRequireGetter(this, "util", "gcli/util/util");
loader.lazyRequireGetter(this, "ConsoleServiceListener", "devtools/server/actors/utils/webconsole-utils", true);
loader.lazyRequireGetter(this, "gDevTools", "devtools/client/framework/devtools", true);
loader.lazyRequireGetter(this, "gDevToolsBrowser", "devtools/client/framework/devtools-browser", true);
loader.lazyRequireGetter(this, "nodeConstants", "devtools/shared/dom-node-constants", true);
loader.lazyRequireGetter(this, "EventEmitter", "devtools/shared/event-emitter");

/**
 * A collection of utilities to help working with commands
 */
var CommandUtils = {
  /**
   * Utility to ensure that things are loaded in the correct order
   */
  createRequisition: function (target, options) {
    if (!gcliInit) {
      return promise.reject("Unable to load gcli");
    }
    return gcliInit.getSystem(target).then(system => {
      var Requisition = require("gcli/cli").Requisition;
      return new Requisition(system, options);
    });
  },

  /**
   * Destroy the remote side of the requisition as well as the local side
   */
  destroyRequisition: function (requisition, target) {
    requisition.destroy();
    gcliInit.releaseSystem(target);
  },

  /**
   * Read a toolbarSpec from preferences
   * @param pref The name of the preference to read
   */
  getCommandbarSpec: function (pref) {
    let value = prefBranch.getComplexValue(pref, Ci.nsISupportsString).data;
    return JSON.parse(value);
  },

  /**
   * A toolbarSpec is an array of strings each of which is a GCLI command.
   *
   * Warning: this method uses the unload event of the window that owns the
   * buttons that are of type checkbox. this means that we don't properly
   * unregister event handlers until the window is destroyed.
   */
  createButtons: function (toolbarSpec, target, document, requisition) {
    return util.promiseEach(toolbarSpec, typed => {
      // Ask GCLI to parse the typed string (doesn't execute it)
      return requisition.update(typed).then(() => {
        let button = document.createElementNS(NS_XHTML, "button");

        // Ignore invalid commands
        let command = requisition.commandAssignment.value;
        if (command == null) {
          throw new Error("No command '" + typed + "'");
        }

        if (command.buttonId != null) {
          button.id = command.buttonId;
          if (command.buttonClass != null) {
            button.className = command.buttonClass;
          }
        }
        else {
          button.setAttribute("text-as-image", "true");
          button.setAttribute("label", command.name);
        }

        button.classList.add("devtools-button");

        if (command.tooltipText != null) {
          button.setAttribute("title", command.tooltipText);
        }
        else if (command.description != null) {
          button.setAttribute("title", command.description);
        }

        button.addEventListener("click", () => {
          requisition.updateExec(typed);
        }, false);

        button.addEventListener("keypress", (event) => {
          if (ViewHelpers.isSpaceOrReturn(event)) {
            event.preventDefault();
            requisition.updateExec(typed);
          }
        }, false);

        // Allow the command button to be toggleable
        if (command.state) {
          button.setAttribute("autocheck", false);

          /**
           * The onChange event should be called with an event object that
           * contains a target property which specifies which target the event
           * applies to. For legacy reasons the event object can also contain
           * a tab property.
           */
          let onChange = (eventName, ev) => {
            if (ev.target == target || ev.tab == target.tab) {

              let updateChecked = (checked) => {
                if (checked) {
                  button.setAttribute("checked", true);
                }
                else if (button.hasAttribute("checked")) {
                  button.removeAttribute("checked");
                }
              };

              // isChecked would normally be synchronous. An annoying quirk
              // of the 'csscoverage toggle' command forces us to accept a
              // promise here, but doing Promise.resolve(reply).then(...) here
              // makes this async for everyone, which breaks some tests so we
              // treat non-promise replies separately to keep then synchronous.
              let reply = command.state.isChecked(target);
              if (typeof reply.then == "function") {
                reply.then(updateChecked, console.error);
              }
              else {
                updateChecked(reply);
              }
            }
          };

          command.state.onChange(target, onChange);
          onChange("", { target: target });
          document.defaultView.addEventListener("unload", () => {
            if (command.state.offChange) {
              command.state.offChange(target, onChange);
            }
          }, false);
        }

        requisition.clear();

        return button;
      });
    });
  },

  /**
   * A helper function to create the environment object that is passed to
   * GCLI commands.
   * @param targetContainer An object containing a 'target' property which
   * reflects the current debug target
   */
  createEnvironment: function (container, targetProperty = "target") {
    if (!container[targetProperty].toString ||
        !/TabTarget/.test(container[targetProperty].toString())) {
      throw new Error("Missing target");
    }

    return {
      get target() {
        if (!container[targetProperty].toString ||
            !/TabTarget/.test(container[targetProperty].toString())) {
          throw new Error("Removed target");
        }

        return container[targetProperty];
      },

      get chromeWindow() {
        return this.target.tab.ownerDocument.defaultView;
      },

      get chromeDocument() {
        return this.target.tab.ownerDocument.defaultView.document;
      },

      get window() {
        // throw new Error("environment.window is not available in runAt:client commands");
        return this.chromeWindow.gBrowser.contentWindowAsCPOW;
      },

      get document() {
        // throw new Error("environment.document is not available in runAt:client commands");
        return this.chromeWindow.gBrowser.contentDocumentAsCPOW;
      }
    };
  },
};

exports.CommandUtils = CommandUtils;

/**
 * Due to a number of panel bugs we need a way to check if we are running on
 * Linux. See the comments for TooltipPanel and OutputPanel for further details.
 *
 * When bug 780102 is fixed all isLinux checks can be removed and we can revert
 * to using panels.
 */
loader.lazyGetter(this, "isLinux", function () {
  return Services.appinfo.OS == "Linux";
});
loader.lazyGetter(this, "isMac", function () {
  return Services.appinfo.OS == "Darwin";
});

/**
 * A component to manage the global developer toolbar, which contains a GCLI
 * and buttons for various developer tools.
 * @param aChromeWindow The browser window to which this toolbar is attached
 */
function DeveloperToolbar(aChromeWindow)
{
  this._chromeWindow = aChromeWindow;

  this.target = null; // Will be setup when show() is called

  this._doc = aChromeWindow.document;

  this._telemetry = new Telemetry();
  this._errorsCount = {};
  this._warningsCount = {};
  this._errorListeners = {};

  this._onToolboxReady = this._onToolboxReady.bind(this);
  this._onToolboxDestroyed = this._onToolboxDestroyed.bind(this);

  EventEmitter.decorate(this);
}
exports.DeveloperToolbar = DeveloperToolbar;

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
Object.defineProperty(DeveloperToolbar.prototype, "visible", {
  get: function () {
    return this._element && !this._element.hidden;
  },
  enumerable: true
});

var _gSequenceId = 0;

/**
 * Getter for a unique ID.
 */
Object.defineProperty(DeveloperToolbar.prototype, "sequenceId", {
  get: function () {
    return _gSequenceId++;
  },
  enumerable: true
});

/**
 * Create the <toolbar> element to insert within browser UI
 */
DeveloperToolbar.prototype.createToolbar = function () {
  if (this._element) {
    return;
  }
  let toolbar = this._doc.createElement("toolbar");
  toolbar.setAttribute("id", "developer-toolbar");
  toolbar.setAttribute("hidden", "true");

  let close = this._doc.createElement("toolbarbutton");
  close.setAttribute("id", "developer-toolbar-closebutton");
  close.setAttribute("class", "close-icon");
  close.setAttribute("oncommand", "DeveloperToolbar.hide();");
  let closeTooltip = toolboxStrings.GetStringFromName("toolbar.closeButton.tooltip");
  close.setAttribute("tooltiptext", closeTooltip);

  let stack = this._doc.createElement("stack");
  stack.setAttribute("flex", "1");

  let input = this._doc.createElement("textbox");
  input.setAttribute("class", "gclitoolbar-input-node");
  input.setAttribute("rows", "1");
  stack.appendChild(input);

  let hbox = this._doc.createElement("hbox");
  hbox.setAttribute("class", "gclitoolbar-complete-node");
  stack.appendChild(hbox);

  let toolboxBtn = this._doc.createElement("toolbarbutton");
  toolboxBtn.setAttribute("id", "developer-toolbar-toolbox-button");
  toolboxBtn.setAttribute("class", "developer-toolbar-button");
  let toolboxTooltip = toolboxStrings.GetStringFromName("toolbar.toolsButton.tooltip");
  toolboxBtn.setAttribute("tooltiptext", toolboxTooltip);
  toolboxBtn.addEventListener("command", function (event) {
    let window = event.target.ownerDocument.defaultView;
    gDevToolsBrowser.toggleToolboxCommand(window.gBrowser);
  });
  this._errorCounterButton = toolboxBtn;
  this._errorCounterButton._defaultTooltipText = toolboxTooltip;

  // On Mac, the close button is on the left,
  // while it is on the right on every other platforms.
  if (isMac) {
    toolbar.appendChild(close);
    toolbar.appendChild(stack);
    toolbar.appendChild(toolboxBtn);
  } else {
    toolbar.appendChild(stack);
    toolbar.appendChild(toolboxBtn);
    toolbar.appendChild(close);
  }

  this._element = toolbar;
  let bottomBox = this._doc.getElementById("browser-bottombox");
  if (bottomBox) {
    bottomBox.appendChild(this._element);
  } else { // SeaMonkey does not have a "browser-bottombox".
    let statusBar = this._doc.getElementById("status-bar");
    if (statusBar)
      statusBar.parentNode.insertBefore(this._element, statusBar);
  }
};

/**
 * Called from browser.xul in response to menu-click or keyboard shortcut to
 * toggle the toolbar
 */
DeveloperToolbar.prototype.toggle = function () {
  if (this.visible) {
    return this.hide().catch(console.error);
  } else {
    return this.show(true).catch(console.error);
  }
};

/**
 * Called from browser.xul in response to menu-click or keyboard shortcut to
 * toggle the toolbar
 */
DeveloperToolbar.prototype.focus = function () {
  if (this.visible) {
    this._input.focus();
    return promise.resolve();
  } else {
    return this.show(true);
  }
};

/**
 * Called from browser.xul in response to menu-click or keyboard shortcut to
 * toggle the toolbar
 */
DeveloperToolbar.prototype.focusToggle = function () {
  if (this.visible) {
    // If we have focus then the active element is the HTML input contained
    // inside the xul input element
    let active = this._chromeWindow.document.activeElement;
    let position = this._input.compareDocumentPosition(active);
    if (position & nodeConstants.DOCUMENT_POSITION_CONTAINED_BY) {
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
 */
DeveloperToolbar.prototype.show = function (focus) {
  if (this._showPromise != null) {
    return this._showPromise;
  }

  // hide() is async, so ensure we don't need to wait for hide() to finish
  var waitPromise = this._hidePromise || promise.resolve();

  this._showPromise = waitPromise.then(() => {
    this.createToolbar();

    Services.prefs.setBoolPref("devtools.toolbar.visible", true);

    this._telemetry.toolOpened("developertoolbar");

    this._notify(NOTIFICATIONS.LOAD);

    this._input = this._doc.querySelector(".gclitoolbar-input-node");

    // Initializing GCLI can only be done when we've got content windows to
    // write to, so this needs to be done asynchronously.
    let panelPromises = [
      TooltipPanel.create(this),
      OutputPanel.create(this)
    ];
    return promise.all(panelPromises).then(panels => {
      [ this.tooltipPanel, this.outputPanel ] = panels;

      this._doc.getElementById("menu_devToolbar").setAttribute("checked", "true");

      this.target = TargetFactory.forTab(this._chromeWindow.gBrowser.selectedTab);
      const options = {
        environment: CommandUtils.createEnvironment(this, "target"),
        document: this.outputPanel.document,
      };
      return CommandUtils.createRequisition(this.target, options).then(requisition => {
        this.requisition = requisition;

        // The <textbox> `value` may still be undefined on the XUL binding if
        // we fetch it early
        let value = this._input.value || "";
        return this.requisition.update(value).then(() => {
          const Inputter = require("gcli/mozui/inputter").Inputter;
          const Completer = require("gcli/mozui/completer").Completer;
          const Tooltip = require("gcli/mozui/tooltip").Tooltip;
          const FocusManager = require("gcli/ui/focus").FocusManager;

          this.onOutput = this.requisition.commandOutputManager.onOutput;

          this.focusManager = new FocusManager(this._doc, requisition.system.settings);

          this.inputter = new Inputter({
            requisition: this.requisition,
            focusManager: this.focusManager,
            element: this._input,
          });

          this.completer = new Completer({
            requisition: this.requisition,
            inputter: this.inputter,
            backgroundElement: this._doc.querySelector(".gclitoolbar-stack-node"),
            element: this._doc.querySelector(".gclitoolbar-complete-node"),
          });

          this.tooltip = new Tooltip({
            requisition: this.requisition,
            focusManager: this.focusManager,
            inputter: this.inputter,
            element: this.tooltipPanel.hintElement,
          });

          this.inputter.tooltip = this.tooltip;

          this.focusManager.addMonitoredElement(this.outputPanel._frame);
          this.focusManager.addMonitoredElement(this._element);

          this.focusManager.onVisibilityChange.add(this.outputPanel._visibilityChanged,
                                                   this.outputPanel);
          this.focusManager.onVisibilityChange.add(this.tooltipPanel._visibilityChanged,
                                                   this.tooltipPanel);
          this.onOutput.add(this.outputPanel._outputChanged, this.outputPanel);

          let tabbrowser = this._chromeWindow.gBrowser;
          tabbrowser.tabContainer.addEventListener("TabSelect", this, false);
          tabbrowser.tabContainer.addEventListener("TabClose", this, false);
          tabbrowser.addEventListener("load", this, true);
          tabbrowser.addEventListener("beforeunload", this, true);

          gDevTools.on("toolbox-ready", this._onToolboxReady);
          gDevTools.on("toolbox-destroyed", this._onToolboxDestroyed);

          this._initErrorsCount(tabbrowser.selectedTab);

          this._element.hidden = false;

          if (focus) {
            // If the toolbar was just inserted, the <textbox> may still have
            // its binding in process of being applied and not be focusable yet
            let waitForBinding = () => {
              // Bail out if the toolbar has been destroyed in the meantime
              if (!this._input) {
                return;
              }
              // mInputField is a xbl field of <xul:textbox>
              if (typeof this._input.mInputField != "undefined") {
                this._input.focus();
                this._notify(NOTIFICATIONS.SHOW);
              } else {
                this._input.ownerDocument.defaultView.setTimeout(waitForBinding, 50);
              }
            };
            waitForBinding();
          } else {
            this._notify(NOTIFICATIONS.SHOW);
          }

          if (!DeveloperToolbar.introShownThisSession) {
            let intro = require("gcli/ui/intro");
            intro.maybeShowIntro(this.requisition.commandOutputManager,
                                 this.requisition.conversionContext,
                                 this.outputPanel);
            DeveloperToolbar.introShownThisSession = true;
          }

          this._showPromise = null;
        });
      });
    });
  });

  return this._showPromise;
};

/**
 * Hide the developer toolbar.
 */
DeveloperToolbar.prototype.hide = function () {
  // If we're already in the process of hiding, just use the other promise
  if (this._hidePromise != null) {
    return this._hidePromise;
  }

  // show() is async, so ensure we don't need to wait for show() to finish
  var waitPromise = this._showPromise || promise.resolve();

  this._hidePromise = waitPromise.then(() => {
    this._element.hidden = true;

    Services.prefs.setBoolPref("devtools.toolbar.visible", false);

    this._doc.getElementById("menu_devToolbar").setAttribute("checked", "false");
    this.destroy();

    this._telemetry.toolClosed("developertoolbar");
    this._notify(NOTIFICATIONS.HIDE);

    this._hidePromise = null;
  });

  return this._hidePromise;
};

/**
 * Initialize the listeners needed for tracking the number of errors for a given
 * tab.
 *
 * @private
 * @param nsIDOMNode tab the xul:tab for which you want to track the number of
 * errors.
 */
DeveloperToolbar.prototype._initErrorsCount = function (tab) {
  let tabId = tab.linkedPanel;
  if (tabId in this._errorsCount) {
    this._updateErrorsCount();
    return;
  }

  let window = tab.linkedBrowser.contentWindow;
  let listener = new ConsoleServiceListener(window, {
    onConsoleServiceMessage: this._onPageError.bind(this, tabId),
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
 * @param nsIDOMNode tab the xul:tab for which you want to stop tracking the
 * number of errors.
 */
DeveloperToolbar.prototype._stopErrorsCount = function (tab) {
  let tabId = tab.linkedPanel;
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
 * Hide the developer toolbar
 */
DeveloperToolbar.prototype.destroy = function () {
  if (this._input == null) {
    return; // Already destroyed
  }

  let tabbrowser = this._chromeWindow.gBrowser;
  tabbrowser.tabContainer.removeEventListener("TabSelect", this, false);
  tabbrowser.tabContainer.removeEventListener("TabClose", this, false);
  tabbrowser.removeEventListener("load", this, true);
  tabbrowser.removeEventListener("beforeunload", this, true);

  gDevTools.off("toolbox-ready", this._onToolboxReady);
  gDevTools.off("toolbox-destroyed", this._onToolboxDestroyed);

  Array.prototype.forEach.call(tabbrowser.tabs, this._stopErrorsCount, this);

  this.focusManager.removeMonitoredElement(this.outputPanel._frame);
  this.focusManager.removeMonitoredElement(this._element);

  this.focusManager.onVisibilityChange.remove(this.outputPanel._visibilityChanged,
                                              this.outputPanel);
  this.focusManager.onVisibilityChange.remove(this.tooltipPanel._visibilityChanged,
                                              this.tooltipPanel);
  this.onOutput.remove(this.outputPanel._outputChanged, this.outputPanel);

  this.tooltip.destroy();
  this.completer.destroy();
  this.inputter.destroy();
  this.focusManager.destroy();

  this.outputPanel.destroy();
  this.tooltipPanel.destroy();
  delete this._input;

  CommandUtils.destroyRequisition(this.requisition, this.target);
  this.target = undefined;

  this._element.remove();
  delete this._element;
};

/**
 * Utility for sending notifications
 * @param topic a NOTIFICATION constant
 */
DeveloperToolbar.prototype._notify = function (topic) {
  let data = { toolbar: this };
  data.wrappedJSObject = data;
  Services.obs.notifyObservers(data, topic, null);
};

/**
 * Update various parts of the UI when the current tab changes
 */
DeveloperToolbar.prototype.handleEvent = function (ev) {
  if (ev.type == "TabSelect" || ev.type == "load") {
    if (this.visible) {
      let tab = this._chromeWindow.gBrowser.selectedTab;
      this.target = TargetFactory.forTab(tab);
      gcliInit.getSystem(this.target).then(system => {
        this.requisition.system = system;
      }, error => {
        if (!this._chromeWindow.gBrowser.getBrowserForTab(tab)) {
          // The tab was closed, suppress the error and print a warning as the
          // destroyed tab was likely the cause.
          console.warn("An error occurred as the tab was closed while " +
            "updating Developer Toolbar state. The error was: ", error);
          return;
        }

        // Propagate other errors as they're more likely to cause real issues
        // and thus should cause tests to fail.
        throw error;
      });

      if (ev.type == "TabSelect") {
        this._initErrorsCount(ev.target);
      }
    }
  }
  else if (ev.type == "TabClose") {
    this._stopErrorsCount(ev.target);
  }
  else if (ev.type == "beforeunload") {
    this._onPageBeforeUnload(ev);
  }
};

/**
 * Update toolbox toggle button when toolbox goes on and off
 */
DeveloperToolbar.prototype._onToolboxReady = function () {
  this._errorCounterButton.setAttribute("checked", "true");
};
DeveloperToolbar.prototype._onToolboxDestroyed = function () {
  this._errorCounterButton.setAttribute("checked", "false");
};

/**
 * Count a page error received for the currently selected tab. This
 * method counts the JavaScript exceptions received and CSS errors/warnings.
 *
 * @private
 * @param string tabId the ID of the tab from where the page error comes.
 * @param object pageError the page error object received from the
 * PageErrorListener.
 */
DeveloperToolbar.prototype._onPageError = function (tabId, pageError) {
  if (pageError.category == "CSS Parser" ||
      pageError.category == "CSS Loader") {
    return;
  }
  if ((pageError.flags & pageError.warningFlag) ||
      (pageError.flags & pageError.strictFlag)) {
    this._warningsCount[tabId]++;
  } else {
    this._errorsCount[tabId]++;
  }
  this._updateErrorsCount(tabId);
};

/**
 * The |beforeunload| event handler. This function resets the errors count when
 * a different page starts loading.
 *
 * @private
 * @param nsIDOMEvent ev the beforeunload DOM event.
 */
DeveloperToolbar.prototype._onPageBeforeUnload = function (ev) {
  let window = ev.target.defaultView;
  if (window.top !== window) {
    return;
  }

  let tabs = this._chromeWindow.gBrowser.tabs;
  Array.prototype.some.call(tabs, function (tab) {
    if (tab.linkedBrowser.contentWindow === window) {
      let tabId = tab.linkedPanel;
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
 * @param string [changedTabId] Optional. The tab ID that had its page errors
 * count changed. If this is provided and it doesn't match the currently
 * selected tab, then the button is not updated.
 */
DeveloperToolbar.prototype._updateErrorsCount = function (changedTabId) {
  let tabId = this._chromeWindow.gBrowser.selectedTab.linkedPanel;
  if (changedTabId && tabId != changedTabId) {
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

  this.emit("errors-counter-updated");
};

/**
 * Reset the errors counter for the given tab.
 *
 * @param nsIDOMElement tab The xul:tab for which you want to reset the page
 * errors counters.
 */
DeveloperToolbar.prototype.resetErrorsCount = function (tab) {
  let tabId = tab.linkedPanel;
  if (tabId in this._errorsCount || tabId in this._warningsCount) {
    this._errorsCount[tabId] = 0;
    this._warningsCount[tabId] = 0;
    this._updateErrorsCount(tabId);
  }
};

/**
 * Creating a OutputPanel is asynchronous
 */
function OutputPanel() {
  throw new Error("Use OutputPanel.create()");
}

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
 * @param devtoolbar The parent DeveloperToolbar object
 */
OutputPanel.create = function (devtoolbar) {
  var outputPanel = Object.create(OutputPanel.prototype);
  return outputPanel._init(devtoolbar);
};

/**
 * @private See OutputPanel.create
 */
OutputPanel.prototype._init = function (devtoolbar) {
  this._devtoolbar = devtoolbar;
  this._input = this._devtoolbar._input;
  this._toolbar = this._devtoolbar._doc.getElementById("developer-toolbar");

  /*
  <tooltip|panel id="gcli-output"
         noautofocus="true"
         noautohide="true"
         class="gcli-panel">
    <html:iframe xmlns:html="http://www.w3.org/1999/xhtml"
                 id="gcli-output-frame"
                 src="chrome://devtools/content/commandline/commandlineoutput.xhtml"
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
  this._frame.setAttribute("src", "chrome://devtools/content/commandline/commandlineoutput.xhtml");
  this._frame.setAttribute("sandbox", "allow-same-origin");
  this._panel.appendChild(this._frame);

  this.displayedOutput = undefined;

  this._update = this._update.bind(this);

  // Wire up the element from the iframe, and resolve the promise
  let deferred = defer();
  let onload = () => {
    this._frame.removeEventListener("load", onload, true);

    this.document = this._frame.contentDocument;
    this._copyTheme();

    this._div = this.document.getElementById("gcli-output-root");
    this._div.classList.add("gcli-row-out");
    this._div.setAttribute("aria-live", "assertive");

    let styles = this._toolbar.ownerDocument.defaultView
                    .getComputedStyle(this._toolbar);
    this._div.setAttribute("dir", styles.direction);

    deferred.resolve(this);
  };
  this._frame.addEventListener("load", onload, true);

  return deferred.promise;
};

/* Copy the current devtools theme attribute into the iframe,
   so it can be styled correctly. */
OutputPanel.prototype._copyTheme = function () {
  if (this.document) {
    let theme =
      this._devtoolbar._doc.documentElement.getAttribute("devtoolstheme");
    this.document.documentElement.setAttribute("devtoolstheme", theme);
  }
};

/**
 * Prevent the popup from hiding if it is not permitted via this.canHide.
 */
OutputPanel.prototype._onpopuphiding = function (ev) {
  // TODO: When we switch back from tooltip to panel we can remove this hack:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=780102
  if (isLinux && !this.canHide) {
    ev.preventDefault();
  }
};

/**
 * Display the OutputPanel.
 */
OutputPanel.prototype.show = function () {
  if (isLinux) {
    this.canHide = false;
  }

  // We need to reset the iframe size in order for future size calculations to
  // be correct
  this._frame.style.minHeight = this._frame.style.maxHeight = 0;
  this._frame.style.minWidth = 0;

  this._copyTheme();
  this._panel.openPopup(this._input, "before_start", 0, 0, false, false, null);
  this._resize();

  this._input.focus();
};

/**
 * Internal helper to set the height of the output panel to fit the available
 * content;
 */
OutputPanel.prototype._resize = function () {
  if (this._panel == null || this.document == null || !this._panel.state == "closed") {
    return;
  }

  // Set max panel width to match any content with a max of the width of the
  // browser window.
  let maxWidth = this._panel.ownerDocument.documentElement.clientWidth;

  // Adjust max width according to OS.
  // We'd like to put this in CSS but we can't:
  //   body { width: calc(min(-5px, max-content)); }
  //   #_panel { max-width: -5px; }
  switch (Services.appinfo.OS) {
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
OutputPanel.prototype._outputChanged = function (ev) {
  if (ev.output.hidden) {
    return;
  }

  this.remove();

  this.displayedOutput = ev.output;

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
OutputPanel.prototype._update = function () {
  // destroy has been called, bail out
  if (this._div == null) {
    return;
  }

  // Empty this._div
  while (this._div.hasChildNodes()) {
    this._div.removeChild(this._div.firstChild);
  }

  if (this.displayedOutput.data != null) {
    let context = this._devtoolbar.requisition.conversionContext;
    this.displayedOutput.convert("dom", context).then(node => {
      if (node == null) {
        return;
      }

      while (this._div.hasChildNodes()) {
        this._div.removeChild(this._div.firstChild);
      }

      var links = node.querySelectorAll("*[href]");
      for (var i = 0; i < links.length; i++) {
        links[i].setAttribute("target", "_blank");
      }

      this._div.appendChild(node);
      this.show();
    });
  }
};

/**
 * Detach listeners from the currently displayed Output.
 */
OutputPanel.prototype.remove = function () {
  if (isLinux) {
    this.canHide = true;
  }

  if (this._panel && this._panel.hidePopup) {
    this._panel.hidePopup();
  }

  if (this.displayedOutput) {
    delete this.displayedOutput;
  }
};

/**
 * Detach listeners from the currently displayed Output.
 */
OutputPanel.prototype.destroy = function () {
  this.remove();

  this._panel.removeEventListener("popuphiding", this._onpopuphiding, true);

  this._panel.removeChild(this._frame);
  this._toolbar.parentElement.removeChild(this._panel);

  delete this._devtoolbar;
  delete this._input;
  delete this._toolbar;
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
OutputPanel.prototype._visibilityChanged = function (ev) {
  if (ev.outputVisible === true) {
    // this.show is called by _outputChanged
  } else {
    if (isLinux) {
      this.canHide = true;
    }
    this._panel.hidePopup();
  }
};

/**
 * Creating a TooltipPanel is asynchronous
 */
function TooltipPanel() {
  throw new Error("Use TooltipPanel.create()");
}

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
 * @param devtoolbar The parent DeveloperToolbar object
 */
TooltipPanel.create = function (devtoolbar) {
  var tooltipPanel = Object.create(TooltipPanel.prototype);
  return tooltipPanel._init(devtoolbar);
};

/**
 * @private See TooltipPanel.create
 */
TooltipPanel.prototype._init = function (devtoolbar) {
  let deferred = defer();

  let chromeDocument = devtoolbar._doc;
  this._devtoolbar = devtoolbar;
  this._input = devtoolbar._doc.querySelector(".gclitoolbar-input-node");
  this._toolbar = devtoolbar._doc.querySelector("#developer-toolbar");
  this._dimensions = { start: 0, end: 0 };

  /*
  <tooltip|panel id="gcli-tooltip"
         type="arrow"
         noautofocus="true"
         noautohide="true"
         class="gcli-panel">
    <html:iframe xmlns:html="http://www.w3.org/1999/xhtml"
                 id="gcli-tooltip-frame"
                 src="chrome://devtools/content/commandline/commandlinetooltip.xhtml"
                 flex="1"
                 sandbox="allow-same-origin"/>
  </tooltip|panel>
  */

  // TODO: Switch back from tooltip to panel when metacity focus issue is fixed:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=780102
  this._panel = devtoolbar._doc.createElement(isLinux ? "tooltip" : "panel");

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

  this._frame = devtoolbar._doc.createElementNS(NS_XHTML, "iframe");
  this._frame.id = "gcli-tooltip-frame";
  this._frame.setAttribute("src", "chrome://devtools/content/commandline/commandlinetooltip.xhtml");
  this._frame.setAttribute("flex", "1");
  this._frame.setAttribute("sandbox", "allow-same-origin");
  this._panel.appendChild(this._frame);

  /**
   * Wire up the element from the iframe, and resolve the promise.
   */
  let onload = () => {
    this._frame.removeEventListener("load", onload, true);

    this.document = this._frame.contentDocument;
    this._copyTheme();
    this.hintElement = this.document.getElementById("gcli-tooltip-root");
    this._connector = this.document.getElementById("gcli-tooltip-connector");

    let styles = this._toolbar.ownerDocument.defaultView
                    .getComputedStyle(this._toolbar);
    this.hintElement.setAttribute("dir", styles.direction);

    deferred.resolve(this);
  };
  this._frame.addEventListener("load", onload, true);

  return deferred.promise;
};

/* Copy the current devtools theme attribute into the iframe,
   so it can be styled correctly. */
TooltipPanel.prototype._copyTheme = function () {
  if (this.document) {
    let theme =
      this._devtoolbar._doc.documentElement.getAttribute("devtoolstheme");
    this.document.documentElement.setAttribute("devtoolstheme", theme);
  }
};

/**
 * Prevent the popup from hiding if it is not permitted via this.canHide.
 */
TooltipPanel.prototype._onpopuphiding = function (ev) {
  // TODO: When we switch back from tooltip to panel we can remove this hack:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=780102
  if (isLinux && !this.canHide) {
    ev.preventDefault();
  }
};

/**
 * Display the TooltipPanel.
 */
TooltipPanel.prototype.show = function (dimensions) {
  if (!dimensions) {
    dimensions = { start: 0, end: 0 };
  }
  this._dimensions = dimensions;

  // This is nasty, but displaying the panel causes it to re-flow, which can
  // change the size it should be, so we need to resize the iframe after the
  // panel has displayed
  this._panel.ownerDocument.defaultView.setTimeout(() => {
    this._resize();
  }, 0);

  if (isLinux) {
    this.canHide = false;
  }

  this._copyTheme();
  this._resize();
  this._panel.openPopup(this._input, "before_start", dimensions.start * 10, 0,
                        false, false, null);
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
TooltipPanel.prototype._resize = function () {
  if (this._panel == null || this.document == null || !this._panel.state == "closed") {
    return;
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
TooltipPanel.prototype.remove = function () {
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
TooltipPanel.prototype.destroy = function () {
  this.remove();

  this._panel.removeEventListener("popuphiding", this._onpopuphiding, true);

  this._panel.removeChild(this._frame);
  this._toolbar.parentElement.removeChild(this._panel);

  delete this._connector;
  delete this._dimensions;
  delete this._input;
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
TooltipPanel.prototype._visibilityChanged = function (ev) {
  if (ev.tooltipVisible === true) {
    this.show(ev.dimensions);
  } else {
    if (isLinux) {
      this.canHide = true;
    }
    this._panel.hidePopup();
  }
};
