/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Firefox Developer Toolbar.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com> (Original Author)
 *   Joe Walker <jwalker@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

"use strict";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

let EXPORTED_SYMBOLS = [ "DeveloperToolbar", "loadCommands" ];

XPCOMUtils.defineLazyGetter(this, "gcli", function () {
  let obj = {};
  Components.utils.import("resource:///modules/gcli.jsm", obj);
  return obj.gcli;
});

let console = gcli._internal.console;

/**
 * Load the various Command JSMs.
 * Should be called when the developer toolbar first opens.
 */
function loadCommands()
{
  Components.utils.import("resource:///modules/GcliCommands.jsm", {});
  Components.utils.import("resource:///modules/GcliTiltCommands.jsm", {});
}



let commandsLoaded = false;

/**
 * A component to manage the global developer toolbar, which contains a GCLI
 * and buttons for various developer tools.
 * @param aChromeWindow The browser window to which this toolbar is attached
 * @param aToolbarElement See browser.xul:<toolbar id="developer-toolbar">
 */
function DeveloperToolbar(aChromeWindow, aToolbarElement)
{
  if (!commandsLoaded) {
    loadCommands();
    commandsLoaded = true;
  }

  this._chromeWindow = aChromeWindow;

  this._element = aToolbarElement;
  this._element.hidden = true;
  this._doc = this._element.ownerDocument;

  this._command = this._doc.getElementById("Tools:DevToolbar");

  aChromeWindow.getBrowser().tabContainer.addEventListener("TabSelect", this, false);
}

/**
 * Inspector notifications dispatched through the nsIObserverService
 */
const NOTIFICATIONS = {
  /** DeveloperToolbar.show() has been called */
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
 */
DeveloperToolbar.prototype.show = function DT_show()
{
  this._command.setAttribute("checked", "true");

  this._input = this._doc.querySelector(".gclitoolbar-input-node");

  this.tooltipPanel = new TooltipPanel(this._doc, this._input);
  this.outputPanel = new OutputPanel(this._doc, this._input);

  let contentDocument = this._chromeWindow.getBrowser().contentDocument;

  this.display = gcli._internal.createDisplay({
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

  this._element.hidden = false;
  this._notify(NOTIFICATIONS.SHOW);

  if (!DeveloperToolbar.introShownThisSession) {
    this.display.maybeShowIntro();
    DeveloperToolbar.introShownThisSession = true;
  }
};

/**
 * Hide the developer toolbar
 */
DeveloperToolbar.prototype.hide = function DT_hide()
{
  this._command.setAttribute("checked", "false");

  this.display.onVisibilityChange.remove(this.outputPanel._visibilityChanged, this.outputPanel);
  this.display.onVisibilityChange.remove(this.tooltipPanel._visibilityChanged, this.tooltipPanel);
  this.display.onOutput.remove(this.outputPanel._outputChanged, this.outputPanel);
  this.display.destroy();

  // We could "delete this.display" etc if we have hard-to-track-down memory
  // leaks as a belt-and-braces approach, however this prevents our DOM node
  // hunter from looking in all the nooks and crannies, so it's better if we
  // can be leak-free without

  this.outputPanel.remove();
  delete this.outputPanel;

  this.tooltipPanel.remove();
  delete this.tooltipPanel;

  this._element.hidden = true;
  this._notify(NOTIFICATIONS.HIDE);
};

/**
 * Utility for sending notifications
 * @param aTopic a NOTIFICATION constant
 */
DeveloperToolbar.prototype._notify = function DT_notify(aTopic)
{
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
  if (aEvent.type == "TabSelect") {
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
 */
function OutputPanel(aChromeDoc, aInput)
{
  this._input = aInput;
  this._panel = aChromeDoc.getElementById("gcli-output");
  this._frame = aChromeDoc.getElementById("gcli-output-frame");
  this._anchor = aChromeDoc.getElementById("developer-toolbar");

  this._content = getContentBox(this._panel);
  this._content.classList.add("gcli-panel-inner-arrowcontent");

  this.document = this._frame.contentDocument;
  this.document.body.classList.add("gclichrome-output");

  this._div = this.document.querySelector("div");
  this._div.classList.add('gcli-row-out');
  this._div.setAttribute('aria-live', 'assertive');

  this.displayedOutput = undefined;
}

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
 */
function TooltipPanel(aChromeDoc, aInput)
{
  this._input = aInput;
  this._panel = aChromeDoc.getElementById("gcli-tooltip");
  this._frame = aChromeDoc.getElementById("gcli-tooltip-frame");
  this._anchor = aChromeDoc.getElementById("developer-toolbar");

  this._content = getContentBox(this._panel);
  this._content.classList.add("gcli-panel-inner-arrowcontent");

  this.document = this._frame.contentDocument;
  this.document.body.classList.add("gclichrome-tooltip");

  this.hintElement = this.document.querySelector("div");
}

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
