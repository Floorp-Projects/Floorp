/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "PageActions",
  // PageActions.Action
  // PageActions.Button
  // PageActions.Subview
  // PageActions.ACTION_ID_BOOKMARK
  // PageActions.ACTION_ID_BOOKMARK_SEPARATOR
  // PageActions.ACTION_ID_BUILT_IN_SEPARATOR
];

const { utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "BinarySearch",
  "resource://gre/modules/BinarySearch.jsm");


const ACTION_ID_BOOKMARK = "bookmark";
const ACTION_ID_BOOKMARK_SEPARATOR = "bookmarkSeparator";
const ACTION_ID_BUILT_IN_SEPARATOR = "builtInSeparator";

const PREF_PERSISTED_ACTIONS = "browser.pageActions.persistedActions";
const PERSISTED_ACTIONS_CURRENT_VERSION = 1;


this.PageActions = {
  /**
   * Inits.  Call to init.
   */
  init() {
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
    // it this way means that all windows initially place their actions the same
    // way -- placeAllActions -- regardless of whether they're open when this
    // method is called or opened later.
    for (let bpa of allBrowserPageActions()) {
      bpa.placeAllActions();
    }

    // These callbacks are deferred until init happens and all built-in actions
    // are added.
    while (callbacks && callbacks.length) {
      callbacks.shift()();
    }
  },

  _deferredAddActionCalls: [],

  /**
   * The list of Action objects, sorted in the order in which they should be
   * placed in the page action panel.  If there are both built-in and non-built-
   * in actions, then the list will include the separator between the two.  The
   * list is not live.  (array of Action objects)
   */
  get actions() {
    let actions = this.builtInActions;
    if (this.nonBuiltInActions.length) {
      // There are non-built-in actions, so include them too.
      if (actions.length) {
        // There are both built-in and non-built-in actions.  Add a separator
        // between the two groups so that the returned array looks like:
        // [...built-ins, separator, ...non-built-ins]
        actions.push(new Action({
          id: ACTION_ID_BUILT_IN_SEPARATOR,
          _isSeparator: true,
        }));
      }
      actions.push(...this.nonBuiltInActions);
    }
    return actions;
  },

  /**
   * The list of built-in actions.  Not live.  (array of Action objects)
   */
  get builtInActions() {
    return this._builtInActions.slice();
  },

  /**
   * The list of non-built-in actions.  Not live.  (array of Action objects)
   */
  get nonBuiltInActions() {
    return this._nonBuiltInActions.slice();
  },

  /**
   * The list of actions in the urlbar, sorted in the order in which they should
   * appear there.  Not live.  (array of Action objects)
   */
  get actionsInUrlbar() {
    // Remember that IDs in idsInUrlbar may belong to actions that aren't
    // currently registered.
    return this._persistedActions.idsInUrlbar.reduce((actions, id) => {
      let action = this.actionForID(id);
      if (action) {
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

    let hadSep = this.actions.some(a => a.id == ACTION_ID_BUILT_IN_SEPARATOR);

    this._registerAction(action);

    let sep = null;
    if (!hadSep) {
      sep = this.actions.find(a => a.id == ACTION_ID_BUILT_IN_SEPARATOR);
    }

    for (let bpa of allBrowserPageActions()) {
      if (sep) {
        // There are now both built-in and non-built-in actions, so place the
        // separator between the two groups.
        bpa.placeAction(sep);
      }
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
      let index =
        !action.__insertBeforeActionID ? -1 :
        this._builtInActions.findIndex(a => {
          return a.id == action.__insertBeforeActionID;
        });
      if (index < 0) {
        // Append the action.
        index = this._builtInActions.length;
      }
      this._builtInActions.splice(index, 0, action);
    } else if (gBuiltInActions.find(a => a.id == action.id)) {
      // A built-in action.  These are always added on init before all other
      // actions, one after the other, so just push onto the array.
      this._builtInActions.push(action);
    } else {
      // A non-built-in action, like a non-bundled extension potentially.
      // Keep this list sorted by title.
      let index = BinarySearch.insertionIndexOf((a1, a2) => {
        return a1.title.localeCompare(a2.title);
      }, this._nonBuiltInActions, action);
      this._nonBuiltInActions.splice(index, 0, action);
    }

    if (this._persistedActions.ids.includes(action.id)) {
      // The action has been seen before.  Override its shownInUrlbar value
      // with the persisted value.  Set the private version of that property
      // so that onActionToggledShownInUrlbar isn't called, which happens when
      // the public version is set.
      action._shownInUrlbar =
        this._persistedActions.idsInUrlbar.includes(action.id);
    } else {
      // The action is new.  Store it in the persisted actions.
      this._persistedActions.ids.push(action.id);
      this._updateIDsInUrlbarForAction(action);
    }
  },

  _updateIDsInUrlbarForAction(action) {
    let index = this._persistedActions.idsInUrlbar.indexOf(action.id);
    if (action.shownInUrlbar) {
      if (index < 0) {
        let nextID = this.nextActionIDInUrlbar(action.id);
        let nextIndex =
          nextID ? this._persistedActions.idsInUrlbar.indexOf(nextID) : -1;
        if (nextIndex < 0) {
          nextIndex = this._persistedActions.idsInUrlbar.length;
        }
        this._persistedActions.idsInUrlbar.splice(nextIndex, 0, action.id);
      }
    } else if (index >= 0) {
      this._persistedActions.idsInUrlbar.splice(index, 1);
    }
    this._storePersistedActions();
  },

  _builtInActions: [],
  _nonBuiltInActions: [],
  _actionsByID: new Map(),

  /**
   * Returns the ID of the action before which the given action should be
   * inserted in the urlbar.
   *
   * @param  action (Action object, required)
   *         The action you're inserting.
   * @return The ID of the reference action, or null if your action should be
   *         appended.
   */
  nextActionIDInUrlbar(action) {
    // Actions in the urlbar are always inserted before the bookmark action,
    // which always comes last if it's present.
    if (action.id == ACTION_ID_BOOKMARK) {
      return null;
    }
    let id = this._nextActionID(action, this.actionsInUrlbar);
    return id || ACTION_ID_BOOKMARK;
  },

  /**
   * Returns the ID of the action before which the given action should be
   * inserted in the panel.
   *
   * @param  action (Action object, required)
   *         The action you're inserting.
   * @return The ID of the reference action, or null if your action should be
   *         appended.
   */
  nextActionIDInPanel(action) {
    return this._nextActionID(action, this.actions);
  },

  /**
   * The DOM nodes of actions should be ordered properly, both in the panel and
   * the urlbar.  This method returns the ID of the action that comes after the
   * given action in the given array.  You can use the returned ID to get a DOM
   * node ID to pass to node.insertBefore().
   *
   * Pass PageActions.actions to get the ID of the next action in the panel.
   * Pass PageActions.actionsInUrlbar to get the ID of the next action in the
   * urlbar.
   *
   * @param  action
   *         The action whose node you want to insert into your DOM.
   * @param  actionArray
   *         The relevant array of actions, either PageActions.actions or
   *         actionsInUrlbar.
   * @return The ID of the action before which the given action should be
   *         inserted.  If the given action should be inserted last, returns
   *         null.
   */
  _nextActionID(action, actionArray) {
    let index = actionArray.findIndex(a => a.id == action.id);
    if (index < 0) {
      return null;
    }
    let nextAction = actionArray[index + 1];
    if (!nextAction) {
      return null;
    }
    return nextAction.id;
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

    // Remove the action from persisted storage.
    for (let name of ["ids", "idsInUrlbar"]) {
      let array = this._persistedActions[name];
      let index = array.indexOf(action.id);
      if (index >= 0) {
        array.splice(index, 1);
      }
    }
    this._storePersistedActions();

    for (let bpa of allBrowserPageActions()) {
      bpa.removeAction(action);
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
    for (let bpa of allBrowserPageActions()) {
      bpa.updateActionIconURL(action);
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
    for (let bpa of allBrowserPageActions()) {
      bpa.updateActionTitle(action);
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
    this._updateIDsInUrlbarForAction(action);
    for (let bpa of allBrowserPageActions()) {
      bpa.placeActionInUrlbar(action);
    }
  },

  logTelemetry(type, action, node = null) {
    const kAllowedLabels = ["pocket", "screenshots", "webcompat"].concat(
      gBuiltInActions.filter(a => !a.__isSeparator).map(a => a.id)
    );

    if (type == "used") {
      type = (node && node.closest("#urlbar-container")) ? "urlbar_used" : "panel_used";
    }
    let histogramID = "FX_PAGE_ACTION_" + type.toUpperCase();
    try {
      let histogram = Services.telemetry.getHistogramById(histogramID);
      if (kAllowedLabels.includes(action.labelForHistogram)) {
        histogram.add(action.labelForHistogram);
      } else {
        histogram.add("other");
      }
    } catch (ex) {
      Cu.reportError(ex);
    }
  },

  _storePersistedActions() {
    let json = JSON.stringify(this._persistedActions);
    Services.prefs.setStringPref(PREF_PERSISTED_ACTIONS, json);
  },

  _loadPersistedActions() {
    try {
      let json = Services.prefs.getStringPref(PREF_PERSISTED_ACTIONS);
      this._persistedActions = this._migratePersistedActions(JSON.parse(json));
    } catch (ex) {}
  },

  _migratePersistedActions(actions) {
    // Start with actions.version and migrate one version at a time, all the way
    // up to the current version.
    for (let version = actions.version || 0;
         version < PERSISTED_ACTIONS_CURRENT_VERSION;
         version++) {
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
 * @param  options (object, required)
 *         An object with the following properties:
 *         @param id (string, required)
 *                The action's ID.  Treat this like the ID of a DOM node.
 *         @param title (string, required)
 *                The action's title.
 *         @param anchorIDOverride (string, optional)
 *                Pass a string for this property if to override which element
 *                that the temporary panel is anchored to.
 *         @param iconURL (string, optional)
 *                The URL of the action's icon.  Usually you want to specify an
 *                icon in CSS, but this option is useful if that would be a pain
 *                for some reason -- like your code is in an embedded
 *                WebExtension.
 *         @param nodeAttributes (object, optional)
 *                An object of name-value pairs.  Each pair will be added as
 *                an attribute to DOM nodes created for this action.
 *         @param onBeforePlacedInWindow (function, optional)
 *                Called before the action is placed in the window:
 *                onBeforePlacedInWindow(window)
 *                * window: The window that the action will be placed in.
 *         @param onCommand (function, optional)
 *                Called when the action is clicked, but only if it has neither
 *                a subview nor an iframe:
 *                onCommand(event, buttonNode)
 *                * event: The triggering event.
 *                * buttonNode: The button node that was clicked.
 *         @param onIframeHiding (function, optional)
 *                Called when the action's iframe is hiding:
 *                onIframeHiding(iframeNode, parentPanelNode)
 *                * iframeNode: The iframe.
 *                * parentPanelNode: The panel node in which the iframe is
 *                  shown.
 *         @param onIframeHidden (function, optional)
 *                Called when the action's iframe is hidden:
 *                onIframeHidden(iframeNode, parentPanelNode)
 *                * iframeNode: The iframe.
 *                * parentPanelNode: The panel node in which the iframe is
 *                  shown.
 *         @param onIframeShown (function, optional)
 *                Called when the action's iframe is shown to the user:
 *                onIframeShown(iframeNode, parentPanelNode)
 *                * iframeNode: The iframe.
 *                * parentPanelNode: The panel node in which the iframe is
 *                  shown.
 *         @param onLocationChange (function, optional)
 *                Called after tab switch or when the current <browser>'s
 *                location changes:
 *                onLocationChange(browserWindow)
 *                * browserWindow: The browser window containing the tab switch
 *                  or changed <browser>.
 *         @param onPlacedInPanel (function, optional)
 *                Called when the action is added to the page action panel in
 *                a browser window:
 *                onPlacedInPanel(buttonNode)
 *                * buttonNode: The action's node in the page action panel.
 *         @param onPlacedInUrlbar (function, optional)
 *                Called when the action is added to the urlbar in a browser
 *                window:
 *                onPlacedInUrlbar(buttonNode)
 *                * buttonNode: The action's node in the urlbar.
 *         @param onShowingInPanel (function, optional)
 *                Called when a browser window's page action panel is showing:
 *                onShowingInPanel(buttonNode)
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
    anchorIDOverride: false,
    iconURL: false,
    labelForHistogram: false,
    nodeAttributes: false,
    onBeforePlacedInWindow: false,
    onCommand: false,
    onIframeHiding: false,
    onIframeHidden: false,
    onIframeShown: false,
    onLocationChange: false,
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
    // The ID of another action before which to insert this new action in the
    // panel.
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
   * Override for the ID of the action's temporary panel anchor (string, nullable)
   */
  get anchorIDOverride() {
    return this._anchorIDOverride;
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

  get labelForHistogram() {
    return this._labelForHistogram || this._id;
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
   * Makes PageActions forget about this action and removes its DOM nodes from
   * all browser windows.  Call this when the user removes your action, like
   * when your extension is uninstalled.  You probably don't want to call it
   * simply when your extension is disabled or the app quits, because then
   * PageActions won't remember it the next time your extension is enabled or
   * the app starts.
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
 *                browser window:
 *                onPlaced(panelViewNode)
 *                * panelViewNode: The panelview node represented by this
 *                  Subview.
 *         @param onShowing (function, optional)
 *                Called when the subview is showing in a browser window:
 *                onShowing(panelViewNode)
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
 *                Called when the button is clicked:
 *                onCommand(event, buttonNode)
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


// These are only necessary so that Pocket and the test can use them.
this.PageActions.ACTION_ID_BOOKMARK = ACTION_ID_BOOKMARK;
this.PageActions.ACTION_ID_BOOKMARK_SEPARATOR = ACTION_ID_BOOKMARK_SEPARATOR;
this.PageActions.PREF_PERSISTED_ACTIONS = PREF_PERSISTED_ACTIONS;

// This is only necessary so that the test can access it.
this.PageActions.ACTION_ID_BUILT_IN_SEPARATOR = ACTION_ID_BUILT_IN_SEPARATOR;


// Sorted in the order in which they should appear in the page action panel.
// Does not include the page actions of extensions bundled with the browser.
// They're added by the relevant extension code.
// NOTE: If you add items to this list (or system add-on actions that we
// want to keep track of), make sure to also update Histograms.json for the
// new actions.
var gBuiltInActions = [

  // bookmark
  {
    id: ACTION_ID_BOOKMARK,
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
function* allBrowserWindows() {
  let windows = Services.wm.getEnumerator("navigator:browser");
  while (windows.hasMoreElements()) {
    yield windows.getNext();
  }
}

function* allBrowserPageActions() {
  for (let win of allBrowserWindows()) {
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
