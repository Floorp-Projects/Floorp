/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Allows a popup panel to host multiple subviews. The main view shown when the
 * panel is opened may slide out to display a subview, which in turn may lead to
 * other subviews in a cascade menu pattern.
 *
 * The <panel> element should contain a <panelmultiview> element. Views are
 * declared using <panelview> elements that are usually children of the main
 * <panelmultiview> element, although they don't need to be, as views can also
 * be imported into the panel from other panels or popup sets.
 *
 * The panel should be opened asynchronously using the openPopup static method
 * on the PanelMultiView object. This will display the view specified using the
 * mainViewId attribute on the contained <panelmultiview> element.
 *
 * Specific subviews can slide in using the showSubView method, and backwards
 * navigation can be done using the goBack method or through a button in the
 * subview headers.
 *
 * The process of displaying the main view or a new subview requires multiple
 * steps to be completed, hence at any given time the <panelview> element may
 * be in different states:
 *
 * -- Open or closed
 *
 *    All the <panelview> elements start "closed", meaning that they are not
 *    associated to a <panelmultiview> element and can be located anywhere in
 *    the document. When the openPopup or showSubView methods are called, the
 *    relevant view becomes "open" and the <panelview> element may be moved to
 *    ensure it is a descendant of the <panelmultiview> element.
 *
 *    The "ViewShowing" event is fired at this point, when the view is not
 *    visible yet. The event is allowed to cancel the operation, in which case
 *    the view is closed immediately.
 *
 *    Closing the view does not move the node back to its original position.
 *
 * -- Visible or invisible
 *
 *    This indicates whether the view is visible in the document from a layout
 *    perspective, regardless of whether it is currently scrolled into view. In
 *    fact, all subviews are already visible before they start sliding in.
 *
 *    Before scrolling into view, a view may become visible but be placed in a
 *    special off-screen area of the document where layout and measurements can
 *    take place asyncronously.
 *
 *    When navigating forward, an open view may become invisible but stay open
 *    after sliding out of view. The last known size of these views is still
 *    taken into account for determining the overall panel size.
 *
 *    When navigating backwards, an open subview will first become invisible and
 *    then will be closed.
 *
 * -- Navigating with the keyboard
 *
 *    An open view may keep state related to keyboard navigation, even if it is
 *    invisible. When a view is closed, keyboard navigation state is cleared.
 *
 * This diagram shows how <panelview> nodes move during navigation:
 *
 *   In this <panelmultiview>     In other panels    Action
 *             ┌───┬───┬───┐        ┌───┬───┐
 *             │(A)│ B │ C │        │ D │ E │          Open panel
 *             └───┴───┴───┘        └───┴───┘
 *         ┌───┬───┬───┐            ┌───┬───┐
 *         │{A}│(C)│ B │            │ D │ E │          Show subview C
 *         └───┴───┴───┘            └───┴───┘
 *     ┌───┬───┬───┬───┐            ┌───┐
 *     │{A}│{C}│(D)│ B │            │ E │              Show subview D
 *     └───┴───┴───┴───┘            └───┘
 *       │ ┌───┬───┬───┬───┐        ┌───┐
 *       │ │{A}│(C)│ D │ B │        │ E │              Go back
 *       │ └───┴───┴───┴───┘        └───┘
 *       │   │   │
 *       │   │   └── Currently visible view
 *       │   │   │
 *       └───┴───┴── Open views
 *
 * If the <panelmultiview> element is "ephemeral", imported subviews will be
 * moved out again to the element specified by the viewCacheId attribute, so
 * that the panel element can be removed safely.
 */

"use strict";

this.EXPORTED_SYMBOLS = [
  "PanelMultiView",
  "PanelView",
];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "BrowserUtils",
  "resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.defineModuleGetter(this, "CustomizableUI",
  "resource:///modules/CustomizableUI.jsm");

/**
 * Safety timeout after which asynchronous events will be canceled if any of the
 * registered blockers does not return.
 */
const BLOCKERS_TIMEOUT_MS = 10000;

const TRANSITION_PHASES = Object.freeze({
  START: 1,
  PREPARE: 2,
  TRANSITION: 3,
  END: 4
});

let gNodeToObjectMap = new WeakMap();
let gMultiLineElementsMap = new WeakMap();

/**
 * Allows associating an object to a node lazily using a weak map.
 *
 * Classes deriving from this one may be easily converted to Custom Elements,
 * although they would lose the ability of being associated lazily.
 */
this.AssociatedToNode = class {
  constructor(node) {
    /**
     * Node associated to this object.
     */
    this.node = node;

    /**
     * This promise is resolved when the current set of blockers set by event
     * handlers have all been processed.
     */
    this._blockersPromise = Promise.resolve();
  }

  /**
   * Retrieves the instance associated with the given node, constructing a new
   * one if necessary. When the last reference to the node is released, the
   * object instance will be garbage collected as well.
   */
  static forNode(node) {
    let associatedToNode = gNodeToObjectMap.get(node);
    if (!associatedToNode) {
      associatedToNode = new this(node);
      gNodeToObjectMap.set(node, associatedToNode);
    }
    return associatedToNode;
  }

  get document() {
    return this.node.ownerDocument;
  }

  get window() {
    return this.node.ownerGlobal;
  }

  /**
   * nsIDOMWindowUtils for the window of this node.
   */
  get _dwu() {
    if (this.__dwu)
      return this.__dwu;
    return this.__dwu = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                                   .getInterface(Ci.nsIDOMWindowUtils);
  }

  /**
   * Dispatches a custom event on this element.
   *
   * @param  {String}    eventName Name of the event to dispatch.
   * @param  {Object}    [detail]  Event detail object. Optional.
   * @param  {Boolean}   cancelable If the event can be canceled.
   * @return {Boolean} `true` if the event was canceled by an event handler, `false`
   *                   otherwise.
   */
  dispatchCustomEvent(eventName, detail, cancelable = false) {
    let event = new this.window.CustomEvent(eventName, {
      detail,
      bubbles: true,
      cancelable,
    });
    this.node.dispatchEvent(event);
    return event.defaultPrevented;
  }

  /**
   * Dispatches a custom event on this element and waits for any blocking
   * promises registered using the "addBlocker" function on the details object.
   * If this function is called again, the event is only dispatched after all
   * the previously registered blockers have returned.
   *
   * The event can be canceled either by resolving any blocking promise to the
   * boolean value "false" or by calling preventDefault on the event. Rejections
   * and exceptions will be reported and will cancel the event.
   *
   * Blocking should be used sporadically because it slows down the interface.
   * Also, non-reentrancy is not strictly guaranteed because a safety timeout of
   * BLOCKERS_TIMEOUT_MS is implemented, after which the event will be canceled.
   * This helps to prevent deadlocks if any of the event handlers does not
   * resolve a blocker promise.
   *
   * @note Since there is no use case for dispatching different asynchronous
   *       events in parallel for the same element, this function will also wait
   *       for previous blockers when the event name is different.
   *
   * @param eventName
   *        Name of the custom event to dispatch.
   *
   * @resolves True if the event was canceled by a handler, false otherwise.
   */
  async dispatchAsyncEvent(eventName) {
    // Wait for all the previous blockers before dispatching the event.
    let blockersPromise = this._blockersPromise.catch(() => {});
    return this._blockersPromise = blockersPromise.then(async () => {
      let blockers = new Set();
      let cancel = this.dispatchCustomEvent(eventName, {
        addBlocker(promise) {
          // Any exception in the blocker will cancel the operation.
          blockers.add(promise.catch(ex => {
            Cu.reportError(ex);
            return true;
          }));
        },
      }, true);
      if (blockers.size) {
        let timeoutPromise = new Promise((resolve, reject) => {
          this.window.setTimeout(reject, BLOCKERS_TIMEOUT_MS);
        });
        try {
          let results = await Promise.race([Promise.all(blockers),
                                            timeoutPromise]);
          cancel = cancel || results.some(result => result === false);
        } catch (ex) {
          Cu.reportError(new Error(
            `One of the blockers for ${eventName} timed out.`));
          return true;
        }
      }
      return cancel;
    });
  }
};

/**
 * This is associated to <panelmultiview> elements by the panelUI.xml binding.
 */
this.PanelMultiView = class extends this.AssociatedToNode {
  /**
   * Tries to open the specified <panel> and displays the main view specified
   * with the "mainViewId" attribute on the <panelmultiview> node it contains.
   *
   * If the panel does not contain a <panelmultiview>, it is opened directly.
   * This allows consumers like page actions to accept different panel types.
   *
   * @see The non-static openPopup method for details.
   */
  static async openPopup(panelNode, ...args) {
    let panelMultiViewNode = panelNode.querySelector("panelmultiview");
    if (panelMultiViewNode) {
      return this.forNode(panelMultiViewNode).openPopup(...args);
    }
    panelNode.openPopup(...args);
    return true;
  }

  /**
   * Closes the specified <panel> which contains a <panelmultiview> node.
   *
   * If the panel does not contain a <panelmultiview>, it is closed directly.
   * This allows consumers like page actions to accept different panel types.
   *
   * @see The non-static hidePopup method for details.
   */
  static hidePopup(panelNode) {
    let panelMultiViewNode = panelNode.querySelector("panelmultiview");
    if (panelMultiViewNode) {
      this.forNode(panelMultiViewNode).hidePopup();
    } else {
      panelNode.hidePopup();
    }
  }

  get _panel() {
    return this.node.parentNode;
  }

  get _mainViewId() {
    return this.node.getAttribute("mainViewId");
  }
  get _mainView() {
    return this.document.getElementById(this._mainViewId);
  }

  get _transitioning() {
    return this.__transitioning;
  }
  set _transitioning(val) {
    this.__transitioning = val;
    if (val) {
      this.node.setAttribute("transitioning", "true");
    } else {
      this.node.removeAttribute("transitioning");
    }
  }

  /**
   * @return {Boolean} |true| when the 'ephemeral' attribute is set, which means
   *                   that this instance should be ready to be thrown away at
   *                   any time.
   */
  get _ephemeral() {
    return this.node.hasAttribute("ephemeral");
  }

  get _screenManager() {
    if (this.__screenManager)
      return this.__screenManager;
    return this.__screenManager = Cc["@mozilla.org/gfx/screenmanager;1"]
                                    .getService(Ci.nsIScreenManager);
  }
  /**
   * @return {panelview} the currently visible subview OR the subview that is
   *                     about to be shown whilst a 'ViewShowing' event is being
   *                     dispatched.
   */
  get current() {
    return this.node && this._currentSubView;
  }
  get _currentSubView() {
    // Peek the top of the stack, but fall back to the main view if the list of
    // opened views is currently empty.
    let panelView = this.openViews[this.openViews.length - 1];
    return (panelView && panelView.node) || this._mainView;
  }
  get showingSubView() {
    return this.openViews.length > 1;
  }

  constructor(node) {
    super(node);
    this._openPopupPromise = Promise.resolve(false);
    this._openPopupCancelCallback = () => {};
  }

  connect() {
    this.connected = true;
    this.openViews = [];
    this.__transitioning = false;

    const {document, window} = this;

    this._viewContainer =
      document.getAnonymousElementByAttribute(this.node, "anonid", "viewContainer");
    this._viewStack =
      document.getAnonymousElementByAttribute(this.node, "anonid", "viewStack");
    this._offscreenViewStack =
      document.getAnonymousElementByAttribute(this.node, "anonid", "offscreenViewStack");

    XPCOMUtils.defineLazyGetter(this, "_panelViewCache", () => {
      let viewCacheId = this.node.getAttribute("viewCacheId");
      return viewCacheId ? document.getElementById(viewCacheId) : null;
    });

    this._panel.addEventListener("popupshowing", this);
    this._panel.addEventListener("popuppositioned", this);
    this._panel.addEventListener("popuphidden", this);
    this._panel.addEventListener("popupshown", this);
    let cs = window.getComputedStyle(document.documentElement);
    // Set CSS-determined attributes now to prevent a layout flush when we do
    // it when transitioning between panels.
    this._dir = cs.direction;

    // Proxy these public properties and methods, as used elsewhere by various
    // parts of the browser, to this instance.
    ["goBack", "showSubView"].forEach(method => {
      Object.defineProperty(this.node, method, {
        enumerable: true,
        value: (...args) => this[method](...args)
      });
    });
    ["current", "showingSubView"].forEach(property => {
      Object.defineProperty(this.node, property, {
        enumerable: true,
        get: () => this[property]
      });
    });
  }

  destructor() {
    // Guard against re-entrancy.
    if (!this.node)
      return;

    this._cleanupTransitionPhase();
    let mainView = this._mainView;
    if (mainView) {
      if (this._panelViewCache)
        this._panelViewCache.appendChild(mainView);
      mainView.removeAttribute("mainview");
    }

    this._moveOutKids(this._viewStack);
    this._panel.removeEventListener("mousemove", this);
    this._panel.removeEventListener("popupshowing", this);
    this._panel.removeEventListener("popuppositioned", this);
    this._panel.removeEventListener("popupshown", this);
    this._panel.removeEventListener("popuphidden", this);
    this.window.removeEventListener("keydown", this);
    this.node = this._openPopupPromise = this._openPopupCancelCallback =
      this._viewContainer = this._viewStack = this.__dwu =
      this._panelViewCache = this._transitionDetails = null;
  }

  /**
   * Tries to open the panel associated with this PanelMultiView, and displays
   * the main view specified with the "mainViewId" attribute.
   *
   * The hidePopup method can be called while the operation is in progress to
   * prevent the panel from being displayed. View events may also cancel the
   * operation, so there is no guarantee that the panel will become visible.
   *
   * The "popuphidden" event will be fired either when the operation is canceled
   * or when the popup is closed later. This event can be used for example to
   * reset the "open" state of the anchor or tear down temporary panels.
   *
   * If this method is called again before the panel is shown, the result
   * depends on the operation currently in progress. If the operation was not
   * canceled, the panel is opened using the arguments from the previous call,
   * and this call is ignored. If the operation was canceled, it will be
   * retried again using the arguments from this call.
   *
   * It's not necessary for the <panelmultiview> binding to be connected when
   * this method is called, but the containing panel must have its display
   * turned on, for example it shouldn't have the "hidden" attribute.
   *
   * @param args
   *        Arguments to be forwarded to the openPopup method of the panel.
   *
   * @resolves With true as soon as the request to display the panel has been
   *           sent, or with false if the operation was canceled. The state of
   *           the panel at this point is not guaranteed. It may be still
   *           showing, completely shown, or completely hidden.
   * @rejects If an exception is thrown at any point in the process before the
   *          request to display the panel is sent.
   */
  async openPopup(...args) {
    // Set up the function that allows hidePopup or a second call to showPopup
    // to cancel the specific panel opening operation that we're starting below.
    // This function must be synchronous, meaning we can't use Promise.race,
    // because hidePopup wants to dispatch the "popuphidden" event synchronously
    // even if the panel has not been opened yet.
    let canCancel = true;
    let cancelCallback = this._openPopupCancelCallback = () => {
      // If the cancel callback is called and the panel hasn't been prepared
      // yet, cancel showing it. Setting canCancel to false will prevent the
      // popup from opening. If the panel has opened by the time the cancel
      // callback is called, canCancel will be false already, and we will not
      // fire the "popuphidden" event.
      if (canCancel && this.node) {
        canCancel = false;
        this.dispatchCustomEvent("popuphidden");
      }
    };

    // Create a promise that is resolved with the result of the last call to
    // this method, where errors indicate that the panel was not opened.
    let openPopupPromise = this._openPopupPromise.catch(() => {
      return false;
    });

    // Make the preparation done before showing the panel non-reentrant. The
    // promise created here will be resolved only after the panel preparation is
    // completed, even if a cancellation request is received in the meantime.
    return this._openPopupPromise = openPopupPromise.then(async wasShown => {
      // The panel may have been destroyed in the meantime.
      if (!this.node) {
        return false;
      }
      // If the panel has been already opened there is nothing more to do. We
      // check the actual state of the panel rather than setting some state in
      // our handler of the "popuphidden" event because this has a lower chance
      // of locking indefinitely if events aren't raised in the expected order.
      if (wasShown && ["open", "showing"].includes(this._panel.state)) {
        return true;
      }
      try {
        // Most of the panel elements in the browser window have their display
        // turned off for performance reasons, typically by setting the "hidden"
        // attribute. If the caller has just turned on the display, the XBL
        // binding for the <panelmultiview> element may still be disconnected.
        // In this case, give the layout code a chance to run.
        if (!this.connected) {
          await BrowserUtils.promiseLayoutFlushed(this.document, "layout",
                                                  () => {});
          // The XBL binding must be connected at this point. If this is not the
          // case, the calling code should be updated to unhide the panel.
          if (!this.connected) {
            throw new Error("The binding for the panelmultiview element isn't" +
                            " connected. The containing panel may still have" +
                            " its display turned off by the hidden attribute.");
          }
        }
        // Allow any of the ViewShowing handlers to prevent showing the main view.
        if (!(await this._showMainView())) {
          cancelCallback();
        }
      } catch (ex) {
        cancelCallback();
        throw ex;
      }
      // If a cancellation request was received there is nothing more to do.
      if (!canCancel || !this.node) {
        return false;
      }
      // We have to set canCancel to false before opening the popup because the
      // hidePopup method of PanelMultiView can be re-entered by event handlers.
      // If the openPopup call fails, however, we still have to dispatch the
      // "popuphidden" event even if canCancel was set to false.
      try {
        canCancel = false;
        this._panel.openPopup(...args);
        return true;
      } catch (ex) {
        this.dispatchCustomEvent("popuphidden");
        throw ex;
      }
    });
  }

  /**
   * Closes the panel associated with this PanelMultiView.
   *
   * If the openPopup method was called but the panel has not been displayed
   * yet, the operation is canceled and the panel will not be displayed, but the
   * "popuphidden" event is fired synchronously anyways.
   *
   * This means that by the time this method returns all the operations handled
   * by the "popuphidden" event are completed, for example resetting the "open"
   * state of the anchor, and the panel is already invisible.
   */
  hidePopup() {
    if (!this.node || !this.connected) {
      return;
    }

    // If we have already reached the _panel.openPopup call in the openPopup
    // method, we can call hidePopup. Otherwise, we have to cancel the latest
    // request to open the panel, which will have no effect if the request has
    // been canceled already.
    if (["open", "showing"].includes(this._panel.state)) {
      this._panel.hidePopup();
    } else {
      this._openPopupCancelCallback();
    }

    // We close all the views synchronously, so that they are ready to be opened
    // in other PanelMultiView instances. The "popuphidden" handler may also
    // call this function, but the second time openViews will be empty.
    this.closeAllViews();
  }

  /**
   * Remove any child subviews into the panelViewCache, to ensure
   * they remain usable even if this panelmultiview instance is removed
   * from the DOM.
   * @param viewNodeContainer the container from which to remove subviews
   */
  _moveOutKids(viewNodeContainer) {
    if (!this._panelViewCache)
      return;

    // Node.children and Node.childNodes is live to DOM changes like the
    // ones we're about to do, so iterate over a static copy:
    let subviews = Array.from(viewNodeContainer.childNodes);
    for (let subview of subviews) {
      // XBL lists the 'children' XBL element explicitly. :-(
      if (subview.nodeName != "children")
        this._panelViewCache.appendChild(subview);
    }
  }

  /**
   * Slides in the specified view as a subview.
   *
   * @param viewIdOrNode
   *        DOM element or string ID of the <panelview> to display.
   * @param anchor
   *        DOM element that triggered the subview, which will be highlighted
   *        and whose "label" attribute will be used for the title of the
   *        subview when a "title" attribute is not specified.
   */
  showSubView(viewIdOrNode, anchor) {
    this._showSubView(viewIdOrNode, anchor).catch(Cu.reportError);
  }
  async _showSubView(viewIdOrNode, anchor) {
    let viewNode = typeof viewIdOrNode == "string" ?
                   this.document.getElementById(viewIdOrNode) : viewIdOrNode;
    if (!viewNode) {
      Cu.reportError(new Error(`Subview ${viewIdOrNode} doesn't exist.`));
      return;
    }

    let prevPanelView = this.openViews[this.openViews.length - 1];
    let nextPanelView = PanelView.forNode(viewNode);
    if (this.openViews.includes(nextPanelView)) {
      Cu.reportError(new Error(`Subview ${viewNode.id} is already open.`));
      return;
    }
    if (!(await this._openView(nextPanelView))) {
      return;
    }

    prevPanelView.captureKnownSize();

    // The main view of a panel can be a subview in another one. Make sure to
    // reset all the properties that may be set on a subview.
    nextPanelView.mainview = false;
    // The header may change based on how the subview was opened.
    nextPanelView.headerText = viewNode.getAttribute("title") ||
                               (anchor && anchor.getAttribute("label"));
    // The constrained width of subviews may also vary between panels.
    nextPanelView.minMaxWidth = prevPanelView.knownWidth;

    if (anchor) {
      viewNode.classList.add("PanelUI-subView");
    }

    await this._transitionViews(prevPanelView.node, viewNode, false, anchor);
    this._viewShown(nextPanelView);
  }

  /**
   * Navigates backwards by sliding out the most recent subview.
   */
  goBack() {
    this._goBack().catch(Cu.reportError);
  }
  async _goBack() {
    if (this.openViews.length < 2) {
      // This may be called by keyboard navigation or external code when only
      // the main view is open.
      return;
    }

    let prevPanelView = this.openViews[this.openViews.length - 1];
    let nextPanelView = this.openViews[this.openViews.length - 2];

    prevPanelView.captureKnownSize();
    await this._transitionViews(prevPanelView.node, nextPanelView.node, true);

    this._closeLatestView();

    this._viewShown(nextPanelView);
  }

  /**
   * Prepares the main view before showing the panel.
   */
  async _showMainView() {
    if (!this.node || !this._mainViewId) {
      return false;
    }

    let nextPanelView = PanelView.forNode(this._mainView);

    // If the view is already open in another panel, close the panel first.
    let oldPanelMultiViewNode = nextPanelView.node.panelMultiView;
    if (oldPanelMultiViewNode) {
      PanelMultiView.forNode(oldPanelMultiViewNode).hidePopup();
    }

    if (!(await this._openView(nextPanelView))) {
      return false;
    }

    // The main view of a panel can be a subview in another one. Make sure to
    // reset all the properties that may be set on a subview.
    nextPanelView.mainview = true;
    nextPanelView.headerText = "";
    nextPanelView.minMaxWidth = 0;

    await this._cleanupTransitionPhase();
    nextPanelView.visible = true;
    nextPanelView.descriptionHeightWorkaround();

    this._viewShown(nextPanelView);
    return true;
  }

  /**
   * Opens the specified PanelView and dispatches the ViewShowing event, which
   * can be used to populate the subview or cancel the operation.
   *
   * @resolves With true if the view was opened, false otherwise.
   */
  async _openView(panelView) {
    if (panelView.node.parentNode != this._viewStack) {
      this._viewStack.appendChild(panelView.node);
    }

    panelView.node.panelMultiView = this.node;
    this.openViews.push(panelView);

    let canceled = await panelView.dispatchAsyncEvent("ViewShowing");

    // The panel can be hidden while we are processing the ViewShowing event.
    // This results in all the views being closed synchronously, and at this
    // point the ViewHiding event has already been dispatched for all of them.
    if (!this.openViews.length) {
      return false;
    }

    // Check if the event requested cancellation but the panel is still open.
    if (canceled) {
      // Handlers for ViewShowing can't know if a different handler requested
      // cancellation, so this will dispatch a ViewHiding event to give a chance
      // to clean up.
      this._closeLatestView();
      return false;
    }

    return true;
  }

  /**
   * Raises the ViewShown event if the specified view is still open.
   */
  _viewShown(panelView) {
    if (panelView.node.panelMultiView == this.node) {
      panelView.dispatchCustomEvent("ViewShown");
    }
  }

  /**
   * Closes the most recent PanelView and raises the ViewHiding event.
   *
   * @note The ViewHiding event is not cancelable and should probably be renamed
   *       to ViewHidden or ViewClosed instead, see bug 1438507.
   */
  _closeLatestView() {
    let panelView = this.openViews.pop();
    panelView.clearNavigation();
    panelView.dispatchCustomEvent("ViewHiding");
    panelView.node.panelMultiView = null;
    // Views become invisible synchronously when they are closed, and they won't
    // become visible again until they are opened.
    panelView.visible = false;
  }

  /**
   * Closes all the views that are currently open.
   */
  closeAllViews() {
    // Raise ViewHiding events for open views in reverse order.
    while (this.openViews.length) {
      this._closeLatestView();
    }
  }

  /**
   * Apply a transition to 'slide' from the currently active view to the next
   * one.
   * Sliding the next subview in means that the previous panelview stays where it
   * is and the active panelview slides in from the left in LTR mode, right in
   * RTL mode.
   *
   * @param {panelview} previousViewNode Node that is currently shown as active,
   *                                     but is about to be transitioned away.
   * @param {panelview} viewNode         Node that will becode the active view,
   *                                     after the transition has finished.
   * @param {Boolean}   reverse          Whether we're navigation back to a
   *                                     previous view or forward to a next view.
   * @param {Element}   anchor           the anchor for which we're opening
   *                                     a new panelview, if any
   */
  async _transitionViews(previousViewNode, viewNode, reverse, anchor) {
    // Clean up any previous transition that may be active at this point.
    await this._cleanupTransitionPhase();

    // There's absolutely no need to show off our epic animation skillz when
    // the panel's not even open.
    if (this._panel.state != "open") {
      return;
    }

    const {window, document} = this;

    let nextPanelView = PanelView.forNode(viewNode);
    let prevPanelView = PanelView.forNode(previousViewNode);

    if (this._autoResizeWorkaroundTimer)
      window.clearTimeout(this._autoResizeWorkaroundTimer);

    let details = this._transitionDetails = {
      phase: TRANSITION_PHASES.START,
      previousViewNode, viewNode, reverse, anchor
    };

    if (anchor)
      anchor.setAttribute("open", "true");

    // Since we're going to show two subview at the same time, don't abuse the
    // 'current' attribute, since it's needed for other state-keeping, but use
    // a separate 'in-transition' attribute instead.
    previousViewNode.setAttribute("in-transition", true);
    // Set the viewContainer dimensions to make sure only the current view is
    // visible.
    let olderView = reverse ? nextPanelView : prevPanelView;
    this._viewContainer.style.minHeight = olderView.knownHeight + "px";
    this._viewContainer.style.height = prevPanelView.knownHeight + "px";
    this._viewContainer.style.width = prevPanelView.knownWidth + "px";
    // Lock the dimensions of the window that hosts the popup panel.
    let rect = this._panel.popupBoxObject.getOuterScreenRect();
    this._panel.setAttribute("width", rect.width);
    this._panel.setAttribute("height", rect.height);

    let viewRect;
    if (reverse) {
      // Use the cached size when going back to a previous view, but not when
      // reopening a subview, because its contents may have changed.
      viewRect = { width: nextPanelView.knownWidth,
                   height: nextPanelView.knownHeight };
      viewNode.setAttribute("in-transition", true);
    } else if (viewNode.customRectGetter) {
      // Can't use Object.assign directly with a DOM Rect object because its properties
      // aren't enumerable.
      let width = prevPanelView.knownWidth;
      let height = prevPanelView.knownHeight;
      viewRect = Object.assign({height, width}, viewNode.customRectGetter());
      let header = viewNode.firstChild;
      if (header && header.classList.contains("panel-header")) {
        viewRect.height += this._dwu.getBoundsWithoutFlushing(header).height;
      }
      viewNode.setAttribute("in-transition", true);
    } else {
      let oldSibling = viewNode.nextSibling || null;
      this._offscreenViewStack.style.minHeight = olderView.knownHeight + "px";
      this._offscreenViewStack.appendChild(viewNode);
      viewNode.setAttribute("in-transition", true);

      // Now that the subview is visible, we can check the height of the
      // description elements it contains.
      nextPanelView.descriptionHeightWorkaround();

      viewRect = await BrowserUtils.promiseLayoutFlushed(this.document, "layout", () => {
        return this._dwu.getBoundsWithoutFlushing(viewNode);
      });

      try {
        this._viewStack.insertBefore(viewNode, oldSibling);
      } catch (ex) {
        this._viewStack.appendChild(viewNode);
      }

      this._offscreenViewStack.style.removeProperty("min-height");
    }

    this._transitioning = true;
    details.phase = TRANSITION_PHASES.PREPARE;

    // The 'magic' part: build up the amount of pixels to move right or left.
    let moveToLeft = (this._dir == "rtl" && !reverse) || (this._dir == "ltr" && reverse);
    let deltaX = prevPanelView.knownWidth;
    let deepestNode = reverse ? previousViewNode : viewNode;

    // With a transition when navigating backwards - user hits the 'back'
    // button - we need to make sure that the views are positioned in a way
    // that a translateX() unveils the previous view from the right direction.
    if (reverse)
      this._viewStack.style.marginInlineStart = "-" + deltaX + "px";

    // Set the transition style and listen for its end to clean up and make sure
    // the box sizing becomes dynamic again.
    // Somehow, putting these properties in PanelUI.css doesn't work for newly
    // shown nodes in a XUL parent node.
    this._viewStack.style.transition = "transform var(--animation-easing-function)" +
      " var(--panelui-subview-transition-duration)";
    this._viewStack.style.willChange = "transform";
    // Use an outline instead of a border so that the size is not affected.
    deepestNode.style.outline = "1px solid var(--panel-separator-color)";

    // Now set the viewContainer dimensions to that of the new view, which
    // kicks of the height animation.
    this._viewContainer.style.height = viewRect.height + "px";
    this._viewContainer.style.width = viewRect.width + "px";
    this._panel.removeAttribute("width");
    this._panel.removeAttribute("height");
    // We're setting the width property to prevent flickering during the
    // sliding animation with smaller views.
    viewNode.style.width = viewRect.width + "px";

    await BrowserUtils.promiseLayoutFlushed(document, "layout", () => {});

    // Kick off the transition!
    details.phase = TRANSITION_PHASES.TRANSITION;
    this._viewStack.style.transform = "translateX(" + (moveToLeft ? "" : "-") + deltaX + "px)";

    await new Promise(resolve => {
      details.resolve = resolve;
      this._viewContainer.addEventListener("transitionend", details.listener = ev => {
        // It's quite common that `height` on the view container doesn't need
        // to transition, so we make sure to do all the work on the transform
        // transition-end, because that is guaranteed to happen.
        if (ev.target != this._viewStack || ev.propertyName != "transform")
          return;
        this._viewContainer.removeEventListener("transitionend", details.listener);
        delete details.listener;
        resolve();
      });
      this._viewContainer.addEventListener("transitioncancel", details.cancelListener = ev => {
        if (ev.target != this._viewStack)
          return;
        this._viewContainer.removeEventListener("transitioncancel", details.cancelListener);
        delete details.cancelListener;
        resolve();
      });
    });

    details.phase = TRANSITION_PHASES.END;

    // Apply the final visibility, unless the view was closed in the meantime.
    if (nextPanelView.node.panelMultiView == this.node) {
      prevPanelView.visible = false;
      nextPanelView.visible = true;
      nextPanelView.descriptionHeightWorkaround();
    }

    // This will complete the operation by removing any transition properties.
    await this._cleanupTransitionPhase(details);

    // Focus the correct element, unless the view was closed in the meantime.
    if (nextPanelView.node.panelMultiView == this.node) {
      nextPanelView.focusSelectedElement();
    }
  }

  /**
   * Attempt to clean up the attributes and properties set by `_transitionViews`
   * above. Which attributes and properties depends on the phase the transition
   * was left from - normally that'd be `TRANSITION_PHASES.END`.
   *
   * @param {Object} details Dictionary object containing details of the transition
   *                         that should be cleaned up after. Defaults to the most
   *                         recent details.
   */
  async _cleanupTransitionPhase(details = this._transitionDetails) {
    if (!details || !this.node)
      return;

    let {phase, previousViewNode, viewNode, reverse, resolve, listener, cancelListener, anchor} = details;
    if (details == this._transitionDetails)
      this._transitionDetails = null;

    // Do the things we _always_ need to do whenever the transition ends or is
    // interrupted.
    previousViewNode.removeAttribute("in-transition");
    viewNode.removeAttribute("in-transition");

    if (anchor)
      anchor.removeAttribute("open");

    if (phase >= TRANSITION_PHASES.START) {
      this._panel.removeAttribute("width");
      this._panel.removeAttribute("height");
      // Myeah, panel layout auto-resizing is a funky thing. We'll wait
      // another few milliseconds to remove the width and height 'fixtures',
      // to be sure we don't flicker annoyingly.
      // NB: HACK! Bug 1363756 is there to fix this.
      this._autoResizeWorkaroundTimer = this.window.setTimeout(() => {
        if (!this._viewContainer)
          return;
        this._viewContainer.style.removeProperty("height");
        this._viewContainer.style.removeProperty("width");
      }, 500);
    }
    if (phase >= TRANSITION_PHASES.PREPARE) {
      this._transitioning = false;
      if (reverse)
        this._viewStack.style.removeProperty("margin-inline-start");
      let deepestNode = reverse ? previousViewNode : viewNode;
      deepestNode.style.removeProperty("outline");
      this._viewStack.style.removeProperty("transition");
    }
    if (phase >= TRANSITION_PHASES.TRANSITION) {
      this._viewStack.style.removeProperty("transform");
      viewNode.style.removeProperty("width");
      if (listener)
        this._viewContainer.removeEventListener("transitionend", listener);
      if (cancelListener)
        this._viewContainer.removeEventListener("transitioncancel", cancelListener);
      if (resolve)
        resolve();
    }
    if (phase >= TRANSITION_PHASES.END) {
      // We force 'display: none' on the previous view node to make sure that it
      // doesn't cause an annoying flicker whilst resetting the styles above.
      previousViewNode.style.display = "none";
      await BrowserUtils.promiseLayoutFlushed(this.document, "layout", () => {});
      previousViewNode.style.removeProperty("display");
    }
  }

  _calculateMaxHeight() {
    // While opening the panel, we have to limit the maximum height of any
    // view based on the space that will be available. We cannot just use
    // window.screen.availTop and availHeight because these may return an
    // incorrect value when the window spans multiple screens.
    let anchorBox = this._panel.anchorNode.boxObject;
    let screen = this._screenManager.screenForRect(anchorBox.screenX,
                                                   anchorBox.screenY,
                                                   anchorBox.width,
                                                   anchorBox.height);
    let availTop = {}, availHeight = {};
    screen.GetAvailRect({}, availTop, {}, availHeight);
    let cssAvailTop = availTop.value / screen.defaultCSSScaleFactor;

    // The distance from the anchor to the available margin of the screen is
    // based on whether the panel will open towards the top or the bottom.
    let maxHeight;
    if (this._panel.alignmentPosition.startsWith("before_")) {
      maxHeight = anchorBox.screenY - cssAvailTop;
    } else {
      let anchorScreenBottom = anchorBox.screenY + anchorBox.height;
      let cssAvailHeight = availHeight.value / screen.defaultCSSScaleFactor;
      maxHeight = cssAvailTop + cssAvailHeight - anchorScreenBottom;
    }

    // To go from the maximum height of the panel to the maximum height of
    // the view stack, we need to subtract the height of the arrow and the
    // height of the opposite margin, but we cannot get their actual values
    // because the panel is not visible yet. However, we know that this is
    // currently 11px on Mac, 13px on Windows, and 13px on Linux. We also
    // want an extra margin, both for visual reasons and to prevent glitches
    // due to small rounding errors. So, we just use a value that makes
    // sense for all platforms. If the arrow visuals change significantly,
    // this value will be easy to adjust.
    const EXTRA_MARGIN_PX = 20;
    maxHeight -= EXTRA_MARGIN_PX;
    return maxHeight;
  }

  handleEvent(aEvent) {
    if (aEvent.type.startsWith("popup") && aEvent.target != this._panel) {
      // Shouldn't act on e.g. context menus being shown from within the panel.
      return;
    }
    switch (aEvent.type) {
      case "keydown":
        if (!this._transitioning) {
          PanelView.forNode(this._currentSubView)
                   .keyNavigation(aEvent, this._dir);
        }
        break;
      case "mousemove":
        this.openViews.forEach(panelView => panelView.clearNavigation());
        break;
      case "popupshowing": {
        this.node.setAttribute("panelopen", "true");
        if (!this.node.hasAttribute("disablekeynav")) {
          this.window.addEventListener("keydown", this);
          this._panel.addEventListener("mousemove", this);
        }
        break;
      }
      case "popuppositioned": {
        // When autoPosition is true, the popup window manager would attempt to re-position
        // the panel as subviews are opened and it changes size. The resulting popoppositioned
        // events triggers the binding's arrow position adjustment - and its reflow.
        // This is not needed here, as we calculated and set maxHeight so it is known
        // to fit the screen while open.
        // autoPosition gets reset after each popuppositioned event, and when the
        // popup closes, so we must set it back to false each time.
        this._panel.autoPosition = false;

        if (this._panel.state == "showing") {
          let maxHeight = this._calculateMaxHeight();
          this._viewStack.style.maxHeight = maxHeight + "px";
          this._offscreenViewStack.style.maxHeight = maxHeight + "px";
        }
        break;
      }
      case "popupshown":
        // Now that the main view is visible, we can check the height of the
        // description elements it contains.
        PanelView.forNode(this._mainView).descriptionHeightWorkaround();
        break;
      case "popuphidden": {
        // WebExtensions consumers can hide the popup from viewshowing, or
        // mid-transition, which disrupts our state:
        this._transitioning = false;
        this.node.removeAttribute("panelopen");
        this._cleanupTransitionPhase();
        this.window.removeEventListener("keydown", this);
        this._panel.removeEventListener("mousemove", this);
        this.closeAllViews();

        // Clear the main view size caches. The dimensions could be different
        // when the popup is opened again, e.g. through touch mode sizing.
        this._viewContainer.style.removeProperty("min-height");
        this._viewStack.style.removeProperty("max-height");
        this._viewContainer.style.removeProperty("width");
        this._viewContainer.style.removeProperty("height");

        this.dispatchCustomEvent("PanelMultiViewHidden");
        break;
      }
    }
  }
};

/**
 * This is associated to <panelview> elements.
 */
this.PanelView = class extends this.AssociatedToNode {
  /**
   * The "mainview" attribute is set before the panel is opened when this view
   * is displayed as the main view, and is removed before the <panelview> is
   * displayed as a subview. The same view element can be displayed as a main
   * view and as a subview at different times.
   */
  set mainview(value) {
    if (value) {
      this.node.setAttribute("mainview", true);
    } else {
      this.node.removeAttribute("mainview");
    }
  }

  set visible(value) {
    if (value) {
      this.node.setAttribute("current", true);
    } else {
      this.node.removeAttribute("current");
    }
  }

  /**
   * Constrains the width of this view using the "min-width" and "max-width"
   * styles. Setting this to zero removes the constraints.
   */
  set minMaxWidth(value) {
    let style = this.node.style;
    if (value) {
      style.minWidth = style.maxWidth = value + "px";
    } else {
      style.removeProperty("min-width");
      style.removeProperty("max-width");
    }
  }

  /**
   * Adds a header with the given title, or removes it if the title is empty.
   */
  set headerText(value) {
    // If the header already exists, update or remove it as requested.
    let header = this.node.firstChild;
    if (header && header.classList.contains("panel-header")) {
      if (value) {
        header.querySelector("label").setAttribute("value", value);
      } else {
        header.remove();
      }
      return;
    }

    // The header doesn't exist, only create it if needed.
    if (!value) {
      return;
    }

    header = this.document.createElement("box");
    header.classList.add("panel-header");

    let backButton = this.document.createElement("toolbarbutton");
    backButton.className =
      "subviewbutton subviewbutton-iconic subviewbutton-back";
    backButton.setAttribute("closemenu", "none");
    backButton.setAttribute("tabindex", "0");
    backButton.setAttribute("tooltip",
      this.node.getAttribute("data-subviewbutton-tooltip"));
    backButton.addEventListener("command", () => {
      // The panelmultiview element may change if the view is reused.
      this.node.panelMultiView.goBack();
      backButton.blur();
    });

    let label = this.document.createElement("label");
    label.setAttribute("value", value);

    header.append(backButton, label);
    this.node.prepend(header);
  }

  /**
   * Also make sure that the correct method is called on CustomizableWidget.
   */
  dispatchCustomEvent(...args) {
    CustomizableUI.ensureSubviewListeners(this.node);
    return super.dispatchCustomEvent(...args);
  }

  /**
   * Populates the "knownWidth" and "knownHeight" properties with the current
   * dimensions of the view. These may be zero if the view is invisible.
   *
   * These values are relevant during transitions and are retained for backwards
   * navigation if the view is still open but is invisible.
   */
  captureKnownSize() {
    let rect = this._dwu.getBoundsWithoutFlushing(this.node);
    this.knownWidth = rect.width;
    this.knownHeight = rect.height;
  }

  /**
   * If the main view or a subview contains wrapping elements, the attribute
   * "descriptionheightworkaround" should be set on the view to force all the
   * wrapping "description", "label" or "toolbarbutton" elements to a fixed
   * height. If the attribute is set and the visibility, contents, or width
   * of any of these elements changes, this function should be called to
   * refresh the calculated heights.
   *
   * This may trigger a synchronous layout.
   */
  descriptionHeightWorkaround() {
    if (!this.node.hasAttribute("descriptionheightworkaround")) {
      // This view does not require the workaround.
      return;
    }

    // We batch DOM changes together in order to reduce synchronous layouts.
    // First we reset any change we may have made previously. The first time
    // this is called, and in the best case scenario, this has no effect.
    let items = [];
    // Non-hidden <label> or <description> elements that also aren't empty
    // and also don't have a value attribute can be multiline (if their
    // text content is long enough).
    let isMultiline = ":not(:-moz-any([hidden],[value],:empty))";
    let selector = [
      "description" + isMultiline,
      "label" + isMultiline,
      "toolbarbutton[wrap]:not([hidden])",
    ].join(",");
    for (let element of this.node.querySelectorAll(selector)) {
      // Ignore items in hidden containers.
      if (element.closest("[hidden]")) {
        continue;
      }
      // Take the label for toolbarbuttons; it only exists on those elements.
      element = element.labelElement || element;

      let bounds = element.getBoundingClientRect();
      let previous = gMultiLineElementsMap.get(element);
      // We don't need to (re-)apply the workaround for invisible elements or
      // on elements we've seen before and haven't changed in the meantime.
      if (!bounds.width || !bounds.height ||
          (previous && element.textContent == previous.textContent &&
                       bounds.width == previous.bounds.width)) {
        continue;
      }

      items.push({ element });
    }

    // Removing the 'height' property will only cause a layout flush in the next
    // loop below if it was set.
    for (let item of items) {
      item.element.style.removeProperty("height");
    }

    // We now read the computed style to store the height of any element that
    // may contain wrapping text.
    for (let item of items) {
      item.bounds = item.element.getBoundingClientRect();
    }

    // Now we can make all the necessary DOM changes at once.
    for (let { element, bounds } of items) {
      gMultiLineElementsMap.set(element, { bounds, textContent: element.textContent });
      element.style.height = bounds.height + "px";
    }
  }

  /**
   * Retrieves the button elements that can be used for navigation using the
   * keyboard, that is all enabled buttons including the back button if visible.
   *
   * @return {Array}
   */
  getNavigableElements() {
    let buttons = Array.from(this.node.querySelectorAll(".subviewbutton:not([disabled])"));
    let dwu = this._dwu;
    return buttons.filter(button => {
      let bounds = dwu.getBoundsWithoutFlushing(button);
      return bounds.width > 0 && bounds.height > 0;
    });
  }

  /**
   * Element that is currently selected with the keyboard, or null if no element
   * is selected. Since the reference is held weakly, it can become null or
   * undefined at any time.
   *
   * The element is usually, but not necessarily, in the "buttons" property
   * which in turn is initialized from the getNavigableElements list.
   */
  get selectedElement() {
    return this._selectedElement && this._selectedElement.get();
  }
  set selectedElement(value) {
    if (!value) {
      delete this._selectedElement;
    } else {
      this._selectedElement = Cu.getWeakReference(value);
    }
  }

  /**
   * Based on going up or down, select the previous or next focusable button.
   *
   * @param {Boolean} isDown   whether we're going down (true) or up (false).
   *
   * @return {DOMNode} the button we selected.
   */
  moveSelection(isDown) {
    let buttons = this.buttons;
    let lastSelected = this.selectedElement;
    let newButton = null;
    let maxIdx = buttons.length - 1;
    if (lastSelected) {
      let buttonIndex = buttons.indexOf(lastSelected);
      if (buttonIndex != -1) {
        // Buttons may get selected whilst the panel is shown, so add an extra
        // check here.
        do {
          buttonIndex = buttonIndex + (isDown ? 1 : -1);
        } while (buttons[buttonIndex] && buttons[buttonIndex].disabled);
        if (isDown && buttonIndex > maxIdx)
          buttonIndex = 0;
        else if (!isDown && buttonIndex < 0)
          buttonIndex = maxIdx;
        newButton = buttons[buttonIndex];
      } else {
        // The previously selected item is no longer selectable. Find the next item:
        let allButtons = lastSelected.closest("panelview").getElementsByTagName("toolbarbutton");
        let maxAllButtonIdx = allButtons.length - 1;
        let allButtonIndex = allButtons.indexOf(lastSelected);
        while (allButtonIndex >= 0 && allButtonIndex <= maxAllButtonIdx) {
          allButtonIndex++;
          // Check if the next button is in the list of focusable buttons.
          buttonIndex = buttons.indexOf(allButtons[allButtonIndex]);
          if (buttonIndex != -1) {
            // If it is, just use that button if we were going down, or the previous one
            // otherwise. If this was the first button, newButton will end up undefined,
            // which is fine because we'll fall back to using the last button at the
            // bottom of this method.
            newButton = buttons[isDown ? buttonIndex : buttonIndex - 1];
            break;
          }
        }
      }
    }

    // If we couldn't find something, select the first or last item:
    if (!newButton) {
      newButton = buttons[isDown ? 0 : maxIdx];
    }
    this.selectedElement = newButton;
    return newButton;
  }

  /**
   * Allow for navigating subview buttons using the arrow keys and the Enter key.
   * The Up and Down keys can be used to navigate the list up and down and the
   * Enter, Right or Left - depending on the text direction - key can be used to
   * simulate a click on the currently selected button.
   * The Right or Left key - depending on the text direction - can be used to
   * navigate to the previous view, functioning as a shortcut for the view's
   * back button.
   * Thus, in LTR mode:
   *  - The Right key functions the same as the Enter key, simulating a click
   *  - The Left key triggers a navigation back to the previous view.
   *
   * @param {KeyEvent} event
   * @param {String} dir
   *        Direction for arrow navigation, either "ltr" or "rtl".
   */
  keyNavigation(event, dir) {
    let buttons = this.buttons;
    if (!buttons || !buttons.length) {
      buttons = this.buttons = this.getNavigableElements();
      // Set the 'tabindex' attribute on the buttons to make sure they're focussable.
      for (let button of buttons) {
        if (!button.classList.contains("subviewbutton-back") &&
            !button.hasAttribute("tabindex")) {
          button.setAttribute("tabindex", 0);
        }
      }
    }
    if (!buttons.length)
      return;

    let stop = () => {
      event.stopPropagation();
      event.preventDefault();
    };

    let keyCode = event.code;
    switch (keyCode) {
      case "ArrowDown":
      case "ArrowUp":
      case "Tab": {
        stop();
        let isDown = (keyCode == "ArrowDown") ||
                     (keyCode == "Tab" && !event.shiftKey);
        let button = this.moveSelection(isDown);
        button.focus();
        break;
      }
      case "ArrowLeft":
      case "ArrowRight": {
        stop();
        if ((dir == "ltr" && keyCode == "ArrowLeft") ||
            (dir == "rtl" && keyCode == "ArrowRight")) {
          this.node.panelMultiView.goBack();
          break;
        }
        // If the current button is _not_ one that points to a subview, pressing
        // the arrow key shouldn't do anything.
        let button = this.selectedElement;
        if (!button || !button.classList.contains("subviewbutton-nav")) {
          break;
        }
        // Fall-through...
      }
      case "Space":
      case "Enter": {
        let button = this.selectedElement;
        if (!button)
          break;
        stop();

        // Unfortunately, 'tabindex' doesn't execute the default action, so
        // we explicitly do this here.
        // We are sending a command event and then a click event.
        // This is done in order to mimic a "real" mouse click event.
        // The command event executes the action, then the click event closes the menu.
        button.doCommand();
        let clickEvent = new event.target.ownerGlobal.MouseEvent("click", {"bubbles": true});
        button.dispatchEvent(clickEvent);
        break;
      }
    }
  }

  /**
   * Focus the last selected element in the view, if any.
   */
  focusSelectedElement() {
    let selected = this.selectedElement;
    if (selected) {
      selected.focus();
    }
  }

  /**
   * Clear all traces of keyboard navigation happening right now.
   */
  clearNavigation() {
    delete this.buttons;
    let selected = this.selectedElement;
    if (selected) {
      selected.blur();
      this.selectedElement = null;
    }
  }
};
