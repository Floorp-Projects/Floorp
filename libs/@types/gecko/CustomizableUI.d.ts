export enum TCustomizableUIArea {
  /**
   * Constant reference to the ID of the navigation toolbar.
   */
  AREA_NAVBAR = "nav-bar",
  /**
   * Constant reference to the ID of the menubar's toolbar.
   */
  AREA_MENUBAR = "toolbar-menubar",
  /**
   * Constant reference to the ID of the tabstrip toolbar.
   */
  AREA_TABSTRIP = "TabsToolbar",
  /**
   * Constant reference to the ID of the bookmarks toolbar.
   */
  AREA_BOOKMARKS = "PersonalToolbar",
  /**
   * Constant reference to the ID of the non-dymanic (fixed) list in the overflow panel.
   */
  AREA_FIXED_OVERFLOW_PANEL = "widget-overflow-fixed-list",
  /**
   * Constant reference to the ID of the addons area.
   */
  AREA_ADDONS = "unified-extensions-area",
  /**
   * Constant reference to the ID of the customization palette, which is
   * where widgets go when they're not assigned to an area. Note that this
   * area is "virtual" in that it's never set as a value for a widgets
   * currentArea or defaultArea. It's only used for the `canWidgetMoveToArea`
   * function to check if widgets can be moved to the palette. Callers who
   * wish to move items to the palette should use `removeWidgetFromArea`.
   */
  AREA_NO_AREA = "customization-palette",
}

export enum TCustomizableUIType {
  /**
   * Constant indicating the area is a panel.
   */
  TYPE_PANEL = "panel",
  /**
   * Constant indicating the area is a toolbar.
   */
  TYPE_TOOLBAR = "toolbar",
}

export enum TCustomizableUIProvider {
  /**
   * Constant indicating a XUL-type provider.
   */
  PROVIDER_XUL = "xul",
  /**
   * Constant indicating an API-type provider.
   */
  PROVIDER_API = "api",
  /**
   * Constant indicating dynamic (special) widgets: spring, spacer, and separator.
   */
  PROVIDER_SPECIAL = "special",
}

export enum TCustomizableUISource {
  /**
   * Constant indicating the widget is built-in
   */
  SOURCE_BUILTIN = "builtin",
  /**
   * Constant indicating the widget is externally provided
   * (e.g. by add-ons or other items not part of the builtin widget set).
   */
  SOURCE_EXTERNAL = "external",
}

export enum TCustomizableUIReason {
  /**
   * Constant indicating the reason the event was fired was a window closing
   */
  REASON_WINDOW_CLOSED = "window-closed",
  /**
   * Constant indicating the reason the event was fired was an area being
   * unregistered separately from window closing mechanics.
   */
  REASON_AREA_UNREGISTERED = "area-unregistered",
}

/**
 * @todo https://searchfox.org/mozilla-central/rev/79f37e7fcd8994d86b0c610a87af4ef0343166ee/browser/components/customizableui/CustomizableUI.sys.mjs#5003
 */
export type TWidgetGroupWrapper = any;

export let TCustomizableUI: {
  /**
   * Constant reference to the ID of the navigation toolbar.
   */
  AREA_NAVBAR: TCustomizableUIArea;
  /**
   * Constant reference to the ID of the menubar's toolbar.
   */
  AREA_MENUBAR: TCustomizableUIArea;
  /**
   * Constant reference to the ID of the tabstrip toolbar.
   */
  AREA_TABSTRIP: TCustomizableUIArea;
  /**
   * Constant reference to the ID of the bookmarks toolbar.
   */
  AREA_BOOKMARKS: TCustomizableUIArea;
  /**
   * Constant reference to the ID of the non-dymanic (fixed) list in the overflow panel.
   */
  AREA_FIXED_OVERFLOW_PANEL: TCustomizableUIArea;
  /**
   * Constant reference to the ID of the addons area.
   */
  AREA_ADDONS: TCustomizableUIArea;
  /**
   * Constant reference to the ID of the customization palette, which is
   * where widgets go when they're not assigned to an area. Note that this
   * area is "virtual" in that it's never set as a value for a widgets
   * currentArea or defaultArea. It's only used for the `canWidgetMoveToArea`
   * function to check if widgets can be moved to the palette. Callers who
   * wish to move items to the palette should use `removeWidgetFromArea`.
   */
  AREA_NO_AREA: TCustomizableUIArea;
  /**
   * An iteratable property of windows managed by CustomizableUI.
   * Note that this can *only* be used as an iterator. ie:
   *     for (let window of CustomizableUI.windows) { ... }
   */
  /**
   * Constant indicating the area is a panel.
   */
  TYPE_PANEL: TCustomizableUIType;
  /**
   * Constant indicating the area is a toolbar.
   */
  TYPE_TOOLBAR: TCustomizableUIType;
  /**
   * Constant indicating a XUL-type provider.
   */
  PROVIDER_XUL: TCustomizableUIProvider;
  /**
   * Constant indicating an API-type provider.
   */
  PROVIDER_API: TCustomizableUIProvider;
  /**
   * Constant indicating dynamic (special) widgets: spring, spacer, and separator.
   */
  PROVIDER_SPECIAL: TCustomizableUIProvider;
  /**
   * Constant indicating the widget is built-in
   */
  SOURCE_BUILTIN: TCustomizableUISource;
  /**
   * Constant indicating the widget is externally provided
   * (e.g. by add-ons or other items not part of the builtin widget set).
   */
  SOURCE_EXTERNAL: TCustomizableUISource;
  /**
   * Constant indicating the reason the event was fired was a window closing
   */
  REASON_WINDOW_CLOSED: TCustomizableUIReason;
  /**
   * Constant indicating the reason the event was fired was an area being
   * unregistered separately from window closing mechanics.
   */
  REASON_AREA_UNREGISTERED: TCustomizableUIReason;

  windows: IterableIterator<Window>;

  /**
   * Add a listener object that will get fired for various events regarding
   * customization.
   *
   * @param aListener the listener object to add
   *
   * Not all event handler methods need to be defined.
   * CustomizableUI will catch exceptions. Events are dispatched
   * synchronously on the UI thread, so if you can delay any/some of your
   * processing, that is advisable. The following event handlers are supported:
   *   - onWidgetAdded(aWidgetId, aArea, aPosition)
   *     Fired when a widget is added to an area. aWidgetId is the widget that
   *     was added, aArea the area it was added to, and aPosition the position
   *     in which it was added.
   *   - onWidgetMoved(aWidgetId, aArea, aOldPosition, aNewPosition)
   *     Fired when a widget is moved within its area. aWidgetId is the widget
   *     that was moved, aArea the area it was moved in, aOldPosition its old
   *     position, and aNewPosition its new position.
   *   - onWidgetRemoved(aWidgetId, aArea)
   *     Fired when a widget is removed from its area. aWidgetId is the widget
   *     that was removed, aArea the area it was removed from.
   *
   *   - onWidgetBeforeDOMChange(aNode, aNextNode, aContainer, aIsRemoval)
   *     Fired *before* a widget's DOM node is acted upon by CustomizableUI
   *     (to add, move or remove it). aNode is the DOM node changed, aNextNode
   *     the DOM node (if any) before which a widget will be inserted,
   *     aContainer the *actual* DOM container (could be an overflow panel in
   *     case of an overflowable toolbar), and aWasRemoval is true iff the
   *     action about to happen is the removal of the DOM node.
   *   - onWidgetAfterDOMChange(aNode, aNextNode, aContainer, aWasRemoval)
   *     Like onWidgetBeforeDOMChange, but fired after the change to the DOM
   *     node of the widget.
   *
   *   - onWidgetReset(aNode, aContainer)
   *     Fired after a reset to default placements moves a widget's node to a
   *     different location. aNode is the widget's node, aContainer is the
   *     area it was moved into (NB: it might already have been there and been
   *     moved to a different position!)
   *   - onWidgetUndoMove(aNode, aContainer)
   *     Fired after undoing a reset to default placements moves a widget's
   *     node to a different location. aNode is the widget's node, aContainer
   *     is the area it was moved into (NB: it might already have been there
   *     and been moved to a different position!)
   *   - onAreaReset(aArea, aContainer)
   *     Fired after a reset to default placements is complete on an area's
   *     DOM node. Note that this is fired for each DOM node. aArea is the area
   *     that was reset, aContainer the DOM node that was reset.
   *
   *   - onWidgetCreated(aWidgetId)
   *     Fired when a widget with id aWidgetId has been created, but before it
   *     is added to any placements or any DOM nodes have been constructed.
   *     Only fired for API-based widgets.
   *   - onWidgetAfterCreation(aWidgetId, aArea)
   *     Fired after a widget with id aWidgetId has been created, and has been
   *     added to either its default area or the area in which it was placed
   *     previously. If the widget has no default area and/or it has never
   *     been placed anywhere, aArea may be null. Only fired for API-based
   *     widgets.
   *   - onWidgetDestroyed(aWidgetId)
   *     Fired when widgets are destroyed. aWidgetId is the widget that is
   *     being destroyed. Only fired for API-based widgets.
   *   - onWidgetInstanceRemoved(aWidgetId, aDocument)
   *     Fired when a window is unloaded and a widget's instance is destroyed
   *     because of this. Only fired for API-based widgets.
   *
   *   - onWidgetDrag(aWidgetId, aArea)
   *     Fired both when and after customize mode drag handling system tries
   *     to determine the width and height of widget aWidgetId when dragged to a
   *     different area. aArea will be the area the item is dragged to, or
   *     undefined after the measurements have been done and the node has been
   *     moved back to its 'regular' area.
   *
   *   - onCustomizeStart(aWindow)
   *     Fired when opening customize mode in aWindow.
   *   - onCustomizeEnd(aWindow)
   *     Fired when exiting customize mode in aWindow.
   *
   *   - onWidgetOverflow(aNode, aContainer)
   *     Fired when a widget's DOM node is overflowing its container, a toolbar,
   *     and will be displayed in the overflow panel.
   *   - onWidgetUnderflow(aNode, aContainer)
   *     Fired when a widget's DOM node is *not* overflowing its container, a
   *     toolbar, anymore.
   *   - onWindowOpened(aWindow)
   *     Fired when a window has been opened that is managed by CustomizableUI,
   *     once all of the prerequisite setup has been done.
   *   - onWindowClosed(aWindow)
   *     Fired when a window that has been managed by CustomizableUI has been
   *     closed.
   *   - onAreaNodeRegistered(aArea, aContainer)
   *     Fired after an area node is first built when it is registered. This
   *     is often when the window has opened, but in the case of add-ons,
   *     could fire when the node has just been registered with CustomizableUI
   *     after an add-on update or disable/enable sequence.
   *   - onAreaNodeUnregistered(aArea, aContainer, aReason)
   *     Fired when an area node is explicitly unregistered by an API caller,
   *     or by a window closing. The aReason parameter indicates which of
   *     these is the case.
   */
  addListener(aListener: object): void;
  /**
   * Remove a listener added with addListener
   * @param aListener the listener object to remove
   */
  removeListener(aListener: object): void;

  /**
   * Register a customizable area with CustomizableUI.
   * @param aName   the name of the area to register. Can only contain
   *                alphanumeric characters, dashes (-) and underscores (_).
   * @param aProps  the properties of the area. The following properties are
   *                recognized:
   *                - type:   the type of area. Either TYPE_TOOLBAR (default) or
   *                          TYPE_PANEL;
   *                - anchor: for a menu panel or overflowable toolbar, the
   *                          anchoring node for the panel.
   *                - overflowable: set to true if your toolbar is overflowable.
   *                                This requires an anchor, and only has an
   *                                effect for toolbars.
   *                - defaultPlacements: an array of widget IDs making up the
   *                                     default contents of the area
   *                - defaultCollapsed: (INTERNAL ONLY) applies if the type is TYPE_TOOLBAR, specifies
   *                                    if toolbar is collapsed by default (default to true).
   *                                    Specify null to ensure that reset/inDefaultArea don't care
   *                                    about a toolbar's collapsed state
   */
  registerArea(
    aName: string,
    aProperties: {
      type: TCustomizableUIType;
      anchor: Node;
      overflowable: boolean;
      defaultPlacements: Array<string>;
      defaultCollapsed?: boolean;
    },
  ): void;
  /**
   * Register a concrete node for a registered area. This method needs to be called
   * with any toolbar in the main browser window that has its "customizable" attribute
   * set to true.
   *
   * Note that ideally, you should register your toolbar using registerArea
   * before calling this. If you don't, the node will be saved for processing when
   * you call registerArea. Note that CustomizableUI won't restore state in the area,
   * allow the user to customize it in customize mode, or otherwise deal
   * with it, until the area has been registered.
   */
  registerToolbarNode(aToolbar: XULElement): void;
  /**
   * Register a panel node. A panel treated slightly differently from a toolbar in
   * terms of what items can be moved into it. For example, a panel cannot have a
   * spacer or a spring put into it.
   *
   * @param aPanelContents the panel contents DOM node being registered.
   * @param aArea the area for which to register this node.
   */
  registerPanelNode(aNode: XULElement, aArea: TCustomizableUIArea): void;
  /**
   * Unregister a customizable area. The inverse of registerArea.
   *
   * Unregistering an area will remove all the (removable) widgets in the
   * area, which will return to the panel, and destroy all other traces
   * of the area within CustomizableUI. Note that this means the *contents*
   * of the area's DOM nodes will be moved to the panel or removed, but
   * the area's DOM nodes *themselves* will stay.
   *
   * Furthermore, by default the placements of the area will be kept in the
   * saved state (!) and restored if you re-register the area at a later
   * point. This is useful for e.g. add-ons that get disabled and then
   * re-enabled (e.g. when they update).
   *
   * You can override this last behaviour (and destroy the placements
   * information in the saved state) by passing true for aDestroyPlacements.
   *
   * @param aName              the name of the area to unregister
   * @param aDestroyPlacements whether to destroy the placements information
   *                           for the area, too.
   */
  unregisterArea(aName: string, aDestroyPlacements: boolean);
  /**
   * Add a widget to an area.
   * If the area to which you try to add is not known to CustomizableUI,
   * this will throw.
   * If the area to which you try to add is the same as the area in which
   * the widget is currently placed, this will do the same as
   * moveWidgetWithinArea.
   * If the widget cannot be removed from its original location, this will
   * no-op.
   *
   * This will fire an onWidgetAdded notification,
   * and an onWidgetBeforeDOMChange and onWidgetAfterDOMChange notification
   * for each window CustomizableUI knows about.
   *
   * @param aWidgetId the ID of the widget to add
   * @param aArea     the ID of the area to add the widget to
   * @param aPosition the position at which to add the widget. If you do not
   *                  pass a position, the widget will be added to the end
   *                  of the area.
   */
  addWidgetToArea(
    aWidgetId: string,
    aArea: TCustomizableUIArea,
    aPosition: number,
  ): void;
  /**
   * Remove a widget from its area. If the widget cannot be removed from its
   * area, or is not in any area, this will no-op. Otherwise, this will fire an
   * onWidgetRemoved notification, and an onWidgetBeforeDOMChange and
   * onWidgetAfterDOMChange notification for each window CustomizableUI knows
   * about.
   *
   * @param aWidgetId the ID of the widget to remove
   */
  removeWidgetFromArea(aWidgetId: string): void;
  /**
   * Move a widget within an area.
   * If the widget is not in any area, this will no-op.
   * If the widget is already at the indicated position, this will no-op.
   *
   * Otherwise, this will move the widget and fire an onWidgetMoved notification,
   * and an onWidgetBeforeDOMChange and onWidgetAfterDOMChange notification for
   * each window CustomizableUI knows about.
   *
   * @param aWidgetId the ID of the widget to move
   * @param aPosition the position to move the widget to.
   *                  Negative values or values greater than the number of
   *                  widgets will be interpreted to mean moving the widget to
   *                  respectively the first or last position.
   */
  moveWidgetWithinArea(aWidgetId: string, aPosition: number): void;
  /**
   * Ensure a XUL-based widget created in a window after areas were
   * initialized moves to its correct position.
   * Always prefer this over moving items in the DOM yourself.
   *
   * @param aWidgetId the ID of the widget that was just created
   * @param aWindow the window in which you want to ensure it was added.
   *
   * NB: why is this API per-window, you wonder? Because if you need this,
   * presumably you yourself need to create the widget in all the windows
   * and need to loop through them anyway.
   */
  ensureWidgetPlacedInWindow(aWidgetId: string, aWindow: Window): boolean;
  /**
   * Start a batch update of items.
   * During a batch update, the customization state is not saved to the user's
   * preferences file, in order to reduce (possibly sync) IO.
   * Calls to begin/endBatchUpdate may be nested.
   *
   * Callers should ensure that NO MATTER WHAT they call endBatchUpdate once
   * for each call to beginBatchUpdate, even if there are exceptions in the
   * code in the batch update. Otherwise, for the duration of the
   * Firefox session, customization state is never saved. Typically, you
   * would do this using a try...finally block.
   */
  beginBatchUpdate(): void;
  /**
   * End a batch update. See the documentation for beginBatchUpdate above.
   *
   * State is not saved if we believe it is identical to the last known
   * saved state. State is only ever saved when all batch updates have
   * finished (ie there has been 1 endBatchUpdate call for each
   * beginBatchUpdate call). If any of the endBatchUpdate calls pass
   * aForceDirty=true, we will flush to the prefs file.
   *
   * @param aForceDirty force CustomizableUI to flush to the prefs file when
   *                    all batch updates have finished.
   */
  endBatchUpdate(aForceDirty?: boolean): void;
  /**
   * Create a widget.
   *
   * To create a widget, you should pass an object with its desired
   * properties. The following properties are supported:
   *
   * - id:            the ID of the widget (required).
   * - type:          a string indicating the type of widget. Possible types
   *                  are:
   *                  'button' - for simple button widgets (the default)
   *                  'view'   - for buttons that open a panel or subview,
   *                             depending on where they are placed.
   *                  'button-and-view' - A combination of 'button' and 'view',
   *                             which looks different depending on whether it's
   *                             located in the toolbar or in the panel: When
   *                             located in the toolbar, the widget is shown as
   *                             a combined item of a button and a dropmarker
   *                             button. The button triggers the command and the
   *                             dropmarker button opens the view. When located
   *                             in the panel, shown as one item which opens the
   *                             view, and the button command cannot be
   *                             triggered separately.
   *                  'custom' - for fine-grained control over the creation
   *                             of the widget.
   * - viewId:        Only useful for views and button-and-view widgets (and
   *                  required there): the id of the <panelview> that should be
   *                  shown when clicking the widget.  If used with a custom
   *                  widget, the widget must also provide a toolbaritem where
   *                  the first child is the view button.
   * - onBuild(aDoc): Only useful for custom widgets (and required there); a
   *                  function that will be invoked with the document in which
   *                  to build a widget. Should return the DOM node that has
   *                  been constructed.
   * - onBeforeCreated(aDoc): Attached to all non-custom widgets; a function
   *                  that will be invoked before the widget gets a DOM node
   *                  constructed, passing the document in which that will happen.
   *                  This is useful especially for 'view' type widgets that need
   *                  to construct their views on the fly (e.g. from bootstrapped
   *                  add-ons). If the function returns `false`, the widget will
   *                  not be created.
   * - onCreated(aNode): Attached to all widgets; a function that will be invoked
   *                  whenever the widget has a DOM node constructed, passing the
   *                  constructed node as an argument.
   * - onDestroyed(aDoc): Attached to all non-custom widgets; a function that
   *                  will be invoked after the widget has a DOM node destroyed,
   *                  passing the document from which it was removed. This is
   *                  useful especially for 'view' type widgets that need to
   *                  cleanup after views that were constructed on the fly.
   * - onBeforeCommand(aEvt, aNode): A function that will be invoked when the user
   *                          activates the button but before the command
   *                          is evaluated. Useful if code needs to run to
   *                          change the button's icon in preparation to the
   *                          pending command action. Called for any type that
   *                          supports the handler.  The command type, either
   *                          "view" or "command", may be returned to force the
   *                          action that will occur.  View will open the panel
   *                          and command will result in calling onCommand.
   * - onCommand(aEvt): Useful for custom, button and button-and-view widgets; a
   *                    function that will be invoked when the user activates
   *                    the button. A custom widget with a view should
   *                    return "view" or "command" to continue processing
   *                    the command per the needs of the widget.
   * - onClick(aEvt): Attached to all widgets; a function that will be invoked
   *                  when the user clicks the widget.
   * - onViewShowing(aEvt): Only useful for views and button-and-view widgets; a
   *                  function that will be invoked when a user shows your view.
   *                  If any event handler calls aEvt.preventDefault(), the view
   *                  will not be shown.
   *
   *                  The event's `detail` property is an object with an
   *                  `addBlocker` method. Handlers which need to
   *                  perform asynchronous operations before the view is
   *                  shown may pass this method a Promise, which will
   *                  prevent the view from showing until it resolves.
   *                  Additionally, if the promise resolves to the exact
   *                  value `false`, the view will not be shown.
   * - onViewHiding(aEvt): Only useful for views; a function that will be
   *                  invoked when a user hides your view.
   * - l10nId:        fluent string identifier to use for localizing attributes
   *                  on the widget. If present, preferred over the
   *                  label/tooltiptext.
   * - tooltiptext:   string to use for the tooltip of the widget
   * - label:         string to use for the label of the widget
   * - localized:     If true, or undefined, attempt to retrieve the
   *                  widget's string properties from the customizable
   *                  widgets string bundle.
   * - removable:
   * - overflows:
   *
   * @param aProperties the specifications for the widget.
   * @return a wrapper around the created widget (see getWidget)
   */
  createWidget(
    aProperties:
      | {
          id: string;
          type: "button" | "view" | "button-and-view" | "custom";
          viewId?: string;

          onCreated?: (aNode: XULElement) => void;
          onBeforeCommand?: (aEvt: Event, aNode: XULElement) => boolean;
          onClick?: (aEvt: Event) => void;

          l10nId?: string;
          tooltiptext?: string | null;
          label?: string;
          localized?: boolean;
          /**
           * whether the widget is removable
           * NB: if you specify false here, you must provide a
           * defaultArea, too.
           * @default true
           */
          removable?: boolean;
          /**
           * whether widget can overflow when in an overflowable
           * toolbar
           * @default true
           */
          overflows?: boolean;
          /**
           * default area to add the widget to
           * (required if non-removable)
           * @default none
           */
          defaultArea?: TCustomizableUIArea;
          /**
           * id of an element that has a shortcut for this widget. This is only used to display
           * the shortcut as part of the tooltip for builtin widgets
           * (which have strings inside
           * customizableWidgets.properties). If you're in an add-on,
           * you should not set this property.
           * If l10nId is provided, the resulting shortcut is passed
           * as the "$shortcut" variable to the fluent message.
           * @default null
           */
          shortcutId?: string | null;
          /**
           * showInPrivateBrowsing: whether to show the widget in private browsing
           * mode
           * @default true
           */
          showInPrivateBrowsing?: boolean;
          /**
           * whether to hide the widget in non private
           * browsing mode windows
           * @default false
           */
          hideInNonPrivateBrowsing?: boolean;
          /**
           * If true, closes the panel if the tab changes.
           */
          tabSpecific?: boolean;
          /**
           * If true, closes the panel if the location changes.
           * This is similar to tabSpecific, but also if the location
           * changes in the same tab, we may want to close the panel.
           */
          locationSpecific?: boolean;
          /**
           * Set to true if this widget is being created on behalf of anextension.
           */
          webExtension?: boolean;
        }
      | {
          type: "custom";
          onBuild?: (aDoc: Document) => Node;
          onCommand?: (aEvt: Event) => void;
        }
      | {
          type: "button";
          onBeforeCreated?: (aDoc: Document) => boolean;
          onDestroyed?: (aDoc: Document) => void;
          onCommand?: (aEvt: Event) => void;
        }
      | {
          type: "view";
          onBeforeCreated?: (aDoc: Document) => boolean;
          onViewShowing: (aEvt: Event) => void;
          onDestroyed?: (aDoc: Document) => void;
          onViewShowing?: (aEvt: Event) => Promise<boolean>;
          onViewHiding?: (aEvt: Event) => void;
        }
      | {
          type: "button-and-view";
          onBeforeCreated?: (aDoc: Document) => boolean;
          onViewShowing?: (aEvt: Event) => void;
          onDestroyed?: (aDoc: Document) => void;
          onCommand?: (aEvt: Event) => void;
          onViewShowing: (aEvt: Event) => Promise<boolean>;
        },
  ): TWidgetGroupWrapper;
  /**
   * Destroy a widget
   *
   * If the widget is part of the default placements in an area, this will
   * remove it from there. It will also remove any DOM instances. However,
   * it will keep the widget in the placements for whatever area it was
   * in at the time. You can remove it from there yourself by calling
   * CustomizableUI.removeWidgetFromArea(aWidgetId).
   *
   * @param aWidgetId the ID of the widget to destroy
   */
  destroyWidget(aWidgetId: string): void;
  /**
   * Get a wrapper object with information about the widget.
   * The object provides the following properties
   * (all read-only unless otherwise indicated):
   *
   * - id:            the widget's ID;
   * - type:          the type of widget (button, view, custom). For
   *                  XUL-provided widgets, this is always 'custom';
   * - provider:      the provider type of the widget, id est one of
   *                  PROVIDER_API or PROVIDER_XUL;
   * - forWindow(w):  a method to obtain a single window wrapper for a widget,
   *                  in the window w passed as the only argument;
   * - instances:     an array of all instances (single window wrappers)
   *                  of the widget. This array is NOT live;
   * - areaType:      the type of the widget's current area
   * - isGroup:       true; will be false for wrappers around single widget nodes;
   * - source:        for API-provided widgets, whether they are built-in to
   *                  Firefox or add-on-provided;
   * - disabled:      for API-provided widgets, whether the widget is currently
   *                  disabled. NB: this property is writable, and will toggle
   *                  all the widgets' nodes' disabled states;
   * - label:         for API-provied widgets, the label of the widget;
   * - tooltiptext:   for API-provided widgets, the tooltip of the widget;
   * - showInPrivateBrowsing: for API-provided widgets, whether the widget is
   *                          visible in private browsing;
   * - hideInNonPrivateBrowsing: for API-provided widgets, whether the widget is
   *                             hidden in non-private browsing;
   *
   * Single window wrappers obtained through forWindow(someWindow) or from the
   * instances array have the following properties
   * (all read-only unless otherwise indicated):
   *
   * - id:            the widget's ID;
   * - type:          the type of widget (button, view, custom). For
   *                  XUL-provided widgets, this is always 'custom';
   * - provider:      the provider type of the widget, id est one of
   *                  PROVIDER_API or PROVIDER_XUL;
   * - node:          reference to the corresponding DOM node;
   * - anchor:        the anchor on which to anchor panels opened from this
   *                  node. This will point to the overflow chevron on
   *                  overflowable toolbars if and only if your widget node
   *                  is overflowed, to the anchor for the panel menu
   *                  if your widget is inside the panel menu, and to the
   *                  node itself in all other cases;
   * - overflowed:    boolean indicating whether the node is currently in the
   *                  overflow panel of the toolbar;
   * - isGroup:       false; will be true for the group widget;
   * - label:         for API-provided widgets, convenience getter for the
   *                  label attribute of the DOM node;
   * - tooltiptext:   for API-provided widgets, convenience getter for the
   *                  tooltiptext attribute of the DOM node;
   * - disabled:      for API-provided widgets, convenience getter *and setter*
   *                  for the disabled state of this single widget. Note that
   *                  you may prefer to use the group wrapper's getter/setter
   *                  instead.
   *
   * @param aWidgetId the ID of the widget whose information you need
   * @return a wrapper around the widget as described above, or null if the
   *         widget is known not to exist (anymore). NB: non-null return
   *         is no guarantee the widget exists because we cannot know in
   *         advance if a XUL widget exists or not.
   */
  getWidget(aWidgetId: string): TWidgetGroupWrapper;
  /**
   * Get an array of widget wrappers (see getWidget) for all the widgets
   * which are currently not in any area (so which are in the palette).
   *
   * @param aWindowPalette the palette (and by extension, the window) in which
   *                       CustomizableUI should look. This matters because of
   *                       course XUL-provided widgets could be available in
   *                       some windows but not others, and likewise
   *                       API-provided widgets might not exist in a private
   *                       window (because of the showInPrivateBrowsing
   *                       property).
   *
   * @return an array of widget wrappers (see getWidget)
   */
  getUnusedWidgets(aWindowPalette: Node): Array<string>;
  /**
   * Get an array of all the widget IDs placed in an area.
   * Modifying the array will not affect CustomizableUI.
   *
   * @param aArea the ID of the area whose placements you want to obtain.
   * @return an array containing the widget IDs that are in the area.
   *
   * NB: will throw if called too early (before placements have been fetched)
   *     or if the area is not currently known to CustomizableUI.
   */
  getWidgetIdsInArea(aArea: TCustomizableUIArea): Array<string>;
  /**
   * Get an array of widget wrappers for all the widgets in an area. This is
   * the same as calling getWidgetIdsInArea and .map() ing the result through
   * CustomizableUI.getWidget. Careful: this means that if there are IDs in there
   * which don't have corresponding DOM nodes, there might be nulls in this array,
   * or items for which wrapper.forWindow(win) will return null.
   *
   * @param aArea the ID of the area whose widgets you want to obtain.
   * @return an array of widget wrappers and/or null values for the widget IDs
   *         placed in an area.
   *
   * NB: will throw if called too early (before placements have been fetched)
   *     or if the area is not currently known to CustomizableUI.
   */
  getWidgetsInArea(
    aArea: TCustomizableUIArea,
  ): Array<TWidgetGroupWrapper | null>;
  /**
   * Ensure the customizable widget that matches up with this view node
   * will get the right subview showing/shown/hiding/hidden events when
   * they fire.
   * @param aViewNode the view node to add listeners to if they haven't
   *                  been added already.
   */
  ensureSubviewListeners(aViewNode: Node): void;
  /**
   * Obtain an array of all the area IDs known to CustomizableUI.
   * This array is created for you, so is modifiable without CustomizableUI
   * being affected.
   */
  get areas(): Array<string>;
  /**
   * Check what kind of area (toolbar or menu panel) an area is. This is
   * useful if you have a widget that needs to behave differently depending
   * on its location. Note that widget wrappers have a convenience getter
   * property (areaType) for this purpose.
   *
   * @param aArea the ID of the area whose type you want to know
   * @return TYPE_TOOLBAR or TYPE_PANEL depending on the area, null if
   *         the area is unknown.
   */
  getAreaType(aArea: TCustomizableUIArea): TCustomizableUIType;
  /**
   * Check if a toolbar is collapsed by default.
   *
   * @param aArea the ID of the area whose default-collapsed state you want to know.
   * @return `true` or `false` depending on the area, null if the area is unknown,
   *         or its collapsed state cannot normally be controlled by the user
   */
  isToolbarDefaultCollapsed(aArea: TCustomizableUIArea): boolean;
  /**
   * Obtain the DOM node that is the customize target for an area in a
   * specific window.
   *
   * Areas can have a customization target that does not correspond to the
   * node itself. In particular, toolbars that have a customizationtarget
   * attribute set will have their customization target set to that node.
   * This means widgets will end up in the customization target, not in the
   * DOM node with the ID that corresponds to the area ID. This is useful
   * because it lets you have fixed content in a toolbar (e.g. the panel
   * menu item in the navbar) and have all the customizable widgets use
   * the customization target.
   *
   * Using this API yourself is discouraged; you should generally not need
   * to be asking for the DOM container node used for a particular area.
   * In particular, if you're wanting to check it in relation to a widget's
   * node, your DOM node might not be a direct child of the customize target
   * in a window if, for instance, the window is in customization mode, or if
   * this is an overflowable toolbar and the widget has been overflowed.
   *
   * @param aArea   the ID of the area whose customize target you want to have
   * @param aWindow the window where you want to fetch the DOM node.
   * @return the customize target DOM node for aArea in aWindow
   */
  getCustomizeTargetForArea(aArea: TCustomizableUIArea, aWindow: Window): Node;
  /**
   * Reset the customization state back to its default.
   *
   * This is the nuclear option. You should never call this except if the user
   * explicitly requests it. Firefox does this when the user clicks the
   * "Restore Defaults" button in customize mode.
   */
  reset(): void;

  /**
   * Undo the previous reset, can only be called immediately after a reset.
   * @return a promise that will be resolved when the operation is complete.
   */
  undoReset(): void;

  /**
   * Remove a custom toolbar added in a previous version of Firefox or using
   * an add-on. NB: only works on the customizable toolbars generated by
   * the toolbox itself. Intended for use from CustomizeMode, not by
   * other consumers.
   * @param aToolbarId the ID of the toolbar to remove
   */
  removeExtraToolbar(aToolbarId: string): void;
  /**
   * Can the last Restore Defaults operation be undone.
   *
   * @return A boolean stating whether an undo of the
   *         Restore Defaults can be performed.
   */
  get canUndoReset(): boolean;

  /**
   * Get the placement of a widget. This is by far the best way to obtain
   * information about what the state of your widget is. The internals of
   * this call are cheap (no DOM necessary) and you will know where the user
   * has put your widget.
   *
   * @param aWidgetId the ID of the widget whose placement you want to know
   * @return
   *   {
   *     area: "somearea", // The ID of the area where the widget is placed
   *     position: 42 // the index in the placements array corresponding to
   *                  // your widget.
   *   }
   *
   *   OR
   *
   *   null // if the widget is not placed anywhere (ie in the palette)
   */
  getPlacementOfWidget(
    aWidgetId: string,
    aOnlyRegistered?: boolean,
    aDeadAreas?: boolean,
  ): { area: TCustomizableUIArea; position: number };
  /**
   * Check if a widget can be removed from the area it's in.
   *
   * Note that if you're wanting to move the widget somewhere, you should
   * generally be checking canWidgetMoveToArea, because that will return
   * true if the widget is already in the area where you want to move it (!).
   *
   * NB: oh, also, this method might lie if the widget in question is a
   *     XUL-provided widget and there are no windows open, because it
   *     can obviously not check anything in this case. It will return
   *     true. You will be able to move the widget elsewhere. However,
   *     once the user reopens a window, the widget will move back to its
   *     'proper' area automagically.
   *
   * @param aWidgetId a widget ID or DOM node to check
   * @return true if the widget can be removed from its area,
   *          false otherwise.
   */
  isWidgetRemovable(aWidgetId: string): boolean;
  /**
   * Check if a widget can be moved to a particular area. Like
   * isWidgetRemovable but better, because it'll return true if the widget
   * is already in the right area.
   *
   * @param aWidgetId the widget ID or DOM node you want to move somewhere
   * @param aArea     the area ID you want to move it to. This can also be
   *                  AREA_NO_AREA to see if the widget can move to the
   *                  customization palette, whether it's removable or not.
   * @return true if this is possible, false if it is not. The same caveats as
   *              for isWidgetRemovable apply, however, if no windows are open.
   */
  canWidgetMoveToArea(aWidgetId: string, aArea: TCustomizableUIArea): boolean;
  /**
   * Whether we're in a default state. Note that non-removable non-default
   * widgets and non-existing widgets are not taken into account in determining
   * whether we're in the default state.
   *
   * NB: this is a property with a getter. The getter is NOT cheap, because
   * it does smart things with non-removable non-default items, non-existent
   * items, and so forth. Please don't call unless necessary.
   */
  get inDefaultState(): boolean;

  /**
   * Set a toolbar's visibility state in all windows.
   * @param aToolbarId    the toolbar whose visibility should be adjusted
   * @param aIsVisible    whether the toolbar should be visible
   */
  setToolbarVisibility(aToolbarId: string, aIsVisible: boolean): void;

  /**
   * Returns a Set with the IDs of any registered toolbar areas that are
   * currently collapsed in a particular window. Menubars that are set to
   * autohide and are in the temporary "open" state are still considered
   * collapsed by default.
   *
   * @param {Window} window The browser window to check for collapsed toolbars.
   * @return {Set<string>}
   */
  getCollapsedToolbarIds(window: Window): Set<string>;

  /**
   * @deprecated Use fluent instead.
   *
   * Get a localized property off a (widget?) object.
   *
   * NB: this is unlikely to be useful unless you're in Firefox code, because
   *     this code uses the builtin widget stringbundle, and can't be told
   *     to use add-on-provided strings. It's mainly here as convenience for
   *     custom builtin widgets that build their own DOM but use the same
   *     stringbundle as the other builtin widgets.
   *
   * @param aWidget     the object whose property we should use to fetch a
   *                    localizable string;
   * @param aProp       the property on the object to use for the fetching;
   * @param aFormatArgs (optional) any extra arguments to use for a formatted
   *                    string;
   * @param aDef        (optional) the default to return if we don't find the
   *                    string in the stringbundle;
   *
   * @return the localized string, or aDef if the string isn't in the bundle.
   *         If no default is provided,
   *           if aProp exists on aWidget, we'll return that,
   *           otherwise we'll return the empty string
   *
   */
  getLocalizedProperty(aWidget, aProp, aFormatArgs, aDef): any;
  // /**
  //  * Utility function to detect, find and set a keyboard shortcut for a menuitem
  //  * or (toolbar)button.
  //  *
  //  * @param aShortcutNode the XUL node where the shortcut will be derived from;
  //  * @param aTargetNode   (optional) the XUL node on which the `shortcut`
  //  *                      attribute will be set. If NULL, the shortcut will be
  //  *                      set on aShortcutNode;
  //  */
  // addShortcut(aShortcutNode, aTargetNode) {
  //   return CustomizableUIInternal.addShortcut(aShortcutNode, aTargetNode);
  // },
  // /**
  //  * Given a node, walk up to the first panel in its ancestor chain, and
  //  * close it.
  //  *
  //  * @param aNode a node whose panel should be closed;
  //  */
  // hidePanelForNode(aNode) {
  //   CustomizableUIInternal.hidePanelForNode(aNode);
  // },
  // /**
  //  * Check if a widget is a "special" widget: a spring, spacer or separator.
  //  *
  //  * @param aWidgetId the widget ID to check.
  //  * @return true if the widget is 'special', false otherwise.
  //  */
  // isSpecialWidget(aWidgetId) {
  //   return CustomizableUIInternal.isSpecialWidget(aWidgetId);
  // },
  // /**
  //  * Check if a widget is provided by an extension. This effectively checks
  //  * whether `webExtension: true` passed when the widget was being created.
  //  *
  //  * If the widget being referred to hasn't yet been created, or has been
  //  * destroyed, we fallback to checking the ID for the "-browser-action"
  //  * suffix.
  //  *
  //  * @param aWidgetId the widget ID to check.
  //  * @return true if the widget was provided by an extension, false otherwise.
  //  */
  // isWebExtensionWidget(aWidgetId) {
  //   let widget = CustomizableUI.getWidget(aWidgetId);
  //   return widget?.webExtension || aWidgetId.endsWith("-browser-action");
  // },
  // /**
  //  * Add listeners to a panel that will close it. For use from the menu panel
  //  * and overflowable toolbar implementations, unlikely to be useful for
  //  * consumers.
  //  *
  //  * @param aPanel the panel to which listeners should be attached.
  //  */
  // addPanelCloseListeners(aPanel) {
  //   CustomizableUIInternal.addPanelCloseListeners(aPanel);
  // },
  // /**
  //  * Remove close listeners that have been added to a panel with
  //  * addPanelCloseListeners. For use from the menu panel and overflowable
  //  * toolbar implementations, unlikely to be useful for consumers.
  //  *
  //  * @param aPanel the panel from which listeners should be removed.
  //  */
  // removePanelCloseListeners(aPanel) {
  //   CustomizableUIInternal.removePanelCloseListeners(aPanel);
  // },
  // /**
  //  * Notify listeners a widget is about to be dragged to an area. For use from
  //  * Customize Mode only, do not use otherwise.
  //  *
  //  * @param aWidgetId the ID of the widget that is being dragged to an area.
  //  * @param aArea     the ID of the area to which the widget is being dragged.
  //  */
  // onWidgetDrag(aWidgetId, aArea) {
  //   CustomizableUIInternal.notifyListeners("onWidgetDrag", aWidgetId, aArea);
  // },
  // /**
  //  * Notify listeners that a window is entering customize mode. For use from
  //  * Customize Mode only, do not use otherwise.
  //  * @param aWindow the window entering customize mode
  //  */
  // notifyStartCustomizing(aWindow) {
  //   CustomizableUIInternal.notifyListeners("onCustomizeStart", aWindow);
  // },
  // /**
  //  * Notify listeners that a window is exiting customize mode. For use from
  //  * Customize Mode only, do not use otherwise.
  //  * @param aWindow the window exiting customize mode
  //  */
  // notifyEndCustomizing(aWindow) {
  //   CustomizableUIInternal.notifyListeners("onCustomizeEnd", aWindow);
  // },

  // /**
  //  * Notify toolbox(es) of a particular event. If you don't pass aWindow,
  //  * all toolboxes will be notified. For use from Customize Mode only,
  //  * do not use otherwise.
  //  * @param aEvent the name of the event to send.
  //  * @param aDetails optional, the details of the event.
  //  * @param aWindow optional, the window in which to send the event.
  //  */
  // dispatchToolboxEvent(aEvent, aDetails = {}, aWindow = null) {
  //   CustomizableUIInternal.dispatchToolboxEvent(aEvent, aDetails, aWindow);
  // },

  // /**
  //  * Check whether an area is overflowable.
  //  *
  //  * @param aAreaId the ID of an area to check for overflowable-ness
  //  * @return true if the area is overflowable, false otherwise.
  //  */
  // isAreaOverflowable(aAreaId) {
  //   let area = gAreas.get(aAreaId);
  //   return area
  //     ? area.get("type") == this.TYPE_TOOLBAR && area.get("overflowable")
  //     : false;
  // },
  // /**
  //  * Obtain a string indicating the place of an element. This is intended
  //  * for use from customize mode; You should generally use getPlacementOfWidget
  //  * instead, which is cheaper because it does not use the DOM.
  //  *
  //  * @param aElement the DOM node whose place we need to check
  //  * @return "toolbar" if the node is in a toolbar, "panel" if it is in the
  //  *         menu panel, "palette" if it is in the (visible!) customization
  //  *         palette, undefined otherwise.
  //  */
  // getPlaceForItem(aElement) {
  //   let place;
  //   let node = aElement;
  //   while (node && !place) {
  //     if (node.localName == "toolbar") {
  //       place = "toolbar";
  //     } else if (node.id == CustomizableUI.AREA_FIXED_OVERFLOW_PANEL) {
  //       place = "panel";
  //     } else if (node.id == "customization-palette") {
  //       place = "palette";
  //     }

  //     node = node.parentNode;
  //   }
  //   return place;
  // },

  // /**
  //  * Check if a toolbar is builtin or not.
  //  * @param aToolbarId the ID of the toolbar you want to check
  //  */
  // isBuiltinToolbar(aToolbarId) {
  //   return CustomizableUIInternal._builtinToolbars.has(aToolbarId);
  // },

  // /**
  //  * Create an instance of a spring, spacer or separator.
  //  * @param aId       the type of special widget (spring, spacer or separator)
  //  * @param aDocument the document in which to create it.
  //  */
  // createSpecialWidget(aId, aDocument) {
  //   return CustomizableUIInternal.createSpecialWidget(aId, aDocument);
  // },

  // /**
  //  * Fills a submenu with menu items.
  //  * @param aMenuItems the menu items to display.
  //  * @param aSubview   the subview to fill.
  //  */
  // fillSubviewFromMenuItems(aMenuItems, aSubview) {
  //   let attrs = [
  //     "oncommand",
  //     "onclick",
  //     "label",
  //     "key",
  //     "disabled",
  //     "command",
  //     "observes",
  //     "hidden",
  //     "class",
  //     "origin",
  //     "image",
  //     "checked",
  //     "style",
  //   ];

  //   // Use ownerGlobal.document to ensure we get the right doc even for
  //   // elements in template tags.
  //   let doc = aSubview.ownerGlobal.document;
  //   let fragment = doc.createDocumentFragment();
  //   for (let menuChild of aMenuItems) {
  //     if (menuChild.hidden) {
  //       continue;
  //     }

  //     let subviewItem;
  //     if (menuChild.localName == "menuseparator") {
  //       // Don't insert duplicate or leading separators. This can happen if there are
  //       // menus (which we don't copy) above the separator.
  //       if (
  //         !fragment.lastElementChild ||
  //         fragment.lastElementChild.localName == "toolbarseparator"
  //       ) {
  //         continue;
  //       }
  //       subviewItem = doc.createXULElement("toolbarseparator");
  //     } else if (menuChild.localName == "menuitem") {
  //       subviewItem = doc.createXULElement("toolbarbutton");
  //       CustomizableUI.addShortcut(menuChild, subviewItem);

  //       let item = menuChild;
  //       if (!item.hasAttribute("onclick")) {
  //         subviewItem.addEventListener("click", event => {
  //           const ClickEventConstructor = Services.prefs.getBoolPref(
  //             "dom.w3c_pointer_events.dispatch_click_as_pointer_event"
  //           )
  //             ? doc.ownerGlobal.PointerEvent
  //             : doc.ownerGlobal.MouseEvent;
  //           let newEvent = new ClickEventConstructor("click", event);

  //           // Telemetry should only pay attention to the original event.
  //           lazy.BrowserUsageTelemetry.ignoreEvent(newEvent);
  //           item.dispatchEvent(newEvent);
  //         });
  //       }

  //       if (!item.hasAttribute("oncommand")) {
  //         subviewItem.addEventListener("command", event => {
  //           let newEvent = doc.createEvent("XULCommandEvent");
  //           newEvent.initCommandEvent(
  //             event.type,
  //             event.bubbles,
  //             event.cancelable,
  //             event.view,
  //             event.detail,
  //             event.ctrlKey,
  //             event.altKey,
  //             event.shiftKey,
  //             event.metaKey,
  //             0,
  //             event.sourceEvent,
  //             0
  //           );

  //           // Telemetry should only pay attention to the original event.
  //           lazy.BrowserUsageTelemetry.ignoreEvent(newEvent);
  //           item.dispatchEvent(newEvent);
  //         });
  //       }
  //     } else {
  //       continue;
  //     }
  //     for (let attr of attrs) {
  //       let attrVal = menuChild.getAttribute(attr);
  //       if (attrVal) {
  //         subviewItem.setAttribute(attr, attrVal);
  //       }
  //     }
  //     // We do this after so the .subviewbutton class doesn't get overriden.
  //     if (menuChild.localName == "menuitem") {
  //       subviewItem.classList.add("subviewbutton");
  //     }

  //     // We make it possible to supply an alternative Fluent key when cloning
  //     // this menuitem into the AppMenu or panel contexts. This is because
  //     // we often use Title Case in menuitems in native menus, but want to use
  //     // Sentence case in the AppMenu / panels.
  //     let l10nId = menuChild.getAttribute("appmenu-data-l10n-id");
  //     if (l10nId) {
  //       doc.l10n.setAttributes(subviewItem, l10nId);
  //     }

  //     fragment.appendChild(subviewItem);
  //   }
  //   aSubview.appendChild(fragment);
  // },

  // /**
  //  * A helper function for clearing subviews.
  //  * @param aSubview the subview to clear.
  //  */
  // clearSubview(aSubview) {
  //   let parent = aSubview.parentNode;
  //   // We'll take the container out of the document before cleaning it out
  //   // to avoid reflowing each time we remove something.
  //   parent.removeChild(aSubview);

  //   while (aSubview.firstChild) {
  //     aSubview.firstChild.remove();
  //   }

  //   parent.appendChild(aSubview);
  // },

  // getCustomizationTarget(aElement) {
  //   return CustomizableUIInternal.getCustomizationTarget(aElement);
  // },

  // getTestOnlyInternalProp(aProp) {
  //   if (!Cu.isInAutomation) {
  //     return null;
  //   }
  //   switch (aProp) {
  //     case "CustomizableUIInternal":
  //       return CustomizableUIInternal;
  //     case "gAreas":
  //       return gAreas;
  //     case "gFuturePlacements":
  //       return gFuturePlacements;
  //     case "gPalette":
  //       return gPalette;
  //     case "gPlacements":
  //       return gPlacements;
  //     case "gSavedState":
  //       return gSavedState;
  //     case "gSeenWidgets":
  //       return gSeenWidgets;
  //     case "kVersion":
  //       return kVersion;
  //   }
  //   return null;
  // },
  // setTestOnlyInternalProp(aProp, aValue) {
  //   if (!Cu.isInAutomation) {
  //     return;
  //   }
  //   switch (aProp) {
  //     case "gSavedState":
  //       gSavedState = aValue;
  //       break;
  //     case "kVersion":
  //       kVersion = aValue;
  //       break;
  //     case "gDirty":
  //       gDirty = aValue;
  //       break;
  //   }
  // },
};
