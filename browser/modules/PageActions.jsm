/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "PageActions",
  // PageActions.Action
  // PageActions.ACTION_ID_BOOKMARK
  // PageActions.ACTION_ID_PIN_TAB
  // PageActions.ACTION_ID_BOOKMARK_SEPARATOR
  // PageActions.ACTION_ID_BUILT_IN_SEPARATOR
  // PageActions.ACTION_ID_TRANSIENT_SEPARATOR
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  BinarySearch: "resource://gre/modules/BinarySearch.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.jsm",
});

const ACTION_ID_BOOKMARK = "bookmark";
const ACTION_ID_PIN_TAB = "pinTab";
const ACTION_ID_BOOKMARK_SEPARATOR = "bookmarkSeparator";
const ACTION_ID_BUILT_IN_SEPARATOR = "builtInSeparator";
const ACTION_ID_TRANSIENT_SEPARATOR = "transientSeparator";

const PREF_PERSISTED_ACTIONS = "browser.pageActions.persistedActions";
const PERSISTED_ACTIONS_CURRENT_VERSION = 1;

const PROTON_PREF = "browser.proton.urlbar.enabled";

// Escapes the given raw URL string, and returns an equivalent CSS url()
// value for it.
function escapeCSSURL(url) {
  return `url("${url.replace(/[\\\s"]/g, encodeURIComponent)}")`;
}

var PageActions = {
  /**
   * Initializes PageActions.
   *
   * @param {boolean} addShutdownBlocker
   *   This param exists only for tests.  Normally the default value of true
   *   must be used.
   */
  init(addShutdownBlocker = true) {
    this._initBuiltInActions();

    let callbacks = this._deferredAddActionCalls;
    delete this._deferredAddActionCalls;

    this._loadPersistedActions();

    // Register the built-in actions, which are defined below in this file.
    for (let options of gBuiltInActions) {
      if (!this.actionForID(options.id)) {
        this._registerAction(new Action(options));
      }
    }

    // Now place them all in each window.  Instead of splitting the register and
    // place steps, we could simply call addAction, which does both, but doing
    // it this way means that all windows initially place their actions in the
    // urlbar the same way -- placeAllActions -- regardless of whether they're
    // open when this method is called or opened later.
    for (let bpa of allBrowserPageActions()) {
      bpa.placeAllActionsInUrlbar();
    }

    // These callbacks are deferred until init happens and all built-in actions
    // are added.
    while (callbacks && callbacks.length) {
      callbacks.shift()();
    }

    if (addShutdownBlocker) {
      // Purge removed actions from persisted state on shutdown.  The point is
      // not to do it on Action.remove().  That way actions that are removed and
      // re-added while the app is running will have their urlbar placement and
      // other state remembered and restored.  This happens for upgraded and
      // downgraded extensions, for example.
      AsyncShutdown.profileBeforeChange.addBlocker(
        "PageActions: purging unregistered actions from cache",
        () => this._purgeUnregisteredPersistedActions()
      );
    }
  },

  _deferredAddActionCalls: [],

  /**
   * A list of all Action objects, not in any particular order.  Not live.
   * (array of Action objects)
   */
  get actions() {
    let lists = [
      this._builtInActions,
      this._nonBuiltInActions,
      this._transientActions,
    ];
    return lists.reduce((memo, list) => memo.concat(list), []);
  },

  /**
   * The list of Action objects that should appear in the panel for a given
   * window, sorted in the order in which they appear.  If there are both
   * built-in and non-built-in actions, then the list will include the separator
   * between the two.  The list is not live.  (array of Action objects)
   *
   * @param  browserWindow (DOM window, required)
   *         This window's actions will be returned.
   * @return (array of PageAction.Action objects) The actions currently in the
   *         given window's panel.
   */
  actionsInPanel(browserWindow) {
    function filter(action) {
      return action.shouldShowInPanel(browserWindow);
    }
    let actions = this._builtInActions.filter(filter);
    let nonBuiltInActions = this._nonBuiltInActions.filter(filter);
    if (nonBuiltInActions.length) {
      if (actions.length) {
        actions.push(
          new Action({
            id: ACTION_ID_BUILT_IN_SEPARATOR,
            _isSeparator: true,
          })
        );
      }
      actions.push(...nonBuiltInActions);
    }
    let transientActions = this._transientActions.filter(filter);
    if (transientActions.length) {
      if (actions.length) {
        actions.push(
          new Action({
            id: ACTION_ID_TRANSIENT_SEPARATOR,
            _isSeparator: true,
          })
        );
      }
      actions.push(...transientActions);
    }
    return actions;
  },

  /**
   * The list of actions currently in the urlbar, sorted in the order in which
   * they appear.  Not live.
   *
   * @param  browserWindow (DOM window, required)
   *         This window's actions will be returned.
   * @return (array of PageAction.Action objects) The actions currently in the
   *         given window's urlbar.
   */
  actionsInUrlbar(browserWindow) {
    // Remember that IDs in idsInUrlbar may belong to actions that aren't
    // currently registered.
    return this._persistedActions.idsInUrlbar.reduce((actions, id) => {
      let action = this.actionForID(id);
      if (action && action.shouldShowInUrlbar(browserWindow)) {
        actions.push(action);
      }
      return actions;
    }, []);
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
    this._registerAction(action);
    for (let bpa of allBrowserPageActions()) {
      bpa.placeAction(action);
    }
    return action;
  },

  _registerAction(action) {
    if (this.actionForID(action.id)) {
      throw new Error(`Action with ID '${action.id}' already added`);
    }
    this._actionsByID.set(action.id, action);

    // Insert the action into the appropriate list, either _builtInActions or
    // _nonBuiltInActions.

    // Keep in mind that _insertBeforeActionID may be present but null, which
    // means the action should be appended to the built-ins.
    if ("__insertBeforeActionID" in action) {
      // A "semi-built-in" action, probably an action from an extension
      // bundled with the browser.  Right now we simply assume that no other
      // consumers will use _insertBeforeActionID.
      let index = !action.__insertBeforeActionID
        ? -1
        : this._builtInActions.findIndex(a => {
            return a.id == action.__insertBeforeActionID;
          });
      if (index < 0) {
        // Append the action (excluding transient actions).
        index = this._builtInActions.filter(a => !a.__transient).length;
      }
      this._builtInActions.splice(index, 0, action);
    } else if (action.__transient) {
      // A transient action.
      this._transientActions.push(action);
    } else if (action._isBuiltIn) {
      // A built-in action. These are mostly added on init before all other
      // actions, one after the other. Extension actions load later and should
      // be at the end, so just push onto the array.
      this._builtInActions.push(action);
    } else {
      // A non-built-in action, like a non-bundled extension potentially.
      // Keep this list sorted by title.
      let index = BinarySearch.insertionIndexOf(
        (a1, a2) => {
          return a1.getTitle().localeCompare(a2.getTitle());
        },
        this._nonBuiltInActions,
        action
      );
      this._nonBuiltInActions.splice(index, 0, action);
    }

    let isNew = !this._persistedActions.ids.includes(action.id);
    if (isNew) {
      // The action is new.  Store it in the persisted actions.
      this._persistedActions.ids.push(action.id);
    }

    if (UrlbarPrefs.get(PROTON_PREF)) {
      // Actions are always pinned to the urlbar in Proton except for panel
      // separators.
      action._pinnedToUrlbar = !action.__isSeparator;
    } else if (!isNew) {
      // The action has been seen before.  Override its pinnedToUrlbar value
      // with the persisted value.  Set the private version of that property
      // so that onActionToggledPinnedToUrlbar isn't called, which happens when
      // the public version is set.
      action._pinnedToUrlbar = this._persistedActions.idsInUrlbar.includes(
        action.id
      );
    }
    this._updateIDsPinnedToUrlbarForAction(action);
  },

  _updateIDsPinnedToUrlbarForAction(action) {
    let index = this._persistedActions.idsInUrlbar.indexOf(action.id);
    if (action.pinnedToUrlbar) {
      if (index < 0) {
        index =
          action.id == ACTION_ID_BOOKMARK
            ? -1
            : this._persistedActions.idsInUrlbar.indexOf(ACTION_ID_BOOKMARK);
        if (index < 0) {
          index = this._persistedActions.idsInUrlbar.length;
        }
        this._persistedActions.idsInUrlbar.splice(index, 0, action.id);
      }
    } else if (index >= 0) {
      this._persistedActions.idsInUrlbar.splice(index, 1);
    }
    this._storePersistedActions();
  },

  // These keep track of currently registered actions.
  _builtInActions: [],
  _nonBuiltInActions: [],
  _transientActions: [],
  _actionsByID: new Map(),

  /**
   * Call this when an action is removed.
   *
   * @param  action (Action object, required)
   *         The action that was removed.
   */
  onActionRemoved(action) {
    if (!this.actionForID(action.id)) {
      // The action isn't registered (yet).  Not an error.
      return;
    }

    this._actionsByID.delete(action.id);
    let lists = [
      this._builtInActions,
      this._nonBuiltInActions,
      this._transientActions,
    ];
    for (let list of lists) {
      let index = list.findIndex(a => a.id == action.id);
      if (index >= 0) {
        list.splice(index, 1);
        break;
      }
    }

    for (let bpa of allBrowserPageActions()) {
      bpa.removeAction(action);
    }
  },

  /**
   * Call this when an action's pinnedToUrlbar property changes.
   *
   * @param  action (Action object, required)
   *         The action whose pinnedToUrlbar property changed.
   */
  onActionToggledPinnedToUrlbar(action) {
    if (!this.actionForID(action.id)) {
      // This may be called before the action has been added.
      return;
    }
    this._updateIDsPinnedToUrlbarForAction(action);
    for (let bpa of allBrowserPageActions()) {
      bpa.placeActionInUrlbar(action);
    }
  },

  // For tests.  See Bug 1413692.
  _reset() {
    PageActions._purgeUnregisteredPersistedActions();
    PageActions._builtInActions = [];
    PageActions._nonBuiltInActions = [];
    PageActions._transientActions = [];
    PageActions._actionsByID = new Map();
  },

  _storePersistedActions() {
    let json = JSON.stringify(this._persistedActions);
    Services.prefs.setStringPref(PREF_PERSISTED_ACTIONS, json);
  },

  _loadPersistedActions() {
    let actions;
    try {
      let json = Services.prefs.getStringPref(PREF_PERSISTED_ACTIONS);
      actions = this._migratePersistedActions(JSON.parse(json));
    } catch (ex) {}

    // Handle migrating to and from Proton.  We want to gracefully handle
    // downgrades from Proton, and since Proton is controlled by a pref, we also
    // don't want to assume that a downgrade is possible only by downgrading the
    // app.  That makes it hard to use the normal migration approach of creating
    // a new persisted actions version, so we handle Proton migration specially.
    // We try-catch it separately from the earlier _migratePersistedActions call
    // because it should not be short-circuited when the pref load or usual
    // migration fails.
    try {
      actions = this._migratePersistedActionsProton(actions);
    } catch (ex) {}

    // If `actions` is still not defined, then this._persistedActions will
    // remain its default value.
    if (actions) {
      this._persistedActions = actions;
    }
  },

  _purgeUnregisteredPersistedActions() {
    // Remove all action IDs from persisted state that do not correspond to
    // currently registered actions.
    for (let name of ["ids", "idsInUrlbar"]) {
      this._persistedActions[name] = this._persistedActions[name].filter(id => {
        return this.actionForID(id);
      });
    }
    this._storePersistedActions();
  },

  _migratePersistedActions(actions) {
    // Start with actions.version and migrate one version at a time, all the way
    // up to the current version.
    for (
      let version = actions.version || 0;
      version < PERSISTED_ACTIONS_CURRENT_VERSION;
      version++
    ) {
      let methodName = `_migratePersistedActionsTo${version + 1}`;
      actions = this[methodName](actions);
      actions.version = version + 1;
    }
    return actions;
  },

  _migratePersistedActionsTo1(actions) {
    // The `ids` object is a mapping: action ID => true.  Convert it to an array
    // to save space in the prefs.
    let ids = [];
    for (let id in actions.ids) {
      ids.push(id);
    }
    // Move the bookmark ID to the end of idsInUrlbar.  The bookmark action
    // should always remain at the end of the urlbar, if present.
    let bookmarkIndex = actions.idsInUrlbar.indexOf(ACTION_ID_BOOKMARK);
    if (bookmarkIndex >= 0) {
      actions.idsInUrlbar.splice(bookmarkIndex, 1);
      actions.idsInUrlbar.push(ACTION_ID_BOOKMARK);
    }
    return {
      ids,
      idsInUrlbar: actions.idsInUrlbar,
    };
  },

  _migratePersistedActionsProton(actions) {
    if (UrlbarPrefs.get(PROTON_PREF)) {
      if (actions?.idsInUrlbarPreProton) {
        // continue with Proton
      } else if (actions) {
        // upgrade to Proton
        actions.idsInUrlbarPreProton = [...(actions.idsInUrlbar || [])];
      } else {
        // new profile with Proton
        actions = {
          ids: [],
          idsInUrlbar: [],
          idsInUrlbarPreProton: [],
          version: PERSISTED_ACTIONS_CURRENT_VERSION,
        };
      }
    } else if (actions?.idsInUrlbarPreProton) {
      // downgrade from Proton
      // actions.ids will not include any pre-Proton built-in (or non-built-in)
      // actions in idsInUrlbarPreProton, so add them back.
      for (let id of actions.idsInUrlbarPreProton) {
        if (!actions.ids.includes(id)) {
          actions.ids.push(id);
        }
      }
      actions.idsInUrlbar = actions.idsInUrlbarPreProton;
      delete actions.idsInUrlbarPreProton;
      // If idsInUrlbarPreProton was empty, we don't know whether it's because
      // the user is coming from a profile where Proton was always enabled or
      // one where they upgraded to Proton.  In the first case, we don't want
      // them to end up without the default pinned actions, so pin them.  That
      // means we'll get the second case wrong if the user unpinned all actions,
      // but no big deal.
      if (!actions.idsInUrlbar.length) {
        actions.idsInUrlbar = ["pocket", ACTION_ID_BOOKMARK];
      }
    } else if (actions) {
      // continue without Proton
    } else {
      // new profile without Proton
      actions = {
        ids: [],
        idsInUrlbar: [],
        version: PERSISTED_ACTIONS_CURRENT_VERSION,
      };
    }
    return actions;
  },

  // This keeps track of all actions, even those that are not currently
  // registered because they have been removed, so long as
  // _purgeUnregisteredPersistedActions has not been called.
  _persistedActions: {
    version: PERSISTED_ACTIONS_CURRENT_VERSION,
    // action IDs that have ever been seen and not removed, order not important
    ids: [],
    // action IDs ordered by position in urlbar
    idsInUrlbar: [],
  },
};

/**
 * A single page action.
 *
 * Each action can have both per-browser-window state and global state.
 * Per-window state takes precedence over global state.  This is reflected in
 * the title, tooltip, disabled, and icon properties.  Each of these properties
 * has a getter method and setter method that takes a browser window.  Pass null
 * to get the action's global state.  Pass a browser window to get the per-
 * window state.  However, if you pass a window and the action has no state for
 * that window, then the global state will be returned.
 *
 * `options` is a required object with the following properties.  Regarding the
 * properties discussed in the previous paragraph, the values in `options` set
 * global state.
 *
 * @param id (string, required)
 *        The action's ID.  Treat this like the ID of a DOM node.
 * @param title (string, optional)
 *        The action's title. It is optional for built in actions.
 * @param anchorIDOverride (string, optional)
 *        Pass a string to override the node to which the action's activated-
 *        action panel is anchored.
 * @param disabled (bool, optional)
 *        Pass true to cause the action to be disabled initially in all browser
 *        windows.  False by default.
 * @param extensionID (string, optional)
 *        If the action lives in an extension, pass its ID.
 * @param iconURL (string or object, optional)
 *        The URL string of the action's icon.  Usually you want to specify an
 *        icon in CSS, but this option is useful if that would be a pain for
 *        some reason.  You can also pass an object that maps pixel sizes to
 *        URLs, like { 16: url16, 32: url32 }.  The best size for the user's
 *        screen will be used.
 * @param isBadged (bool, optional)
 *        If true, the toolbarbutton for this action will get a
 *        "badged" attribute.
 * @param onBeforePlacedInWindow (function, optional)
 *        Called before the action is placed in the window:
 *        onBeforePlacedInWindow(window)
 *        * window: The window that the action will be placed in.
 * @param onCommand (function, optional)
 *        Called when the action is clicked, but only if it has neither a
 *        subview nor an iframe:
 *        onCommand(event, buttonNode)
 *        * event: The triggering event.
 *        * buttonNode: The button node that was clicked.
 * @param onIframeHiding (function, optional)
 *        Called when the action's iframe is hiding:
 *        onIframeHiding(iframeNode, parentPanelNode)
 *        * iframeNode: The iframe.
 *        * parentPanelNode: The panel node in which the iframe is shown.
 * @param onIframeHidden (function, optional)
 *        Called when the action's iframe is hidden:
 *        onIframeHidden(iframeNode, parentPanelNode)
 *        * iframeNode: The iframe.
 *        * parentPanelNode: The panel node in which the iframe is shown.
 * @param onIframeShowing (function, optional)
 *        Called when the action's iframe is showing to the user:
 *        onIframeShowing(iframeNode, parentPanelNode)
 *        * iframeNode: The iframe.
 *        * parentPanelNode: The panel node in which the iframe is shown.
 * @param onLocationChange (function, optional)
 *        Called after tab switch or when the current <browser>'s location
 *        changes:
 *        onLocationChange(browserWindow)
 *        * browserWindow: The browser window containing the tab switch or
 *          changed <browser>.
 * @param onPlacedInPanel (function, optional)
 *        Called when the action is added to the page action panel in a browser
 *        window:
 *        onPlacedInPanel(buttonNode)
 *        * buttonNode: The action's node in the page action panel.
 * @param onPlacedInUrlbar (function, optional)
 *        Called when the action is added to the urlbar in a browser window:
 *        onPlacedInUrlbar(buttonNode)
 *        * buttonNode: The action's node in the urlbar.
 * @param onRemovedFromWindow (function, optional)
 *        Called after the action is removed from a browser window:
 *        onRemovedFromWindow(browserWindow)
 *        * browserWindow: The browser window that the action was removed from.
 * @param onShowingInPanel (function, optional)
 *        Called when a browser window's page action panel is showing:
 *        onShowingInPanel(buttonNode)
 *        * buttonNode: The action's node in the page action panel.
 * @param onSubviewPlaced (function, optional)
 *        Called when the action's subview is added to its parent panel in a
 *        browser window:
 *        onSubviewPlaced(panelViewNode)
 *        * panelViewNode: The subview's panelview node.
 * @param onSubviewShowing (function, optional)
 *        Called when the action's subview is showing in a browser window:
 *        onSubviewShowing(panelViewNode)
 *        * panelViewNode: The subview's panelview node.
 * @param panelFluentID (string, optional)
 *        The action panel node's corresponding fluent attribute used for its label.
 * @param pinnedToUrlbar (bool, optional)
 *        Pass true to pin the action to the urlbar.  An action is shown in the
 *        urlbar if it's pinned and not disabled.  False by default.
 * @param tooltip (string, optional)
 *        The action's button tooltip text.
 * @param urlbarIDOverride (string, optional)
 *        Usually the ID of the action's button in the urlbar will be generated
 *        automatically.  Pass a string for this property to override that with
 *        your own ID.
 * @param urlbarFluentID (string, optional)
 *        The action urlbar node's corresponding fluent attribute used for its
 *        tooltip.
 * @param wantsIframe (bool, optional)
 *        Pass true to make an action that shows an iframe in a panel when
 *        clicked.
 * @param wantsSubview (bool, optional)
 *        Pass true to make an action that shows a panel subview when clicked.
 * @param disablePrivateBrowsing (bool, optional)
 *        Pass true to prevent the action from showing in a private browsing window.
 */
function Action(options) {
  setProperties(this, options, {
    id: true,
    title: false,
    anchorIDOverride: false,
    disabled: false,
    extensionID: false,
    iconURL: false,
    isBadged: false,
    labelForHistogram: false,
    onBeforePlacedInWindow: false,
    onCommand: false,
    onIframeHiding: false,
    onIframeHidden: false,
    onIframeShowing: false,
    onLocationChange: false,
    onPlacedInPanel: false,
    onPlacedInUrlbar: false,
    onRemovedFromWindow: false,
    onShowingInPanel: false,
    onSubviewPlaced: false,
    onSubviewShowing: false,
    onPinToUrlbarToggled: false,
    panelFluentID: false,
    pinnedToUrlbar: false,
    tooltip: false,
    urlbarIDOverride: false,
    urlbarFluentID: false,
    wantsIframe: false,
    wantsSubview: false,
    disablePrivateBrowsing: false,

    // private

    // (string, optional)
    // The ID of another action before which to insert this new action in the
    // panel.
    _insertBeforeActionID: false,

    // (bool, optional)
    // True if this isn't really an action but a separator to be shown in the
    // page action panel.
    _isSeparator: false,

    // (bool, optional)
    // Transient actions have a couple of special properties: (1) They stick to
    // the bottom of the panel, and (2) they're hidden in the panel when they're
    // disabled.  Other than that they behave like other actions.
    _transient: false,

    // (bool, optional)
    // True if the action's urlbar button is defined in markup.  In that case, a
    // node with the action's urlbar node ID should already exist in the DOM
    // (either the auto-generated ID or urlbarIDOverride).  That node will be
    // shown when the action is added to the urlbar and hidden when the action
    // is removed from the urlbar.
    _urlbarNodeInMarkup: false,
  });

  /**
   * A cache of the pre-computed CSS variable values for a given icon
   * URLs object, as passed to _createIconProperties.
   */
  this._iconProperties = new WeakMap();

  /**
   * The global values for the action properties.
   */
  this._globalProps = {
    disabled: this._disabled,
    iconURL: this._iconURL,
    iconProps: this._createIconProperties(this._iconURL),
    title: this._title,
    tooltip: this._tooltip,
    wantsSubview: this._wantsSubview,
  };

  /**
   * A mapping of window-specific action property objects, each of which
   * derives from the _globalProps object.
   */
  this._windowProps = new WeakMap();
}

Action.prototype = {
  /**
   * The ID of the action's parent extension (string)
   */
  get extensionID() {
    return this._extensionID;
  },

  /**
   * The action's ID (string)
   */
  get id() {
    return this._id;
  },

  /**
   * The action panel node's fluent ID (string)
   */
  get panelFluentID() {
    return this._panelFluentID;
  },

  set panelFluentID(id) {
    this._panelFluentID = id;
  },

  /**
   * The action urlbar node's fluent ID (string)
   */
  get urlbarFluentID() {
    return this._urlbarFluentID;
  },

  set urlbarFluentID(id) {
    this._urlbarFluentID = id;
  },

  get disablePrivateBrowsing() {
    return !!this._disablePrivateBrowsing;
  },

  /**
   * Verifies that the action can be shown in a private window.  For
   * extensions, verifies the extension has access to the window.
   */
  canShowInWindow(browserWindow) {
    if (this._extensionID) {
      let policy = WebExtensionPolicy.getByID(this._extensionID);
      if (!policy.canAccessWindow(browserWindow)) {
        return false;
      }
    }
    return !(
      this.disablePrivateBrowsing &&
      PrivateBrowsingUtils.isWindowPrivate(browserWindow)
    );
  },

  /**
   * True if the action is pinned to the urlbar.  The action is shown in the
   * urlbar if it's pinned and not disabled.  (bool)
   */
  get pinnedToUrlbar() {
    return this._pinnedToUrlbar || false;
  },
  set pinnedToUrlbar(shown) {
    if (this.pinnedToUrlbar != shown) {
      this._pinnedToUrlbar = shown;
      PageActions.onActionToggledPinnedToUrlbar(this);
      this.onPinToUrlbarToggled();
    }
  },

  /**
   * The action's disabled state (bool)
   */
  getDisabled(browserWindow = null) {
    return !!this._getProperties(browserWindow).disabled;
  },
  setDisabled(value, browserWindow = null) {
    return this._setProperty("disabled", !!value, browserWindow);
  },

  /**
   * The action's icon URL string, or an object mapping sizes to URL strings
   * (string or object)
   */
  getIconURL(browserWindow = null) {
    return this._getProperties(browserWindow).iconURL;
  },
  setIconURL(value, browserWindow = null) {
    let props = this._getProperties(browserWindow, !!browserWindow);
    props.iconURL = value;
    props.iconProps = this._createIconProperties(value);

    this._updateProperty("iconURL", props.iconProps, browserWindow);
    return value;
  },

  /**
   * The set of CSS variables which define the action's icons in various
   * sizes. This is generated automatically from the iconURL property.
   */
  getIconProperties(browserWindow = null) {
    return this._getProperties(browserWindow).iconProps;
  },

  _createIconProperties(urls) {
    if (urls && typeof urls == "object") {
      let props = this._iconProperties.get(urls);
      if (!props) {
        props = Object.freeze({
          "--pageAction-image-16px": escapeCSSURL(
            this._iconURLForSize(urls, 16)
          ),
          "--pageAction-image-32px": escapeCSSURL(
            this._iconURLForSize(urls, 32)
          ),
        });
        this._iconProperties.set(urls, props);
      }
      return props;
    }

    let cssURL = urls ? escapeCSSURL(urls) : null;
    return Object.freeze({
      "--pageAction-image-16px": cssURL,
      "--pageAction-image-32px": cssURL,
    });
  },

  /**
   * The action's title (string). Note, built in actions will
   * not have a title property.
   */
  getTitle(browserWindow = null) {
    return this._getProperties(browserWindow).title;
  },
  setTitle(value, browserWindow = null) {
    return this._setProperty("title", value, browserWindow);
  },

  /**
   * The action's tooltip (string)
   */
  getTooltip(browserWindow = null) {
    return this._getProperties(browserWindow).tooltip;
  },
  setTooltip(value, browserWindow = null) {
    return this._setProperty("tooltip", value, browserWindow);
  },

  /**
   * Whether the action wants a subview (bool)
   */
  getWantsSubview(browserWindow = null) {
    return !!this._getProperties(browserWindow).wantsSubview;
  },
  setWantsSubview(value, browserWindow = null) {
    return this._setProperty("wantsSubview", !!value, browserWindow);
  },

  /**
   * Sets a property, optionally for a particular browser window.
   *
   * @param  name (string, required)
   *         The (non-underscored) name of the property.
   * @param  value
   *         The value.
   * @param  browserWindow (DOM window, optional)
   *         If given, then the property will be set in this window's state, not
   *         globally.
   */
  _setProperty(name, value, browserWindow) {
    let props = this._getProperties(browserWindow, !!browserWindow);
    props[name] = value;

    this._updateProperty(name, value, browserWindow);
    return value;
  },

  _updateProperty(name, value, browserWindow) {
    // This may be called before the action has been added.
    if (PageActions.actionForID(this.id)) {
      for (let bpa of allBrowserPageActions(browserWindow)) {
        bpa.updateAction(this, name, { value });
      }
    }
  },

  /**
   * Returns the properties object for the given window, if it exists,
   * or the global properties object if no window-specific properties
   * exist.
   *
   * @param {Window?} window
   *        The window for which to return the properties object, or
   *        null to return the global properties object.
   * @param {bool} [forceWindowSpecific = false]
   *        If true, always returns a window-specific properties object.
   *        If a properties object does not exist for the given window,
   *        one is created and cached.
   * @returns {object}
   */
  _getProperties(window, forceWindowSpecific = false) {
    let props = window && this._windowProps.get(window);

    if (!props && forceWindowSpecific) {
      props = Object.create(this._globalProps);
      this._windowProps.set(window, props);
    }

    return props || this._globalProps;
  },

  /**
   * Override for the ID of the action's activated-action panel anchor (string)
   */
  get anchorIDOverride() {
    return this._anchorIDOverride;
  },

  /**
   * Override for the ID of the action's urlbar node (string)
   */
  get urlbarIDOverride() {
    return this._urlbarIDOverride;
  },

  /**
   * True if the action is shown in an iframe (bool)
   */
  get wantsIframe() {
    return this._wantsIframe || false;
  },

  get isBadged() {
    return this._isBadged || false;
  },

  get labelForHistogram() {
    // The histogram label value has a length limit of 20 and restricted to a
    // pattern. See MAX_LABEL_LENGTH and CPP_IDENTIFIER_PATTERN in
    // toolkit/components/telemetry/parse_histograms.py
    return (
      this._labelForHistogram ||
      this._id.replace(/_\w{1}/g, match => match[1].toUpperCase()).substr(0, 20)
    );
  },

  /**
   * Selects the best matching icon from the given URLs object for the
   * given preferred size.
   *
   * @param {object} urls
   *        An object containing square icons of various sizes. The name
   *        of each property is its width, and the value is its image URL.
   * @param {integer} peferredSize
   *        The preferred icon width. The most appropriate icon in the
   *        urls object will be chosen to match that size. An exact
   *        match will be preferred, followed by an icon exactly double
   *        the size, followed by the smallest icon larger than the
   *        preferred size, followed by the largest available icon.
   * @returns {string}
   *        The chosen icon URL.
   */
  _iconURLForSize(urls, preferredSize) {
    // This case is copied from ExtensionParent.jsm so that our image logic is
    // the same, so that WebExtensions page action tests that deal with icons
    // pass.
    let bestSize = null;
    if (urls[preferredSize]) {
      bestSize = preferredSize;
    } else if (urls[2 * preferredSize]) {
      bestSize = 2 * preferredSize;
    } else {
      let sizes = Object.keys(urls)
        .map(key => parseInt(key, 10))
        .sort((a, b) => a - b);
      bestSize =
        sizes.find(candidate => candidate > preferredSize) || sizes.pop();
    }
    return urls[bestSize];
  },

  /**
   * Performs the command for an action.  If the action has an onCommand
   * handler, then it's called.  If the action has a subview or iframe, then a
   * panel is opened, displaying the subview or iframe.
   *
   * @param  browserWindow (DOM window, required)
   *         The browser window in which to perform the action.
   */
  doCommand(browserWindow) {
    browserPageActions(browserWindow).doCommandForAction(this);
  },

  /**
   * Call this when before placing the action in the window.
   *
   * @param  browserWindow (DOM window, required)
   *         The browser window the action will be placed in.
   */
  onBeforePlacedInWindow(browserWindow) {
    if (this._onBeforePlacedInWindow) {
      this._onBeforePlacedInWindow(browserWindow);
    }
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
   * Call this when the action's iframe is hiding.
   *
   * @param  iframeNode (DOM node, required)
   *         The iframe that's hiding.
   * @param  parentPanelNode (DOM node, required)
   *         The panel in which the iframe is hiding.
   */
  onIframeHiding(iframeNode, parentPanelNode) {
    if (this._onIframeHiding) {
      this._onIframeHiding(iframeNode, parentPanelNode);
    }
  },

  /**
   * Call this when the action's iframe is hidden.
   *
   * @param  iframeNode (DOM node, required)
   *         The iframe that's being hidden.
   * @param  parentPanelNode (DOM node, required)
   *         The panel in which the iframe is hidden.
   */
  onIframeHidden(iframeNode, parentPanelNode) {
    if (this._onIframeHidden) {
      this._onIframeHidden(iframeNode, parentPanelNode);
    }
  },

  /**
   * Call this when the action's iframe is showing.
   *
   * @param  iframeNode (DOM node, required)
   *         The iframe that's being shown.
   * @param  parentPanelNode (DOM node, required)
   *         The panel in which the iframe is shown.
   */
  onIframeShowing(iframeNode, parentPanelNode) {
    if (this._onIframeShowing) {
      this._onIframeShowing(iframeNode, parentPanelNode);
    }
  },

  /**
   * Call this on tab switch or when the current <browser>'s location changes.
   *
   * @param  browserWindow (DOM window, required)
   *         The browser window containing the tab switch or changed <browser>.
   */
  onLocationChange(browserWindow) {
    if (this._onLocationChange) {
      this._onLocationChange(browserWindow);
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
   * Call this when the DOM nodes for the action are removed from a browser
   * window.
   *
   * @param  browserWindow (DOM window, required)
   *         The browser window the action was removed from.
   */
  onRemovedFromWindow(browserWindow) {
    if (this._onRemovedFromWindow) {
      this._onRemovedFromWindow(browserWindow);
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
   * Call this when a panelview node for the action's subview is added to the
   * DOM.
   *
   * @param  panelViewNode (DOM node, required)
   *         The subview's panelview node.
   */
  onSubviewPlaced(panelViewNode) {
    if (this._onSubviewPlaced) {
      this._onSubviewPlaced(panelViewNode);
    }
  },

  /**
   * Call this when a panelview node for the action's subview is showing.
   *
   * @param  panelViewNode (DOM node, required)
   *         The subview's panelview node.
   */
  onSubviewShowing(panelViewNode) {
    if (this._onSubviewShowing) {
      this._onSubviewShowing(panelViewNode);
    }
  },
  /**
   * Call this when an icon in the url is pinned or unpinned.
   */
  onPinToUrlbarToggled() {
    if (this._onPinToUrlbarToggled) {
      this._onPinToUrlbarToggled();
    }
  },

  /**
   * Removes the action's DOM nodes from all browser windows.
   *
   * PageActions will remember the action's urlbar placement, if any, after this
   * method is called until app shutdown.  If the action is not added again
   * before shutdown, then PageActions will discard the placement, and the next
   * time the action is added, its placement will be reset.
   */
  remove() {
    PageActions.onActionRemoved(this);
  },

  /**
   * Returns whether the action should be shown in a given window's panel.
   *
   * @param  browserWindow (DOM window, required)
   *         The window.
   * @return True if the action should be shown and false otherwise.  Actions
   *         are always shown in the panel unless they're both transient and
   *         disabled.
   */
  shouldShowInPanel(browserWindow) {
    return (
      (!this.__transient || !this.getDisabled(browserWindow)) &&
      this.canShowInWindow(browserWindow)
    );
  },

  /**
   * Returns whether the action should be shown in a given window's urlbar.
   *
   * @param  browserWindow (DOM window, required)
   *         The window.
   * @return True if the action should be shown and false otherwise.  The action
   *         should be shown if it's both pinned and not disabled.
   */
  shouldShowInUrlbar(browserWindow) {
    return (
      this.pinnedToUrlbar &&
      !this.getDisabled(browserWindow) &&
      this.canShowInWindow(browserWindow)
    );
  },

  get _isBuiltIn() {
    let builtInIDs = ["pocket", "screenshots_mozilla_org"].concat(
      gBuiltInActions.filter(a => !a.__isSeparator).map(a => a.id)
    );
    return builtInIDs.includes(this.id);
  },

  get _isMozillaAction() {
    return this._isBuiltIn || this.id == "webcompat-reporter_mozilla_org";
  },
};

PageActions.Action = Action;

PageActions.ACTION_ID_BUILT_IN_SEPARATOR = ACTION_ID_BUILT_IN_SEPARATOR;
PageActions.ACTION_ID_TRANSIENT_SEPARATOR = ACTION_ID_TRANSIENT_SEPARATOR;

// These are only necessary so that Pocket and the test can use them.
PageActions.ACTION_ID_BOOKMARK = ACTION_ID_BOOKMARK;
PageActions.ACTION_ID_PIN_TAB = ACTION_ID_PIN_TAB;
PageActions.ACTION_ID_BOOKMARK_SEPARATOR = ACTION_ID_BOOKMARK_SEPARATOR;
PageActions.PREF_PERSISTED_ACTIONS = PREF_PERSISTED_ACTIONS;

// Sorted in the order in which they should appear in the page action panel.
// Does not include the page actions of extensions bundled with the browser.
// They're added by the relevant extension code.
// NOTE: If you add items to this list (or system add-on actions that we
// want to keep track of), make sure to also update Histograms.json for the
// new actions.
var gBuiltInActions;

PageActions._initBuiltInActions = function() {
  gBuiltInActions = [
    // bookmark
    {
      id: ACTION_ID_BOOKMARK,
      urlbarIDOverride: "star-button-box",
      _urlbarNodeInMarkup: true,
      pinnedToUrlbar: true,
      onShowingInPanel(buttonNode) {
        browserPageActions(buttonNode).bookmark.onShowingInPanel(buttonNode);
      },
      onCommand(event, buttonNode) {
        browserPageActions(buttonNode).bookmark.onCommand(event, buttonNode);
      },
    },
  ];

  if (UrlbarPrefs.get(PROTON_PREF)) {
    return;
  }

  gBuiltInActions.push(
    ...[
      // pin tab
      {
        id: ACTION_ID_PIN_TAB,
        onBeforePlacedInWindow(browserWindow) {
          function handlePinEvent() {
            browserPageActions(browserWindow).pinTab.updateState();
          }
          function handleWindowUnload() {
            for (let event of ["TabPinned", "TabUnpinned"]) {
              browserWindow.removeEventListener(event, handlePinEvent);
            }
          }

          for (let event of ["TabPinned", "TabUnpinned"]) {
            browserWindow.addEventListener(event, handlePinEvent);
          }
          browserWindow.addEventListener("unload", handleWindowUnload, {
            once: true,
          });
        },
        onPlacedInPanel(buttonNode) {
          browserPageActions(buttonNode).pinTab.updateState();
        },
        onPlacedInUrlbar(buttonNode) {
          browserPageActions(buttonNode).pinTab.updateState();
        },
        onLocationChange(browserWindow) {
          browserPageActions(browserWindow).pinTab.updateState();
        },
        onCommand(event, buttonNode) {
          browserPageActions(buttonNode).pinTab.onCommand(event, buttonNode);
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
        panelFluentID: "page-action-copy-url-panel",
        urlbarFluentID: "page-action-copy-url-urlbar",
        onCommand(event, buttonNode) {
          browserPageActions(buttonNode).copyURL.onCommand(event, buttonNode);
        },
      },

      // email link
      {
        id: "emailLink",
        panelFluentID: "page-action-email-link-panel",
        urlbarFluentID: "page-action-email-link-urlbar",
        onCommand(event, buttonNode) {
          browserPageActions(buttonNode).emailLink.onCommand(event, buttonNode);
        },
      },

      // add search engine
      {
        id: "addSearchEngine",
        // The title is set in browser-pageActions.js.
        isBadged: true,
        _transient: true,
        onShowingInPanel(buttonNode) {
          browserPageActions(buttonNode).addSearchEngine.onShowingInPanel();
        },
        onCommand(event, buttonNode) {
          browserPageActions(buttonNode).addSearchEngine.onCommand(
            event,
            buttonNode
          );
        },
        onSubviewShowing(panelViewNode) {
          browserPageActions(panelViewNode).addSearchEngine.onSubviewShowing(
            panelViewNode
          );
        },
      },
    ]
  );

  // send to device
  if (Services.prefs.getBoolPref("identity.fxaccounts.enabled")) {
    gBuiltInActions.push({
      id: "sendToDevice",
      panelFluentID: "page-action-send-tabs-panel",
      // The actual title is set by each window, per window, and depends on the
      // number of tabs that are selected.
      urlbarFluentID: "page-action-send-tabs-urlbar",
      onBeforePlacedInWindow(browserWindow) {
        browserPageActions(browserWindow).sendToDevice.onBeforePlacedInWindow(
          browserWindow
        );
      },
      onLocationChange(browserWindow) {
        browserPageActions(browserWindow).sendToDevice.onLocationChange();
      },
      wantsSubview: true,
      onSubviewPlaced(panelViewNode) {
        browserPageActions(panelViewNode).sendToDevice.onSubviewPlaced(
          panelViewNode
        );
      },
      onSubviewShowing(panelViewNode) {
        browserPageActions(panelViewNode).sendToDevice.onShowingSubview(
          panelViewNode
        );
      },
    });
  }

  // share URL
  if (AppConstants.platform == "macosx") {
    gBuiltInActions.push({
      id: "shareURL",
      panelFluentID: "page-action-share-url-panel",
      urlbarFluentID: "page-action-share-url-urlbar",
      onShowingInPanel(buttonNode) {
        browserPageActions(buttonNode).shareURL.onShowingInPanel(buttonNode);
      },
      wantsSubview: true,
      onSubviewShowing(panelViewNode) {
        browserPageActions(panelViewNode).shareURL.onShowingSubview(
          panelViewNode
        );
      },
    });
  }

  if (AppConstants.isPlatformAndVersionAtLeast("win", "6.4")) {
    gBuiltInActions.push(
      // Share URL
      {
        id: "shareURL",
        panelFluentID: "page-action-share-url-panel",
        urlbarFluentID: "page-action-share-url-urlbar",
        onCommand(event, buttonNode) {
          browserPageActions(buttonNode).shareURL.onCommand(event, buttonNode);
        },
      }
    );
  }
};

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
 *
 * @param browserWindow (DOM window, optional)
 *        If given, then only this window will be yielded.  That may sound
 *        pointless, but it can make callers nicer to write since they don't
 *        need two separate cases, one where a window is given and another where
 *        it isn't.
 */
function* allBrowserWindows(browserWindow = null) {
  if (browserWindow) {
    yield browserWindow;
    return;
  }
  yield* Services.wm.getEnumerator("navigator:browser");
}

/**
 * A generator function for BrowserPageActions objects in all open windows.
 *
 * @param browserWindow (DOM window, optional)
 *        If given, then the BrowserPageActions for only this window will be
 *        yielded.
 */
function* allBrowserPageActions(browserWindow = null) {
  for (let win of allBrowserWindows(browserWindow)) {
    yield browserPageActions(win);
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
