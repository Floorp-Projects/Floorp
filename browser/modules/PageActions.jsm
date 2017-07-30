/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "PageActions",
  // PageActions.Action
  // PageActions.Button
  // PageActions.Subview
  // PageActions.ACTION_ID_BOOKMARK_SEPARATOR
];

const { utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "BinarySearch",
  "resource://gre/modules/BinarySearch.jsm");


const ACTION_ID_BOOKMARK_SEPARATOR = "bookmarkSeparator";
const ACTION_ID_BUILT_IN_SEPARATOR = "builtInSeparator";

const PREF_ACTION_IDS_IN_URLBAR = "browser.pageActions.actionIDsInUrlbar";


this.PageActions = {
  /**
   * Inits.  Call to init.
   */
  init() {
    let callbacks = this._deferredAddActionCalls;
    delete this._deferredAddActionCalls;

    let actionIDsInUrlbar = this._loadActionIDsInUrlbar();
    this._actionIDsInUrlbar = actionIDsInUrlbar || [];

    // Add the built-in actions, which are defined below in this file.
    for (let options of gBuiltInActions) {
      if (options._isSeparator || !this.actionForID(options.id)) {
        this.addAction(new Action(options));
      }
    }

    // These callbacks are deferred until init happens and all built-in actions
    // are added.
    while (callbacks && callbacks.length) {
      callbacks.shift()();
    }

    if (!actionIDsInUrlbar) {
      // The action IDs in the urlbar haven't been stored yet.  Compute them and
      // store them now.  From now on, these stored IDs will determine the
      // actions that are shown in the urlbar and the order they're shown in.
      this._actionIDsInUrlbar =
        this.actions.filter(a => a.shownInUrlbar).map(a => a.id);
      this._storeActionIDsInUrlbar();
    }
  },

  _deferredAddActionCalls: [],

  /**
   * The list of Action objects, sorted in the order in which they should be
   * placed in the page action panel.  Not live.  (array of Action objects)
   */
  get actions() {
    let actions = this._builtInActions.slice();
    if (this._nonBuiltInActions.length) {
      // There are non-built-in actions, so include them too.  Add a separator
      // between the built-ins and non-built-ins so that the returned array
      // looks like: [...built-ins, separator, ...non-built-ins]
      actions.push(new Action({
        id: ACTION_ID_BUILT_IN_SEPARATOR,
        _isSeparator: true,
      }));
      actions.push(...this._nonBuiltInActions);
    }
    return actions;
  },

  /**
   * Gets an action.
   *
   * @param  id (string, required)
   *         The ID of the action to get.
   * @return The Action object, or null if none.
   */
  actionForID(id) {
    return this._actionsByID.get(id);
  },

  /**
   * Registers an action.
   *
   * Actions are registered by their IDs.  An error is thrown if an action with
   * the given ID has already been added.  Use actionForID() before calling this
   * method if necessary.
   *
   * Be sure to call remove() on the action if the lifetime of the code that
   * owns it is shorter than the browser's -- if it lives in an extension, for
   * example.
   *
   * @param  action (Action, required)
   *         The Action object to register.
   * @return The given Action.
   */
  addAction(action) {
    if (this._deferredAddActionCalls) {
      // init() hasn't been called yet.  Defer all additions until it's called,
      // at which time _deferredAddActionCalls will be deleted.
      this._deferredAddActionCalls.push(() => this.addAction(action));
      return action;
    }

    // The IDs of the actions in the panel and urlbar before which the new
    // action shoud be inserted.  null means at the end, or it's irrelevant.
    let panelInsertBeforeID = null;
    let urlbarInsertBeforeID = null;

    let placeBuiltInSeparator = false;

    if (action.__isSeparator) {
      this._builtInActions.push(action);
    } else {
      if (this.actionForID(action.id)) {
        throw new Error(`An Action with ID '${action.id}' has already been added.`);
      }
      this._actionsByID.set(action.id, action);

      // Insert the action into the appropriate list, either _builtInActions or
      // _nonBuiltInActions, and find panelInsertBeforeID.

      // Keep in mind that _insertBeforeActionID may be present but null, which
      // means the action should be appended to the built-ins.
      if ("__insertBeforeActionID" in action) {
        // A "semi-built-in" action, probably an action from an extension
        // bundled with the browser.  Right now we simply assume that no other
        // consumers will use _insertBeforeActionID.
        let index =
          !action.__insertBeforeActionID ? -1 :
          this._builtInActions.findIndex(a => {
            return a.id == action.__insertBeforeActionID;
          });
        if (index < 0) {
          // Append the action.
          index = this._builtInActions.length;
          if (this._nonBuiltInActions.length) {
            panelInsertBeforeID = ACTION_ID_BUILT_IN_SEPARATOR;
          }
        } else {
          panelInsertBeforeID = this._builtInActions[index].id;
        }
        this._builtInActions.splice(index, 0, action);
      } else if (gBuiltInActions.find(a => a.id == action.id)) {
        // A built-in action.  These are always added on init before all other
        // actions, one after the other, so just push onto the array.
        this._builtInActions.push(action);
        if (this._nonBuiltInActions.length) {
          panelInsertBeforeID = ACTION_ID_BUILT_IN_SEPARATOR;
        }
      } else {
        // A non-built-in action, like a non-bundled extension potentially.
        // Keep this list sorted by title.
        let index = BinarySearch.insertionIndexOf((a1, a2) => {
          return a1.title.localeCompare(a2.title);
        }, this._nonBuiltInActions, action);
        if (index < this._nonBuiltInActions.length) {
          panelInsertBeforeID = this._nonBuiltInActions[index].id;
        }
        // If this is the first non-built-in, then the built-in separator must
        // be placed between the built-ins and non-built-ins.
        if (!this._nonBuiltInActions.length) {
          placeBuiltInSeparator = true;
        }
        this._nonBuiltInActions.splice(index, 0, action);
      }

      if (this._actionIDsInUrlbar.includes(action.id)) {
        // The action should be shown in the urlbar.  Set its shownInUrlbar to
        // true, but set the private version so that onActionToggledShownInUrlbar
        // isn't called, which happens when the public version is set.
        action._shownInUrlbar = true;
        urlbarInsertBeforeID = this.insertBeforeActionIDInUrlbar(action);
      }
    }

    for (let win of browserWindows()) {
      if (placeBuiltInSeparator) {
        let sep = new Action({
          id: ACTION_ID_BUILT_IN_SEPARATOR,
          _isSeparator: true,
        });
        browserPageActions(win).placeAction(sep, null, null);
      }
      browserPageActions(win).placeAction(action, panelInsertBeforeID,
                                          urlbarInsertBeforeID);
    }

    return action;
  },

  _builtInActions: [],
  _nonBuiltInActions: [],
  _actionsByID: new Map(),

  /**
   * Returns the ID of the action among the current registered actions in the
   * urlbar before which the given action should be inserted, ignoring whether
   * the given action's shownInUrlbar is true or false.
   *
   * @return The ID of the action before which the given action should be
   *         inserted.  If the given action should be inserted last or it should
   *         not be inserted at all, returns null.
   */
  insertBeforeActionIDInUrlbar(action) {
    // First, find the index of the given action.
    let index = this._actionIDsInUrlbar.findIndex(a => a.id == action.id);
    if (index < 0) {
      return null;
    }
    // Now start at the next index and find the ID of the first action that's
    // currently registered.  Remember that IDs in _actionIDsInUrlbar may belong
    // to actions that aren't currently registered.
    for (let i = index + 1; i < this._actionIDsInUrlbar.length; i++) {
      let id = this._actionIDsInUrlbar[i];
      if (this.actionForID(id)) {
        return id;
      }
    }
    return null;
  },

  /**
   * Call this when an action is removed.
   *
   * @param  action (Action object, required)
   *         The action that was removed.
   */
  onActionRemoved(action) {
    if (!this.actionForID(action.id)) {
      // The action isn't present.  Not an error.
      return;
    }
    this._actionsByID.delete(action.id);
    for (let list of [this._nonBuiltInActions, this._builtInActions]) {
      let index = list.findIndex(a => a.id == action.id);
      if (index >= 0) {
        list.splice(index, 1);
        break;
      }
    }
    this._updateActionIDsInUrlbar(action.id, false);
    for (let win of browserWindows()) {
      browserPageActions(win).removeAction(action);
    }
  },

  /**
   * Call this when an action's iconURL changes.
   *
   * @param  action (Action object, required)
   *         The action whose iconURL property changed.
   */
  onActionSetIconURL(action) {
    if (!this.actionForID(action.id)) {
      // This may be called before the action has been added.
      return;
    }
    for (let win of browserWindows()) {
      browserPageActions(win).updateActionIconURL(action);
    }
  },

  /**
   * Call this when an action's title changes.
   *
   * @param  action (Action object, required)
   *         The action whose title property changed.
   */
  onActionSetTitle(action) {
    if (!this.actionForID(action.id)) {
      // This may be called before the action has been added.
      return;
    }
    for (let win of browserWindows()) {
      browserPageActions(win).updateActionTitle(action);
    }
  },

  /**
   * Call this when an action's shownInUrlbar property changes.
   *
   * @param  action (Action object, required)
   *         The action whose shownInUrlbar property changed.
   */
  onActionToggledShownInUrlbar(action) {
    if (!this.actionForID(action.id)) {
      // This may be called before the action has been added.
      return;
    }
    this._updateActionIDsInUrlbar(action.id, action.shownInUrlbar);
    let insertBeforeID = this.insertBeforeActionIDInUrlbar(action);
    for (let win of browserWindows()) {
      browserPageActions(win).placeActionInUrlbar(action, insertBeforeID);
    }
  },

  /**
   * Adds or removes the given action ID to or from _actionIDsInUrlbar.
   *
   * @param  actionID (string, required)
   *         The action ID to add or remove.
   * @param  shownInUrlbar (bool, required)
   *         If true, the ID is added if it's not already present.  If false,
   *         the ID is removed if it's not already absent.
   */
  _updateActionIDsInUrlbar(actionID, shownInUrlbar) {
    let index = this._actionIDsInUrlbar.indexOf(actionID);
    if (shownInUrlbar) {
      if (index < 0) {
        this._actionIDsInUrlbar.push(actionID);
      }
    } else if (index >= 0) {
      this._actionIDsInUrlbar.splice(index, 1);
    }
    this._storeActionIDsInUrlbar();
  },

  _storeActionIDsInUrlbar() {
    let json = JSON.stringify(this._actionIDsInUrlbar);
    Services.prefs.setStringPref(PREF_ACTION_IDS_IN_URLBAR, json);
  },

  _loadActionIDsInUrlbar() {
    try {
      let json = Services.prefs.getStringPref(PREF_ACTION_IDS_IN_URLBAR);
      let obj = JSON.parse(json);
      if (Array.isArray(obj)) {
        return obj;
      }
    } catch (ex) {}
    return null;
  },

  _actionIDsInUrlbar: []
};

/**
 * A single page action.
 *
 * @param  options (object, required)
 *         An object with the following properties:
 *         @param id (string, required)
 *                The action's ID.  Treat this like the ID of a DOM node.
 *         @param title (string, required)
 *                The action's title.
 *         @param iconURL (string, optional)
 *                The URL of the action's icon.  Usually you want to specify an
 *                icon in CSS, but this option is useful if that would be a pain
 *                for some reason -- like your code is in an embedded
 *                WebExtension.
 *         @param nodeAttributes (object, optional)
 *                An object of name-value pairs.  Each pair will be added as
 *                an attribute to DOM nodes created for this action.
 *         @param onCommand (function, optional)
 *                Called when the action is clicked, but only if it has neither
 *                a subview nor an iframe.  Passed the following arguments:
 *                * event: The triggering event.
 *                * buttonNode: The button node that was clicked.
 *         @param onIframeShown (function, optional)
 *                Called when the action's iframe is shown to the user.  Passed
 *                the following arguments:
 *                * iframeNode: The iframe.
 *                * parentPanelNode: The panel node in which the iframe is
 *                  shown.
 *         @param onPlacedInPanel (function, optional)
 *                Called when the action is added to the page action panel in
 *                a browser window.  Passed the following arguments:
 *                * buttonNode: The action's node in the page action panel.
 *         @param onPlacedInUrlbar (function, optional)
 *                Called when the action is added to the urlbar in a browser
 *                window.  Passed the following arguments:
 *                * buttonNode: The action's node in the urlbar.
 *         @param onShowingInPanel (function, optional)
 *                Called when a browser window's page action panel is showing.
 *                Passed the following arguments:
 *                * buttonNode: The action's node in the page action panel.
 *         @param shownInUrlbar (bool, optional)
 *                Pass true to show the action in the urlbar, false otherwise.
 *                False by default.
 *         @param subview (object, optional)
 *                An options object suitable for passing to the Subview
 *                constructor, if you'd like the action to have a subview.  See
 *                the subview constructor for info on this object's properties.
 *         @param tooltip (string, optional)
 *                The action's button tooltip text.
 *         @param urlbarIDOverride (string, optional)
 *                Usually the ID of the action's button in the urlbar will be
 *                generated automatically.  Pass a string for this property to
 *                override that with your own ID.
 *         @param wantsIframe (bool, optional)
 *                Pass true to make an action that shows an iframe in a panel
 *                when clicked.
 */
function Action(options) {
  setProperties(this, options, {
    id: true,
    title: !options._isSeparator,
    iconURL: false,
    nodeAttributes: false,
    onCommand: false,
    onIframeShown: false,
    onPlacedInPanel: false,
    onPlacedInUrlbar: false,
    onShowingInPanel: false,
    shownInUrlbar: false,
    subview: false,
    tooltip: false,
    urlbarIDOverride: false,
    wantsIframe: false,

    // private

    // (string, optional)
    // The ID of another action before which to insert this new action.  Applies
    // to the page action panel only, not the urlbar.
    _insertBeforeActionID: false,

    // (bool, optional)
    // True if this isn't really an action but a separator to be shown in the
    // page action panel.
    _isSeparator: false,

    // (bool, optional)
    // True if the action's urlbar button is defined in markup.  In that case, a
    // node with the action's urlbar node ID should already exist in the DOM
    // (either the auto-generated ID or urlbarIDOverride).  That node will be
    // shown when the action is added to the urlbar and hidden when the action
    // is removed from the urlbar.
    _urlbarNodeInMarkup: false,
  });
  if (this._subview) {
    this._subview = new Subview(options.subview);
  }
}

Action.prototype = {
  /**
   * The action's icon URL (string, nullable)
   */
  get iconURL() {
    return this._iconURL;
  },
  set iconURL(url) {
    this._iconURL = url;
    PageActions.onActionSetIconURL(this);
    return this._iconURL;
  },

  /**
   * The action's ID (string, nonnull)
   */
  get id() {
    return this._id;
  },

  /**
   * Attribute name => value mapping to set on nodes created for this action
   * (object, nullable)
   */
  get nodeAttributes() {
    return this._nodeAttributes;
  },

  /**
   * True if the action is shown in the urlbar (bool, nonnull)
   */
  get shownInUrlbar() {
    return this._shownInUrlbar || false;
  },
  set shownInUrlbar(shown) {
    if (this.shownInUrlbar != shown) {
      this._shownInUrlbar = shown;
      PageActions.onActionToggledShownInUrlbar(this);
    }
    return this.shownInUrlbar;
  },

  /**
   * The action's title (string, nonnull)
   */
  get title() {
    return this._title;
  },
  set title(title) {
    this._title = title || "";
    PageActions.onActionSetTitle(this);
    return this._title;
  },

  /**
   * The action's tooltip (string, nullable)
   */
  get tooltip() {
    return this._tooltip;
  },

  /**
   * Override for the ID of the action's urlbar node (string, nullable)
   */
  get urlbarIDOverride() {
    return this._urlbarIDOverride;
  },

  /**
   * True if the action is shown in an iframe (bool, nonnull)
   */
  get wantsIframe() {
    return this._wantsIframe || false;
  },

  /**
   * A Subview object if the action wants a subview (Subview, nullable)
   */
  get subview() {
    return this._subview;
  },

  /**
   * Call this when the user activates the action.
   *
   * @param  event (DOM event, required)
   *         The triggering event.
   * @param  buttonNode (DOM node, required)
   *         The action's panel or urlbar button node that was clicked.
   */
  onCommand(event, buttonNode) {
    if (this._onCommand) {
      this._onCommand(event, buttonNode);
    }
  },

  /**
   * Call this when the action's iframe is shown.
   *
   * @param  iframeNode (DOM node, required)
   *         The iframe that's being shown.
   * @param  parentPanelNode (DOM node, required)
   *         The panel in which the iframe is shown.
   */
  onIframeShown(iframeNode, parentPanelNode) {
    if (this._onIframeShown) {
      this._onIframeShown(iframeNode, parentPanelNode);
    }
  },

  /**
   * Call this when a DOM node for the action is added to the page action panel.
   *
   * @param  buttonNode (DOM node, required)
   *         The action's panel button node.
   */
  onPlacedInPanel(buttonNode) {
    if (this._onPlacedInPanel) {
      this._onPlacedInPanel(buttonNode);
    }
  },

  /**
   * Call this when a DOM node for the action is added to the urlbar.
   *
   * @param  buttonNode (DOM node, required)
   *         The action's urlbar button node.
   */
  onPlacedInUrlbar(buttonNode) {
    if (this._onPlacedInUrlbar) {
      this._onPlacedInUrlbar(buttonNode);
    }
  },

  /**
   * Call this when the action's button is shown in the page action panel.
   *
   * @param  buttonNode (DOM node, required)
   *         The action's panel button node.
   */
  onShowingInPanel(buttonNode) {
    if (this._onShowingInPanel) {
      this._onShowingInPanel(buttonNode);
    }
  },

  /**
   * Unregisters the action and removes its DOM nodes from all browser windows.
   * Call this when your action lives in an extension and your extension is
   * unloaded, for example.
   */
  remove() {
    PageActions.onActionRemoved(this);
  }
};

this.PageActions.Action = Action;


/**
 * A Subview represents a PanelUI panelview that your actions can show.
 *
 * @param  options (object, required)
 *         An object with the following properties:
 *         @param buttons (array, optional)
 *                An array of buttons to show in the subview.  Each item in the
 *                array must be an options object suitable for passing to the
 *                Button constructor.  See the Button constructor for
 *                information on these objects' properties.
 *         @param onPlaced (function, optional)
 *                Called when the subview is added to its parent panel in a
 *                browser window.  Passed the following arguments:
 *                * panelViewNode: The panelview node represented by this
 *                  Subview.
 *         @param onShowing (function, optional)
 *                Called when the subview is showing in a browser window.
 *                Passed the following arguments:
 *                * panelViewNode: The panelview node represented by this
 *                  Subview.
 */
function Subview(options) {
  setProperties(this, options, {
    buttons: false,
    onPlaced: false,
    onShowing: false,
  });
  this._buttons = (this._buttons || []).map(buttonOptions => {
    return new Button(buttonOptions);
  });
}

Subview.prototype = {
  /**
   * The subview's buttons (array of Button objects, nonnull)
   */
  get buttons() {
    return this._buttons;
  },

  /**
   * Call this when a DOM node for the subview is added to the DOM.
   *
   * @param  panelViewNode (DOM node, required)
   *         The subview's panelview node.
   */
  onPlaced(panelViewNode) {
    if (this._onPlaced) {
      this._onPlaced(panelViewNode);
    }
  },

  /**
   * Call this when a DOM node for the subview is showing.
   *
   * @param  panelViewNode (DOM node, required)
   *         The subview's panelview node.
   */
  onShowing(panelViewNode) {
    if (this._onShowing) {
      this._onShowing(panelViewNode);
    }
  }
};

this.PageActions.Subview = Subview;


/**
 * A button that can be shown in a subview.
 *
 * @param  options (object, required)
 *         An object with the following properties:
 *         @param id (string, required)
 *                The button's ID.  This will not become the ID of a DOM node by
 *                itself, but it will be used to generate DOM node IDs.  But in
 *                terms of spaces and weird characters and such, do treat this
 *                like a DOM node ID.
 *         @param title (string, required)
 *                The button's title.
 *         @param disabled (bool, required)
 *                Pass true to disable the button.
 *         @param onCommand (function, optional)
 *                Called when the button is clicked.  Passed the following
 *                arguments:
 *                * event: The triggering event.
 *                * buttonNode: The node that was clicked.
 *         @param shortcut (string, optional)
 *                The button's shortcut text.
 */
function Button(options) {
  setProperties(this, options, {
    id: true,
    title: true,
    disabled: false,
    onCommand: false,
    shortcut: false,
  });
}

Button.prototype = {
  /**
   * True if the button is disabled (bool, nonnull)
   */
  get disabled() {
    return this._disabled || false;
  },

  /**
   * The button's ID (string, nonnull)
   */
  get id() {
    return this._id;
  },

  /**
   * The button's shortcut (string, nullable)
   */
  get shortcut() {
    return this._shortcut;
  },

  /**
   * The button's title (string, nonnull)
   */
  get title() {
    return this._title;
  },

  /**
   * Call this when the user clicks the button.
   *
   * @param  event (DOM event, required)
   *         The triggering event.
   * @param  buttonNode (DOM node, required)
   *         The button's DOM node that was clicked.
   */
  onCommand(event, buttonNode) {
    if (this._onCommand) {
      this._onCommand(event, buttonNode);
    }
  }
};

this.PageActions.Button = Button;


// This is only necessary so that Pocket can specify it for
// action._insertBeforeActionID.
this.PageActions.ACTION_ID_BOOKMARK_SEPARATOR = ACTION_ID_BOOKMARK_SEPARATOR;


// Sorted in the order in which they should appear in the page action panel.
// Does not include the page actions of extensions bundled with the browser.
// They're added by the relevant extension code.
var gBuiltInActions = [

  // bookmark
  {
    id: "bookmark",
    urlbarIDOverride: "star-button-box",
    _urlbarNodeInMarkup: true,
    title: "",
    shownInUrlbar: true,
    nodeAttributes: {
      observes: "bookmarkThisPageBroadcaster",
    },
    onShowingInPanel(buttonNode) {
      browserPageActions(buttonNode).bookmark.onShowingInPanel(buttonNode);
    },
    onCommand(event, buttonNode) {
      browserPageActions(buttonNode).bookmark.onCommand(event, buttonNode);
    },
  },

  // separator
  {
    id: ACTION_ID_BOOKMARK_SEPARATOR,
    _isSeparator: true,
  },

  // copy URL
  {
    id: "copyURL",
    title: "copyURL-title",
    onPlacedInPanel(buttonNode) {
      browserPageActions(buttonNode).copyURL.onPlacedInPanel(buttonNode);
    },
    onCommand(event, buttonNode) {
      browserPageActions(buttonNode).copyURL.onCommand(event, buttonNode);
    },
  },

  // email link
  {
    id: "emailLink",
    title: "emailLink-title",
    onPlacedInPanel(buttonNode) {
      browserPageActions(buttonNode).emailLink.onPlacedInPanel(buttonNode);
    },
    onCommand(event, buttonNode) {
      browserPageActions(buttonNode).emailLink.onCommand(event, buttonNode);
    },
  },

  // send to device
  {
    id: "sendToDevice",
    title: "sendToDevice-title",
    onPlacedInPanel(buttonNode) {
      browserPageActions(buttonNode).sendToDevice.onPlacedInPanel(buttonNode);
    },
    onShowingInPanel(buttonNode) {
      browserPageActions(buttonNode).sendToDevice.onShowingInPanel(buttonNode);
    },
    subview: {
      buttons: [
        {
          id: "notReady",
          title: "sendToDevice-notReadyTitle",
          disabled: true,
        },
      ],
      onPlaced(panelViewNode) {
        browserPageActions(panelViewNode).sendToDevice
          .onSubviewPlaced(panelViewNode);
      },
      onShowing(panelViewNode) {
        browserPageActions(panelViewNode).sendToDevice
          .onShowingSubview(panelViewNode);
      },
    },
  }
];


/**
 * Gets a BrowserPageActions object in a browser window.
 *
 * @param  obj
 *         Either a DOM node or a browser window.
 * @return The BrowserPageActions object in the browser window related to the
 *         given object.
 */
function browserPageActions(obj) {
  if (obj.BrowserPageActions) {
    return obj.BrowserPageActions;
  }
  return obj.ownerGlobal.BrowserPageActions;
}

/**
 * A generator function for all open browser windows.
 */
function* browserWindows() {
  let windows = Services.wm.getEnumerator("navigator:browser");
  while (windows.hasMoreElements()) {
    yield windows.getNext();
  }
}

/**
 * A simple function that sets properties on a given object while doing basic
 * required-properties checking.  If a required property isn't specified in the
 * given options object, or if the options object has properties that aren't in
 * the given schema, then an error is thrown.
 *
 * @param  obj
 *         The object to set properties on.
 * @param  options
 *         An options object supplied by the consumer.
 * @param  schema
 *         An object a property for each required and optional property.  The
 *         keys are property names; the value of a key is a bool that is true if
 *         the property is required.
 */
function setProperties(obj, options, schema) {
  for (let name in schema) {
    let required = schema[name];
    if (required && !(name in options)) {
      throw new Error(`'${name}' must be specified`);
    }
    let nameInObj = "_" + name;
    if (name[0] == "_") {
      // The property is "private".  If it's defined in the options, then define
      // it on obj exactly as it's defined on options.
      if (name in options) {
        obj[nameInObj] = options[name];
      }
    } else {
      // The property is "public".  Make sure the property is defined on obj.
      obj[nameInObj] = options[name] || null;
    }
  }
  for (let name in options) {
    if (!(name in schema)) {
      throw new Error(`Unrecognized option '${name}'`);
    }
  }
}
