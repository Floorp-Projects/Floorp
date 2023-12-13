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
 * -- Active or inactive
 *
 *    This indicates whether the view is fully scrolled into the visible area
 *    and ready to receive mouse and keyboard events. An active view is always
 *    visible, but a visible view may be inactive. For example, during a scroll
 *    transition, both views will be inactive.
 *
 *    When a view becomes active, the ViewShown event is fired synchronously,
 *    and the showSubView and goBack methods can be called for navigation.
 *
 *    For the main view of the panel, the ViewShown event is dispatched during
 *    the "popupshown" event, which means that other "popupshown" handlers may
 *    be called before the view is active. Thus, code that needs to perform
 *    further navigation automatically should either use the ViewShown event or
 *    wait for an event loop tick, like BrowserTestUtils.waitForEvent does.
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
 */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  CustomizableUI: "resource:///modules/CustomizableUI.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "gBundle", function () {
  return Services.strings.createBundle(
    "chrome://browser/locale/browser.properties"
  );
});

/**
 * Safety timeout after which asynchronous events will be canceled if any of the
 * registered blockers does not return.
 */
const BLOCKERS_TIMEOUT_MS = 10000;

const TRANSITION_PHASES = Object.freeze({
  START: 1,
  PREPARE: 2,
  TRANSITION: 3,
});

let gNodeToObjectMap = new WeakMap();
let gWindowsWithUnloadHandler = new WeakSet();

/**
 * Allows associating an object to a node lazily using a weak map.
 *
 * Classes deriving from this one may be easily converted to Custom Elements,
 * although they would lose the ability of being associated lazily.
 */
var AssociatedToNode = class {
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

  _getBoundsWithoutFlushing(element) {
    return this.window.windowUtils.getBoundsWithoutFlushing(element);
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
    return (this._blockersPromise = blockersPromise.then(async () => {
      let blockers = new Set();
      let cancel = this.dispatchCustomEvent(
        eventName,
        {
          addBlocker(promise) {
            // Any exception in the blocker will cancel the operation.
            blockers.add(
              promise.catch(ex => {
                console.error(ex);
                return true;
              })
            );
          },
        },
        true
      );
      if (blockers.size) {
        let timeoutPromise = new Promise((resolve, reject) => {
          this.window.setTimeout(reject, BLOCKERS_TIMEOUT_MS);
        });
        try {
          let results = await Promise.race([
            Promise.all(blockers),
            timeoutPromise,
          ]);
          cancel = cancel || results.some(result => result === false);
        } catch (ex) {
          console.error(
            new Error(`One of the blockers for ${eventName} timed out.`)
          );
          return true;
        }
      }
      return cancel;
    }));
  }
};

/**
 * This is associated to <panelmultiview> elements.
 */
export var PanelMultiView = class extends AssociatedToNode {
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
   * @param {DOMNode} panelNode The <panel> node.
   * @param {Boolean} [animate] Whether to show a fade animation. Optional.
   *
   * @see The non-static hidePopup method for details.
   */
  static hidePopup(panelNode, animate = false) {
    let panelMultiViewNode = panelNode.querySelector("panelmultiview");
    if (panelMultiViewNode) {
      this.forNode(panelMultiViewNode).hidePopup(animate);
    } else {
      panelNode.hidePopup(animate);
    }
  }

  /**
   * Removes the specified <panel> from the document, ensuring that any
   * <panelmultiview> node it contains is destroyed properly.
   *
   * If the viewCacheId attribute is present on the <panelmultiview> element,
   * imported subviews will be moved out again to the element it specifies, so
   * that the panel element can be removed safely.
   *
   * If the panel does not contain a <panelmultiview>, it is removed directly.
   * This allows consumers like page actions to accept different panel types.
   */
  static removePopup(panelNode) {
    try {
      let panelMultiViewNode = panelNode.querySelector("panelmultiview");
      if (panelMultiViewNode) {
        let panelMultiView = this.forNode(panelMultiViewNode);
        panelMultiView._moveOutKids();
        panelMultiView.disconnect();
      }
    } finally {
      // Make sure to remove the panel element even if disconnecting fails.
      panelNode.remove();
    }
  }
  /**
   * Returns the element with the given id.
   * For nodes that are lazily loaded and not yet in the DOM, the node should
   * be retrieved from the view cache template.
   */
  static getViewNode(doc, id) {
    let viewCacheTemplate = doc.getElementById("appMenu-viewCache");

    return (
      doc.getElementById(id) ||
      viewCacheTemplate?.content.querySelector("#" + id)
    );
  }

  /**
   * Ensures that when the specified window is closed all the <panelmultiview>
   * node it contains are destroyed properly.
   */
  static ensureUnloadHandlerRegistered(window) {
    if (gWindowsWithUnloadHandler.has(window)) {
      return;
    }

    window.addEventListener(
      "unload",
      () => {
        for (let panelMultiViewNode of window.document.querySelectorAll(
          "panelmultiview"
        )) {
          this.forNode(panelMultiViewNode).disconnect();
        }
      },
      { once: true }
    );

    gWindowsWithUnloadHandler.add(window);
  }

  get _panel() {
    return this.node.parentNode;
  }

  set _transitioning(val) {
    if (val) {
      this.node.setAttribute("transitioning", "true");
    } else {
      this.node.removeAttribute("transitioning");
    }
  }

  get _screenManager() {
    if (this.__screenManager) {
      return this.__screenManager;
    }
    return (this.__screenManager = Cc[
      "@mozilla.org/gfx/screenmanager;1"
    ].getService(Ci.nsIScreenManager));
  }

  constructor(node) {
    super(node);
    this._openPopupPromise = Promise.resolve(false);
  }

  connect() {
    this.connected = true;

    PanelMultiView.ensureUnloadHandlerRegistered(this.window);

    let viewContainer = (this._viewContainer =
      this.document.createXULElement("box"));
    viewContainer.classList.add("panel-viewcontainer");

    let viewStack = (this._viewStack = this.document.createXULElement("box"));
    viewStack.classList.add("panel-viewstack");
    viewContainer.append(viewStack);

    let offscreenViewContainer = this.document.createXULElement("box");
    offscreenViewContainer.classList.add("panel-viewcontainer", "offscreen");

    let offscreenViewStack = (this._offscreenViewStack =
      this.document.createXULElement("box"));
    offscreenViewStack.classList.add("panel-viewstack");
    offscreenViewContainer.append(offscreenViewStack);

    this.node.prepend(offscreenViewContainer);
    this.node.prepend(viewContainer);

    this.openViews = [];

    this._panel.addEventListener("popupshowing", this);
    this._panel.addEventListener("popuppositioned", this);
    this._panel.addEventListener("popuphidden", this);
    this._panel.addEventListener("popupshown", this);

    // Proxy these public properties and methods, as used elsewhere by various
    // parts of the browser, to this instance.
    ["goBack", "showSubView"].forEach(method => {
      Object.defineProperty(this.node, method, {
        enumerable: true,
        value: (...args) => this[method](...args),
      });
    });
  }

  disconnect() {
    // Guard against re-entrancy.
    if (!this.node || !this.connected) {
      return;
    }

    this._panel.removeEventListener("mousemove", this);
    this._panel.removeEventListener("popupshowing", this);
    this._panel.removeEventListener("popuppositioned", this);
    this._panel.removeEventListener("popupshown", this);
    this._panel.removeEventListener("popuphidden", this);
    this.document.documentElement.removeEventListener("keydown", this, true);
    this.node =
      this._openPopupPromise =
      this._openPopupCancelCallback =
      this._viewContainer =
      this._viewStack =
      this._transitionDetails =
        null;
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
   * @param anchor
   *        The node to anchor the popup to.
   * @param options
   *        Either options to use or a string position. This is forwarded to
   *        the openPopup method of the panel.
   * @param args
   *        Additional arguments to be forwarded to the openPopup method of the
   *        panel.
   *
   * @resolves With true as soon as the request to display the panel has been
   *           sent, or with false if the operation was canceled. The state of
   *           the panel at this point is not guaranteed. It may be still
   *           showing, completely shown, or completely hidden.
   * @rejects If an exception is thrown at any point in the process before the
   *          request to display the panel is sent.
   */
  async openPopup(anchor, options, ...args) {
    // Set up the function that allows hidePopup or a second call to showPopup
    // to cancel the specific panel opening operation that we're starting below.
    // This function must be synchronous, meaning we can't use Promise.race,
    // because hidePopup wants to dispatch the "popuphidden" event synchronously
    // even if the panel has not been opened yet.
    let canCancel = true;
    let cancelCallback = (this._openPopupCancelCallback = () => {
      // If the cancel callback is called and the panel hasn't been prepared
      // yet, cancel showing it. Setting canCancel to false will prevent the
      // popup from opening. If the panel has opened by the time the cancel
      // callback is called, canCancel will be false already, and we will not
      // fire the "popuphidden" event.
      if (canCancel && this.node) {
        canCancel = false;
        this.dispatchCustomEvent("popuphidden");
      }
      if (cancelCallback == this._openPopupCancelCallback) {
        // If still current, let go of the cancel callback since it will capture
        // the entire scope and tie it to the main window.
        delete this._openPopupCancelCallback;
      }
    });

    // Create a promise that is resolved with the result of the last call to
    // this method, where errors indicate that the panel was not opened.
    let openPopupPromise = this._openPopupPromise.catch(() => {
      return false;
    });

    // Make the preparation done before showing the panel non-reentrant. The
    // promise created here will be resolved only after the panel preparation is
    // completed, even if a cancellation request is received in the meantime.
    return (this._openPopupPromise = openPopupPromise.then(async wasShown => {
      // The panel may have been destroyed in the meantime.
      if (!this.node) {
        return false;
      }
      // If the panel has been already opened there is nothing more to do. We
      // check the actual state of the panel rather than setting some state in
      // our handler of the "popuphidden" event because this has a lower chance
      // of locking indefinitely if events aren't raised in the expected order.
      if (wasShown && ["open", "showing"].includes(this._panel.state)) {
        if (cancelCallback == this._openPopupCancelCallback) {
          // If still current, let go of the cancel callback since it will
          // capture the entire scope and tie it to the main window.
          delete this._openPopupCancelCallback;
        }
        return true;
      }
      try {
        if (!this.connected) {
          this.connect();
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
        this._panel.openPopup(anchor, options, ...args);
        if (cancelCallback == this._openPopupCancelCallback) {
          // If still current, let go of the cancel callback since it will
          // capture the entire scope and tie it to the main window.
          delete this._openPopupCancelCallback;
        }
        // Set an attribute on the popup to let consumers style popup elements -
        // for example, the anchor arrow is styled to match the color of the header
        // in the Protections Panel main view.
        this._panel.setAttribute("mainviewshowing", true);

        // On Windows, if another popup is hiding while we call openPopup, the
        // call won't fail but the popup won't open. In this case, we have to
        // dispatch an artificial "popuphidden" event to reset our state.
        if (this._panel.state == "closed" && this.openViews.length) {
          this.dispatchCustomEvent("popuphidden");
          return false;
        }

        if (
          options &&
          typeof options == "object" &&
          options.triggerEvent &&
          (options.triggerEvent.type == "keypress" ||
            options.triggerEvent.type == "keydown" ||
            options.triggerEvent?.inputSource ==
              MouseEvent.MOZ_SOURCE_KEYBOARD) &&
          this.openViews.length
        ) {
          // This was opened via the keyboard, so focus the first item.
          this.openViews[0].focusWhenActive = true;
        }

        return true;
      } catch (ex) {
        this.dispatchCustomEvent("popuphidden");
        throw ex;
      }
    }));
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
   *
   * @note The value of animate could be changed to true by default, in both
   *       this and the static method above. (see bug 1769813)
   *
   * @param {Boolean} [animate] Whether to show a fade animation. Optional.
   *
   */
  hidePopup(animate = false) {
    if (!this.node || !this.connected) {
      return;
    }

    // If we have already reached the _panel.openPopup call in the openPopup
    // method, we can call hidePopup. Otherwise, we have to cancel the latest
    // request to open the panel, which will have no effect if the request has
    // been canceled already.
    if (["open", "showing"].includes(this._panel.state)) {
      this._panel.hidePopup(animate);
    } else {
      this._openPopupCancelCallback?.();
    }

    // We close all the views synchronously, so that they are ready to be opened
    // in other PanelMultiView instances. The "popuphidden" handler may also
    // call this function, but the second time openViews will be empty.
    this.closeAllViews();
  }

  /**
   * Move any child subviews into the element defined by "viewCacheId" to make
   * sure they will not be removed together with the <panelmultiview> element.
   */
  _moveOutKids() {
    // this.node may have been set to null by a call to disconnect().
    let viewCacheId = this.node?.getAttribute("viewCacheId");
    if (!viewCacheId) {
      return;
    }

    // Node.children and Node.children is live to DOM changes like the
    // ones we're about to do, so iterate over a static copy:
    let subviews = Array.from(this._viewStack.children);
    let viewCache = this.document.getElementById("appMenu-viewCache");
    for (let subview of subviews) {
      viewCache.appendChild(subview);
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
    this._showSubView(viewIdOrNode, anchor).catch(console.error);
  }
  async _showSubView(viewIdOrNode, anchor) {
    let viewNode =
      typeof viewIdOrNode == "string"
        ? PanelMultiView.getViewNode(this.document, viewIdOrNode)
        : viewIdOrNode;
    if (!viewNode) {
      console.error(new Error(`Subview ${viewIdOrNode} doesn't exist.`));
      return;
    }

    if (!this.openViews.length) {
      console.error(new Error(`Cannot show a subview in a closed panel.`));
      return;
    }

    let prevPanelView = this.openViews[this.openViews.length - 1];
    let nextPanelView = PanelView.forNode(viewNode);
    if (this.openViews.includes(nextPanelView)) {
      console.error(new Error(`Subview ${viewNode.id} is already open.`));
      return;
    }

    // Do not re-enter the process if navigation is already in progress. Since
    // there is only one active view at any given time, we can do this check
    // safely, even considering that during the navigation process the actual
    // view to which prevPanelView refers will change.
    if (!prevPanelView.active) {
      return;
    }
    // If prevPanelView._doingKeyboardActivation is true, it will be reset to
    // false synchronously. Therefore, we must capture it before we use any
    // "await" statements.
    let doingKeyboardActivation = prevPanelView._doingKeyboardActivation;
    // Marking the view that is about to scrolled out of the visible area as
    // inactive will prevent re-entrancy and also disable keyboard navigation.
    // From this point onwards, "await" statements can be used safely.
    prevPanelView.active = false;

    // Provide visual feedback while navigation is in progress, starting before
    // the transition starts and ending when the previous view is invisible.
    anchor?.setAttribute("open", "true");
    try {
      // If the ViewShowing event cancels the operation we have to re-enable
      // keyboard navigation, but this must be avoided if the panel was closed.
      if (!(await this._openView(nextPanelView))) {
        if (prevPanelView.isOpenIn(this)) {
          // We don't raise a ViewShown event because nothing actually changed.
          // Technically we should use a different state flag just because there
          // is code that could check the "active" property to determine whether
          // to wait for a ViewShown event later, but this only happens in
          // regression tests and is less likely to be a technique used in
          // production code, where use of ViewShown is less common.
          prevPanelView.active = true;
        }
        return;
      }

      prevPanelView.captureKnownSize();

      // The main view of a panel can be a subview in another one. Make sure to
      // reset all the properties that may be set on a subview.
      nextPanelView.mainview = false;
      // The header may be set by a Fluent message with a title attribute
      // that has changed immediately before showing the panelview,
      // and so is not reflected in the DOM yet.
      let title;
      const l10nId = viewNode.getAttribute("data-l10n-id");
      if (l10nId) {
        const l10nArgs = viewNode.getAttribute("data-l10n-args");
        const args = l10nArgs ? JSON.parse(l10nArgs) : undefined;
        const [msg] = await viewNode.ownerDocument.l10n.formatMessages([
          { id: l10nId, args },
        ]);
        title = msg.attributes.find(a => a.name === "title")?.value;
      }
      // If not set by Fluent, the header may change based on how the subview was opened.
      title ??= viewNode.getAttribute("title") || anchor?.getAttribute("label");
      nextPanelView.headerText = title;
      // The constrained width of subviews may also vary between panels.
      nextPanelView.minMaxWidth = prevPanelView.knownWidth;
      let lockPanelVertical =
        this.openViews[0].node.getAttribute("lockpanelvertical") == "true";
      nextPanelView.minMaxHeight = lockPanelVertical
        ? prevPanelView.knownHeight
        : 0;

      if (anchor) {
        viewNode.classList.add("PanelUI-subView");
      }

      await this._transitionViews(prevPanelView.node, viewNode, false);
    } finally {
      anchor?.removeAttribute("open");
    }

    nextPanelView.focusWhenActive = doingKeyboardActivation;
    this._activateView(nextPanelView);
  }

  /**
   * Navigates backwards by sliding out the most recent subview.
   */
  goBack() {
    this._goBack().catch(console.error);
  }
  async _goBack() {
    if (this.openViews.length < 2) {
      // This may be called by keyboard navigation or external code when only
      // the main view is open.
      return;
    }

    let prevPanelView = this.openViews[this.openViews.length - 1];
    let nextPanelView = this.openViews[this.openViews.length - 2];

    // Like in the showSubView method, do not re-enter navigation while it is
    // in progress, and make the view inactive immediately. From this point
    // onwards, "await" statements can be used safely.
    if (!prevPanelView.active) {
      return;
    }
    prevPanelView.active = false;

    prevPanelView.captureKnownSize();
    await this._transitionViews(prevPanelView.node, nextPanelView.node, true);

    this._closeLatestView();

    this._activateView(nextPanelView);
  }

  /**
   * Prepares the main view before showing the panel.
   */
  async _showMainView() {
    let nextPanelView = PanelView.forNode(
      PanelMultiView.getViewNode(
        this.document,
        this.node.getAttribute("mainViewId")
      )
    );

    // If the view is already open in another panel, close the panel first.
    let oldPanelMultiViewNode = nextPanelView.node.panelMultiView;
    if (oldPanelMultiViewNode) {
      PanelMultiView.forNode(oldPanelMultiViewNode).hidePopup();
      // Wait for a layout flush after hiding the popup, otherwise the view may
      // not be displayed correctly for some time after the new panel is opened.
      // This is filed as bug 1441015.
      await this.window.promiseDocumentFlushed(() => {});
    }

    if (!(await this._openView(nextPanelView))) {
      return false;
    }

    // The main view of a panel can be a subview in another one. Make sure to
    // reset all the properties that may be set on a subview.
    nextPanelView.mainview = true;
    nextPanelView.headerText = "";
    nextPanelView.minMaxWidth = 0;
    nextPanelView.minMaxHeight = 0;

    // Ensure the view will be visible once the panel is opened.
    nextPanelView.visible = true;

    return true;
  }

  /**
   * Opens the specified PanelView and dispatches the ViewShowing event, which
   * can be used to populate the subview or cancel the operation.
   *
   * This also clears all the attributes and styles that may be left by a
   * transition that was interrupted.
   *
   * @resolves With true if the view was opened, false otherwise.
   */
  async _openView(panelView) {
    if (panelView.node.parentNode != this._viewStack) {
      this._viewStack.appendChild(panelView.node);
    }

    panelView.node.panelMultiView = this.node;
    this.openViews.push(panelView);

    // Panels could contain out-pf-process <browser> elements, that need to be
    // supported with a remote attribute on the panel in order to display properly.
    // See bug https://bugzilla.mozilla.org/show_bug.cgi?id=1365660
    if (panelView.node.getAttribute("remote") == "true") {
      this._panel.setAttribute("remote", "true");
    }

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

    // Clean up all the attributes and styles related to transitions. We do this
    // here rather than when the view is closed because we are likely to make
    // other DOM modifications soon, which isn't the case when closing.
    let { style } = panelView.node;
    style.removeProperty("outline");
    style.removeProperty("width");

    return true;
  }

  /**
   * Activates the specified view and raises the ViewShown event, unless the
   * view was closed in the meantime.
   */
  _activateView(panelView) {
    if (panelView.isOpenIn(this)) {
      panelView.active = true;
      if (panelView.focusWhenActive) {
        panelView.focusFirstNavigableElement(false, true);
        panelView.focusWhenActive = false;
      }
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
    // become visible again until they are opened. When this is called at the
    // end of backwards navigation, the view is already invisible.
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
   * @param {panelview} previousViewNode Node that is currently displayed, but
   *                                     is about to be transitioned away. This
   *                                     must be already inactive at this point.
   * @param {panelview} viewNode         Node that will becode the active view,
   *                                     after the transition has finished.
   * @param {Boolean}   reverse          Whether we're navigation back to a
   *                                     previous view or forward to a next view.
   */
  async _transitionViews(previousViewNode, viewNode, reverse) {
    const { window } = this;

    let nextPanelView = PanelView.forNode(viewNode);
    let prevPanelView = PanelView.forNode(previousViewNode);

    let details = (this._transitionDetails = {
      phase: TRANSITION_PHASES.START,
    });

    // Set the viewContainer dimensions to make sure only the current view is
    // visible.
    let olderView = reverse ? nextPanelView : prevPanelView;
    this._viewContainer.style.minHeight = olderView.knownHeight + "px";
    this._viewContainer.style.height = prevPanelView.knownHeight + "px";
    this._viewContainer.style.width = prevPanelView.knownWidth + "px";
    // Lock the dimensions of the window that hosts the popup panel.
    let rect = this._getBoundsWithoutFlushing(this._panel);
    this._panel.style.width = rect.width + "px";
    this._panel.style.height = rect.height + "px";

    let viewRect;
    if (reverse) {
      // Use the cached size when going back to a previous view, but not when
      // reopening a subview, because its contents may have changed.
      viewRect = {
        width: nextPanelView.knownWidth,
        height: nextPanelView.knownHeight,
      };
      nextPanelView.visible = true;
    } else if (viewNode.customRectGetter) {
      // We use a customRectGetter for WebExtensions panels, because they need
      // to query the size from an embedded browser. The presence of this
      // getter also provides an indication that the view node shouldn't be
      // moved around, otherwise the state of the browser would get disrupted.
      let width = prevPanelView.knownWidth;
      let height = prevPanelView.knownHeight;
      viewRect = Object.assign({ height, width }, viewNode.customRectGetter());
      nextPanelView.visible = true;
      // Until the header is visible, it has 0 height.
      // Wait for layout before measuring it
      let header = viewNode.firstElementChild;
      if (header && header.classList.contains("panel-header")) {
        viewRect.height += await window.promiseDocumentFlushed(() => {
          return this._getBoundsWithoutFlushing(header).height;
        });
      }
      // Bail out if the panel was closed in the meantime.
      if (!nextPanelView.isOpenIn(this)) {
        return;
      }
    } else {
      this._offscreenViewStack.style.minHeight = olderView.knownHeight + "px";
      this._offscreenViewStack.appendChild(viewNode);
      nextPanelView.visible = true;

      viewRect = await window.promiseDocumentFlushed(() => {
        return this._getBoundsWithoutFlushing(viewNode);
      });
      // Bail out if the panel was closed in the meantime.
      if (!nextPanelView.isOpenIn(this)) {
        return;
      }

      // Place back the view after all the other views that are already open in
      // order for the transition to work as expected.
      this._viewStack.appendChild(viewNode);

      this._offscreenViewStack.style.removeProperty("min-height");
    }

    this._transitioning = true;
    details.phase = TRANSITION_PHASES.PREPARE;

    // The 'magic' part: build up the amount of pixels to move right or left.
    let moveToLeft =
      (this.window.RTL_UI && !reverse) || (!this.window.RTL_UI && reverse);
    let deltaX = prevPanelView.knownWidth;
    let deepestNode = reverse ? previousViewNode : viewNode;

    // With a transition when navigating backwards - user hits the 'back'
    // button - we need to make sure that the views are positioned in a way
    // that a translateX() unveils the previous view from the right direction.
    if (reverse) {
      this._viewStack.style.marginInlineStart = "-" + deltaX + "px";
    }

    // Set the transition style and listen for its end to clean up and make sure
    // the box sizing becomes dynamic again.
    // Somehow, putting these properties in PanelUI.css doesn't work for newly
    // shown nodes in a XUL parent node.
    this._viewStack.style.transition =
      "transform var(--animation-easing-function)" +
      " var(--panelui-subview-transition-duration)";
    this._viewStack.style.willChange = "transform";
    // Use an outline instead of a border so that the size is not affected.
    deepestNode.style.outline = "1px solid var(--panel-separator-color)";

    // Now that all the elements are in place for the start of the transition,
    // give the layout code a chance to set the initial values.
    await window.promiseDocumentFlushed(() => {});
    // Bail out if the panel was closed in the meantime.
    if (!nextPanelView.isOpenIn(this)) {
      return;
    }

    // Now set the viewContainer dimensions to that of the new view, which
    // kicks of the height animation.
    this._viewContainer.style.height = viewRect.height + "px";
    this._viewContainer.style.width = viewRect.width + "px";
    this._panel.style.removeProperty("width");
    this._panel.style.removeProperty("height");
    // We're setting the width property to prevent flickering during the
    // sliding animation with smaller views.
    viewNode.style.width = viewRect.width + "px";

    // Kick off the transition!
    details.phase = TRANSITION_PHASES.TRANSITION;

    // If we're going to show the main view, we can remove the
    // min-height property on the view container. It's also time
    // to set the mainviewshowing attribute on the popup.
    if (viewNode.getAttribute("mainview")) {
      this._viewContainer.style.removeProperty("min-height");
      this._panel.setAttribute("mainviewshowing", true);
    } else {
      this._panel.removeAttribute("mainviewshowing");
    }

    // Avoid transforming element if the user has prefers-reduced-motion set
    if (
      this.window.matchMedia("(prefers-reduced-motion: no-preference)").matches
    ) {
      this._viewStack.style.transform =
        "translateX(" + (moveToLeft ? "" : "-") + deltaX + "px)";

      await new Promise(resolve => {
        details.resolve = resolve;
        this._viewContainer.addEventListener(
          "transitionend",
          (details.listener = ev => {
            // It's quite common that `height` on the view container doesn't need
            // to transition, so we make sure to do all the work on the transform
            // transition-end, because that is guaranteed to happen.
            if (
              ev.target != this._viewStack ||
              ev.propertyName != "transform"
            ) {
              return;
            }
            this._viewContainer.removeEventListener(
              "transitionend",
              details.listener
            );
            delete details.listener;
            resolve();
          })
        );
        this._viewContainer.addEventListener(
          "transitioncancel",
          (details.cancelListener = ev => {
            if (ev.target != this._viewStack) {
              return;
            }
            this._viewContainer.removeEventListener(
              "transitioncancel",
              details.cancelListener
            );
            delete details.cancelListener;
            resolve();
          })
        );
      });
    }

    // Bail out if the panel was closed during the transition.
    if (!nextPanelView.isOpenIn(this)) {
      return;
    }
    prevPanelView.visible = false;

    // This will complete the operation by removing any transition properties.
    nextPanelView.node.style.removeProperty("width");
    deepestNode.style.removeProperty("outline");
    this._cleanupTransitionPhase();
    // Ensure the newly-visible view has been through a layout flush before we
    // attempt to focus anything in it.
    // See https://firefox-source-docs.mozilla.org/performance/bestpractices.html#detecting-and-avoiding-synchronous-reflow
    // for more information.
    await this.window.promiseDocumentFlushed(() => {});
    nextPanelView.focusSelectedElement();
  }

  /**
   * Attempt to clean up the attributes and properties set by `_transitionViews`
   * above. Which attributes and properties depends on the phase the transition
   * was left from.
   */
  _cleanupTransitionPhase() {
    if (!this._transitionDetails) {
      return;
    }

    let { phase, resolve, listener, cancelListener } = this._transitionDetails;
    this._transitionDetails = null;

    if (phase >= TRANSITION_PHASES.START) {
      this._panel.removeAttribute("width");
      this._panel.removeAttribute("height");
      this._viewContainer.style.removeProperty("height");
      this._viewContainer.style.removeProperty("width");
    }
    if (phase >= TRANSITION_PHASES.PREPARE) {
      this._transitioning = false;
      this._viewStack.style.removeProperty("margin-inline-start");
      this._viewStack.style.removeProperty("transition");
    }
    if (phase >= TRANSITION_PHASES.TRANSITION) {
      this._viewStack.style.removeProperty("transform");
      if (listener) {
        this._viewContainer.removeEventListener("transitionend", listener);
      }
      if (cancelListener) {
        this._viewContainer.removeEventListener(
          "transitioncancel",
          cancelListener
        );
      }
      if (resolve) {
        resolve();
      }
    }
  }

  _calculateMaxHeight(aEvent) {
    // While opening the panel, we have to limit the maximum height of any
    // view based on the space that will be available. We cannot just use
    // window.screen.availTop and availHeight because these may return an
    // incorrect value when the window spans multiple screens.
    let anchor = this._panel.anchorNode;
    let anchorRect = anchor.getBoundingClientRect();
    let screen = anchor.screen;

    // GetAvailRect returns screen-device pixels, which we can convert to CSS
    // pixels here.
    let availTop = {},
      availHeight = {};
    screen.GetAvailRect({}, availTop, {}, availHeight);
    let cssAvailTop = availTop.value / screen.defaultCSSScaleFactor;

    // The distance from the anchor to the available margin of the screen is
    // based on whether the panel will open towards the top or the bottom.
    let maxHeight;
    if (aEvent.alignmentPosition.startsWith("before_")) {
      maxHeight = anchor.screenY - cssAvailTop;
    } else {
      let anchorScreenBottom = anchor.screenY + anchorRect.height;
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
    // Only process actual popup events from the panel or events we generate
    // ourselves, but not from menus being shown from within the panel.
    if (
      aEvent.type.startsWith("popup") &&
      aEvent.target != this._panel &&
      aEvent.target != this.node
    ) {
      return;
    }
    switch (aEvent.type) {
      case "keydown":
        // Since we start listening for the "keydown" event when the popup is
        // already showing and stop listening when the panel is hidden, we
        // always have at least one view open.
        let currentView = this.openViews[this.openViews.length - 1];
        currentView.keyNavigation(aEvent);
        break;
      case "mousemove":
        this.openViews.forEach(panelView => {
          if (!panelView.ignoreMouseMove) {
            panelView.clearNavigation();
          }
        });
        break;
      case "popupshowing": {
        this._viewContainer.setAttribute("panelopen", "true");
        if (!this.node.hasAttribute("disablekeynav")) {
          // We add the keydown handler on the root so that it handles key
          // presses when a panel appears but doesn't get focus, as happens
          // when a button to open a panel is clicked with the mouse.
          // However, this means the listener is on an ancestor of the panel,
          // which means that handlers such as ToolbarKeyboardNavigator are
          // deeper in the tree. Therefore, this must be a capturing listener
          // so we get the event first.
          this.document.documentElement.addEventListener("keydown", this, true);
          this._panel.addEventListener("mousemove", this);
        }
        break;
      }
      case "popuppositioned": {
        if (this._panel.state == "showing") {
          let maxHeight = this._calculateMaxHeight(aEvent);
          this._viewStack.style.maxHeight = maxHeight + "px";
          this._offscreenViewStack.style.maxHeight = maxHeight + "px";
        }
        break;
      }
      case "popupshown":
        // The main view is always open and visible when the panel is first
        // shown, so we can check the height of the description elements it
        // contains and notify consumers using the ViewShown event. In order to
        // minimize flicker we need to allow synchronous reflows, and we still
        // make sure the ViewShown event is dispatched synchronously.
        let mainPanelView = this.openViews[0];
        this._activateView(mainPanelView);
        break;
      case "popuphidden": {
        // WebExtensions consumers can hide the popup from viewshowing, or
        // mid-transition, which disrupts our state:
        this._transitioning = false;
        this._viewContainer.removeAttribute("panelopen");
        this._cleanupTransitionPhase();
        this.document.documentElement.removeEventListener(
          "keydown",
          this,
          true
        );
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
export var PanelView = class extends AssociatedToNode {
  constructor(node) {
    super(node);

    /**
     * Indicates whether the view is active. When this is false, consumers can
     * wait for the ViewShown event to know when the view becomes active.
     */
    this.active = false;

    /**
     * Specifies whether the view should be focused when active. When this
     * is true, the first navigable element in the view will be focused
     * when the view becomes active. This should be set to true when the view
     * is activated from the keyboard. It will be set to false once the view
     * is active.
     */
    this.focusWhenActive = false;
  }

  /**
   * Indicates whether the view is open in the specified PanelMultiView object.
   */
  isOpenIn(panelMultiView) {
    return this.node.panelMultiView == panelMultiView.node;
  }

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

  /**
   * Determines whether the view is visible. Setting this to false also resets
   * the "active" property.
   */
  set visible(value) {
    if (value) {
      this.node.setAttribute("visible", true);
    } else {
      this.node.removeAttribute("visible");
      this.active = false;
      this.focusWhenActive = false;
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
   * Constrains the height of this view using the "min-height" and "max-height"
   * styles. Setting this to zero removes the constraints.
   */
  set minMaxHeight(value) {
    let style = this.node.style;
    if (value) {
      style.minHeight = style.maxHeight = value + "px";
    } else {
      style.removeProperty("min-height");
      style.removeProperty("max-height");
    }
  }

  /**
   * Adds a header with the given title, or removes it if the title is empty.
   */
  set headerText(value) {
    let ensureHeaderSeparator = headerNode => {
      if (headerNode.nextSibling.tagName != "toolbarseparator") {
        let separator = this.document.createXULElement("toolbarseparator");
        this.node.insertBefore(separator, headerNode.nextSibling);
      }
    };

    // If the header already exists, update or remove it as requested.
    let header = this.node.querySelector(".panel-header");
    if (header) {
      let headerInfoButton = header.querySelector(".panel-info-button");
      let headerBackButton = header.querySelector(".subviewbutton-back");
      if (headerBackButton && this.node.getAttribute("mainview")) {
        // A back button should not appear in a mainview.
        // This codepath can be reached if a user enters a panelview in
        // the overflow panel, and then unpins it back to the toolbar.
        headerBackButton.remove();
      }
      if (!this.node.getAttribute("mainview")) {
        if (value) {
          let headerWantsBackButton =
            headerInfoButton || header.hasAttribute("wants-back-button");
          if (headerWantsBackButton && !headerBackButton) {
            // If we're not in a mainview and an info button is present, or the
            // panel says that it wants one, that means the panel header is a
            // custom one and a back button should be added, if not already present.
            header.prepend(this.createHeaderBackButton());
          }
          // Set the header title based on the value given.
          header.querySelector(".panel-header > h1 > span").textContent = value;
          ensureHeaderSeparator(header);
        } else {
          if (header.nextSibling.tagName == "toolbarseparator") {
            header.nextSibling.remove();
          }
          header.remove();
        }
        return;
      } else if (!this.node.getAttribute("showheader")) {
        if (header.nextSibling.tagName == "toolbarseparator") {
          header.nextSibling.remove();
        }
        header.remove();
      }
    }

    // The header doesn't exist, only create it if needed.
    if (!value) {
      return;
    }

    header = this.document.createXULElement("box");
    header.classList.add("panel-header");

    let backButton = this.createHeaderBackButton();
    let h1 = this.document.createElement("h1");
    let span = this.document.createElement("span");
    span.textContent = value;
    h1.appendChild(span);

    header.append(backButton, h1);
    this.node.prepend(header);

    ensureHeaderSeparator(header);
  }

  /**
   * Creates and returns a panel header back toolbarbutton.
   */
  createHeaderBackButton() {
    let backButton = this.document.createXULElement("toolbarbutton");
    backButton.className =
      "subviewbutton subviewbutton-iconic subviewbutton-back";
    backButton.setAttribute("closemenu", "none");
    backButton.setAttribute("tabindex", "0");
    backButton.setAttribute(
      "aria-label",
      lazy.gBundle.GetStringFromName("panel.back")
    );
    backButton.addEventListener("command", () => {
      // The panelmultiview element may change if the view is reused.
      this.node.panelMultiView.goBack();
      backButton.blur();
    });
    return backButton;
  }

  /**
   * Also make sure that the correct method is called on CustomizableWidget.
   */
  dispatchCustomEvent(...args) {
    lazy.CustomizableUI.ensureSubviewListeners(this.node);
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
    let rect = this._getBoundsWithoutFlushing(this.node);
    this.knownWidth = rect.width;
    this.knownHeight = rect.height;
  }

  /**
   * Determine whether an element can only be navigated to with tab/shift+tab,
   * not the arrow keys.
   */
  _isNavigableWithTabOnly(element) {
    let tag = element.localName;
    return (
      tag == "menulist" ||
      tag == "radiogroup" ||
      tag == "input" ||
      tag == "textarea" ||
      // Allow tab to reach embedded documents.
      tag == "browser" ||
      tag == "iframe" ||
      // This is currently needed for the unified extensions panel to allow
      // users to use up/down arrow to more quickly move between the extension
      // items. See Bug 1784118
      element.dataset?.navigableWithTabOnly === "true"
    );
  }

  /**
   * Make a TreeWalker for keyboard navigation.
   *
   * @param {Boolean} arrowKey If `true`, elements only navigable with tab are
   *        excluded.
   */
  _makeNavigableTreeWalker(arrowKey) {
    let filter = node => {
      if (node.disabled) {
        return NodeFilter.FILTER_REJECT;
      }
      let bounds = this._getBoundsWithoutFlushing(node);
      if (bounds.width == 0 || bounds.height == 0) {
        return NodeFilter.FILTER_REJECT;
      }
      let isNavigableWithTabOnly = this._isNavigableWithTabOnly(node);
      // Early return when the node is navigable with tab only and we are using
      // arrow keys so that nodes like button, toolbarbutton, checkbox, etc.
      // can also be marked as "navigable with tab only", otherwise the next
      // condition will unconditionally make them focusable.
      if (arrowKey && isNavigableWithTabOnly) {
        return NodeFilter.FILTER_REJECT;
      }
      let localName = node.localName.toLowerCase();
      if (
        localName == "button" ||
        localName == "toolbarbutton" ||
        localName == "checkbox" ||
        localName == "a" ||
        localName == "moz-toggle" ||
        node.classList.contains("text-link") ||
        (!arrowKey && isNavigableWithTabOnly)
      ) {
        // Set the tabindex attribute to make sure the node is focusable.
        // Don't do this for browser and iframe elements because this breaks
        // tabbing behavior. They're already focusable anyway.
        if (
          localName != "browser" &&
          localName != "iframe" &&
          !node.hasAttribute("tabindex")
        ) {
          node.setAttribute("tabindex", "-1");
        }
        return NodeFilter.FILTER_ACCEPT;
      }
      return NodeFilter.FILTER_SKIP;
    };
    return this.document.createTreeWalker(
      this.node,
      NodeFilter.SHOW_ELEMENT,
      filter
    );
  }

  /**
   * Get a TreeWalker which finds elements navigable with tab/shift+tab.
   */
  get _tabNavigableWalker() {
    if (!this.__tabNavigableWalker) {
      this.__tabNavigableWalker = this._makeNavigableTreeWalker(false);
    }
    return this.__tabNavigableWalker;
  }

  /**
   * Get a TreeWalker which finds elements navigable with up/down arrow keys.
   */
  get _arrowNavigableWalker() {
    if (!this.__arrowNavigableWalker) {
      this.__arrowNavigableWalker = this._makeNavigableTreeWalker(true);
    }
    return this.__arrowNavigableWalker;
  }

  /**
   * Element that is currently selected with the keyboard, or null if no element
   * is selected. Since the reference is held weakly, it can become null or
   * undefined at any time.
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
   * Focuses and moves keyboard selection to the first navigable element.
   * This is a no-op if there are no navigable elements.
   *
   * @param {Boolean} homeKey   `true` if this is for the home key.
   * @param {Boolean} skipBack   `true` if the Back button should be skipped.
   */
  focusFirstNavigableElement(homeKey = false, skipBack = false) {
    // The home key is conceptually similar to the up/down arrow keys.
    let walker = homeKey
      ? this._arrowNavigableWalker
      : this._tabNavigableWalker;
    walker.currentNode = walker.root;
    this.selectedElement = walker.firstChild();
    if (
      skipBack &&
      walker.currentNode &&
      walker.currentNode.classList.contains("subviewbutton-back") &&
      walker.nextNode()
    ) {
      this.selectedElement = walker.currentNode;
    }
    this.focusSelectedElement(/* byKey */ true);
  }

  /**
   * Focuses and moves keyboard selection to the last navigable element.
   * This is a no-op if there are no navigable elements.
   *
   * @param {Boolean} endKey   `true` if this is for the end key.
   */
  focusLastNavigableElement(endKey = false) {
    // The end key is conceptually similar to the up/down arrow keys.
    let walker = endKey ? this._arrowNavigableWalker : this._tabNavigableWalker;
    walker.currentNode = walker.root;
    this.selectedElement = walker.lastChild();
    this.focusSelectedElement(/* byKey */ true);
  }

  /**
   * Based on going up or down, select the previous or next focusable element.
   *
   * @param {Boolean} isDown   whether we're going down (true) or up (false).
   * @param {Boolean} arrowKey   `true` if this is for the up/down arrow keys.
   *
   * @return {DOMNode} the element we selected.
   */
  moveSelection(isDown, arrowKey = false) {
    let walker = arrowKey
      ? this._arrowNavigableWalker
      : this._tabNavigableWalker;
    let oldSel = this.selectedElement;
    let newSel;
    if (oldSel) {
      walker.currentNode = oldSel;
      newSel = isDown ? walker.nextNode() : walker.previousNode();
    }
    // If we couldn't find something, select the first or last item:
    if (!newSel) {
      walker.currentNode = walker.root;
      newSel = isDown ? walker.firstChild() : walker.lastChild();
    }
    this.selectedElement = newSel;
    return newSel;
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
   * Key navigation is only enabled while the view is active, meaning that this
   * method will return early if it is invoked during a sliding transition.
   *
   * @param {KeyEvent} event
   */
  keyNavigation(event) {
    if (!this.active) {
      return;
    }

    let focus = this.document.activeElement;
    // Make sure the focus is actually inside the panel. (It might not be if
    // the panel was opened with the mouse.) If it isn't, we don't care
    // about it for our purposes.
    // We use Node.compareDocumentPosition because Node.contains doesn't
    // behave as expected for anonymous content; e.g. the input inside a
    // textbox.
    if (
      focus &&
      !(
        this.node.compareDocumentPosition(focus) &
        Node.DOCUMENT_POSITION_CONTAINED_BY
      )
    ) {
      focus = null;
    }

    // Some panels contain embedded documents. We can't manage
    // keyboard navigation within those.
    if (focus && (focus.tagName == "browser" || focus.tagName == "iframe")) {
      return;
    }

    let stop = () => {
      event.stopPropagation();
      event.preventDefault();
    };

    // If the focused element is only navigable with tab, it wants the arrow
    // keys, etc. We shouldn't handle any keys except tab and shift+tab.
    // We make a function for this for performance reasons: we only want to
    // check this for keys we potentially care about, not *all* keys.
    let tabOnly = () => {
      // We use the real focus rather than this.selectedElement because focus
      // might have been moved without keyboard navigation (e.g. mouse click)
      // and this.selectedElement is only updated for keyboard navigation.
      return focus && this._isNavigableWithTabOnly(focus);
    };

    // If a context menu is open, we must let it handle all keys.
    // Normally, this just happens, but because we have a capturing root
    // element keydown listener, our listener takes precedence.
    // Again, we only want to do this check on demand for performance.
    let isContextMenuOpen = () => {
      if (!focus) {
        return false;
      }
      let contextNode = focus.closest("[context]");
      if (!contextNode) {
        return false;
      }
      let context = contextNode.getAttribute("context");
      if (!context) {
        return false;
      }
      let popup = this.document.getElementById(context);
      return popup && popup.state == "open";
    };

    this.ignoreMouseMove = false;

    let keyCode = event.code;
    switch (keyCode) {
      case "ArrowDown":
      case "ArrowUp":
        if (tabOnly()) {
          break;
        }
      // Fall-through...
      case "Tab": {
        if (
          isContextMenuOpen() ||
          // Tab in an open menulist should close it.
          (focus && focus.localName == "menulist" && focus.open)
        ) {
          break;
        }
        stop();
        let isDown =
          keyCode == "ArrowDown" || (keyCode == "Tab" && !event.shiftKey);
        let button = this.moveSelection(isDown, keyCode != "Tab");
        Services.focus.setFocus(button, Services.focus.FLAG_BYKEY);
        break;
      }
      case "Home":
        if (tabOnly() || isContextMenuOpen()) {
          break;
        }
        stop();
        this.focusFirstNavigableElement(true);
        break;
      case "End":
        if (tabOnly() || isContextMenuOpen()) {
          break;
        }
        stop();
        this.focusLastNavigableElement(true);
        break;
      case "ArrowLeft":
      case "ArrowRight": {
        if (tabOnly() || isContextMenuOpen()) {
          break;
        }
        stop();
        if (
          (!this.window.RTL_UI && keyCode == "ArrowLeft") ||
          (this.window.RTL_UI && keyCode == "ArrowRight")
        ) {
          this.node.panelMultiView.goBack();
          break;
        }
        // If the current button is _not_ one that points to a subview, pressing
        // the arrow key shouldn't do anything.
        let button = this.selectedElement;
        if (!button || !button.classList.contains("subviewbutton-nav")) {
          break;
        }
      }
      // Fall-through...
      case "Space":
      case "NumpadEnter":
      case "Enter": {
        if (tabOnly() || isContextMenuOpen()) {
          break;
        }
        let button = this.selectedElement;
        if (!button || button?.localName == "moz-toggle") {
          break;
        }
        stop();

        this._doingKeyboardActivation = true;
        const details = {
          bubbles: true,
          ctrlKey: event.ctrlKey,
          altKey: event.altKey,
          shiftKey: event.shiftKey,
          metaKey: event.metaKey,
        };
        let dispEvent = new event.target.ownerGlobal.MouseEvent(
          "mousedown",
          details
        );
        button.dispatchEvent(dispEvent);
        // This event will trigger a command event too.
        dispEvent = new event.target.ownerGlobal.MouseEvent("click", details);
        button.dispatchEvent(dispEvent);
        this._doingKeyboardActivation = false;
        break;
      }
    }
  }

  /**
   * Focus the last selected element in the view, if any.
   *
   * @param byKey {Boolean} whether focus was moved by the user pressing a key.
   *                        Needed to ensure we show focus styles in the right cases.
   */
  focusSelectedElement(byKey = false) {
    let selected = this.selectedElement;
    if (selected) {
      let flag = byKey ? Services.focus.FLAG_BYKEY : 0;
      Services.focus.setFocus(selected, flag);
    }
  }

  /**
   * Clear all traces of keyboard navigation happening right now.
   */
  clearNavigation() {
    let selected = this.selectedElement;
    if (selected) {
      selected.blur();
      this.selectedElement = null;
    }
  }
};
