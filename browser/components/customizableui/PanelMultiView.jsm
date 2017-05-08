/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["PanelMultiView"];

/**
 * This is the implementation of the panelUI.xml XBL binding, moved to this
 * module, to make it easier to fork the logic for the newer photon structure.
 * Goals are:
 * 1. to make it easier to programmatically extend the list of panels,
 * 2. allow for navigation between panels multiple levels deep and
 * 3. maintain the pre-photon structure with as little effort possible.
 *
 * @type {PanelMultiView}
 */
this.PanelMultiView = class {
  get document() {
    return this.node.ownerDocument;
  }

  get window() {
    return this.node.ownerGlobal;
  }

  get _panel() {
    return this.node.parentNode;
  }

  get showingSubView() {
    return this._viewStack.getAttribute("viewtype") == "subview";
  }
  get _mainViewId() {
    return this.node.getAttribute("mainViewId");
  }
  set _mainViewId(val) {
    this.node.setAttribute("mainViewId", val);
    return val;
  }
  get _mainView() {
    return this._mainViewId ? this.document.getElementById(this._mainViewId) : null;
  }
  get showingSubViewAsMainView() {
    return this.node.getAttribute("mainViewIsSubView") == "true";
  }

  get ignoreMutations() {
    return this._ignoreMutations;
  }
  set ignoreMutations(val) {
    this._ignoreMutations = val;
    if (!val && this._panel.state == "open") {
      if (this.showingSubView) {
        this._syncContainerWithSubView();
      } else {
        this._syncContainerWithMainView();
      }
    }
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

  constructor(xulNode) {
    this.node = xulNode;

    this._currentSubView = this._anchorElement = this._subViewObserver = null;
    this._mainViewHeight = 0;
    this.__transitioning = this._ignoreMutations = false;

    const {document, window} = this;

    this._clickCapturer =
      document.getAnonymousElementByAttribute(this.node, "anonid", "clickCapturer");
    this._viewContainer =
      document.getAnonymousElementByAttribute(this.node, "anonid", "viewContainer");
    this._mainViewContainer =
      document.getAnonymousElementByAttribute(this.node, "anonid", "mainViewContainer");
    this._subViews =
      document.getAnonymousElementByAttribute(this.node, "anonid", "subViews");
    this._viewStack =
      document.getAnonymousElementByAttribute(this.node, "anonid", "viewStack");

    this._clickCapturer.addEventListener("click", this);
    this._panel.addEventListener("popupshowing", this);
    this._panel.addEventListener("popupshown", this);
    this._panel.addEventListener("popuphidden", this);
    this._subViews.addEventListener("overflow", this);
    this._mainViewContainer.addEventListener("overflow", this);

    // Get a MutationObserver ready to react to subview size changes. We
    // only attach this MutationObserver when a subview is being displayed.
    this._subViewObserver = new window.MutationObserver(this._syncContainerWithSubView.bind(this));
    this._mainViewObserver = new window.MutationObserver(this._syncContainerWithMainView.bind(this));

    this._mainViewContainer.setAttribute("panelid", this._panel.id);

    if (this._mainView) {
      this.setMainView(this._mainView);
    }
    this.node.setAttribute("viewtype", "main");

    // Proxy these public properties and methods, as used elsewhere by various
    // parts of the browser, to this instance.
    ["_mainView", "ignoreMutations", "showingSubView"].forEach(property => {
      Object.defineProperty(this.node, property, {
        enumerable: true,
        get: () => this[property],
        set: (val) => this[property] = val
      });
    });
    ["setHeightToFit", "setMainView", "showMainView", "showSubView"].forEach(method => {
      Object.defineProperty(this.node, method, {
        enumerable: true,
        value: (...args) => this[method](...args)
      });
    });
  }

  destructor() {
    if (this._mainView) {
      this._mainView.removeAttribute("mainview");
    }
    this._mainViewObserver.disconnect();
    this._subViewObserver.disconnect();
    this._panel.removeEventListener("popupshowing", this);
    this._panel.removeEventListener("popupshown", this);
    this._panel.removeEventListener("popuphidden", this);
    this._subViews.removeEventListener("overflow", this);
    this._mainViewContainer.removeEventListener("overflow", this);
    this._clickCapturer.removeEventListener("click", this);

    this.node = this.__clickCapturer = this.__viewContainer = this.__mainViewContainer =
      this.__subViews = this.__viewStack = null;
  }

  setMainView(aNewMainView) {
    if (this._mainView) {
      this._mainViewObserver.disconnect();
      this._subViews.appendChild(this._mainView);
      this._mainView.removeAttribute("mainview");
    }
    this._mainViewId = aNewMainView.id;
    aNewMainView.setAttribute("mainview", "true");
    this._mainViewContainer.appendChild(aNewMainView);
  }

  showMainView() {
    if (this.showingSubView) {
      let viewNode = this._currentSubView;
      let evt = new this.window.CustomEvent("ViewHiding", { bubbles: true, cancelable: true });
      viewNode.dispatchEvent(evt);

      viewNode.removeAttribute("current");
      this._currentSubView = null;

      this._subViewObserver.disconnect();

      this._setViewContainerHeight(this._mainViewHeight);

      this.node.setAttribute("viewtype", "main");
    }

    this._shiftMainView();
  }

  showSubView(aViewId, aAnchor) {
    const {document, window} = this;
    window.Task.spawn(function*() {
      let viewNode = this.node.querySelector("#" + aViewId);
      if (!viewNode) {
        viewNode = document.getElementById(aViewId);
        if (viewNode) {
          this._subViews.appendChild(viewNode);
        } else {
          throw new Error(`Subview ${aViewId} doesn't exist!`);
        }
      }
      viewNode.setAttribute("current", true);
      // Emit the ViewShowing event so that the widget definition has a chance
      // to lazily populate the subview with things.
      let detail = {
        blockers: new Set(),
        addBlocker(aPromise) {
          this.blockers.add(aPromise);
        },
      };

      let evt = new window.CustomEvent("ViewShowing", { bubbles: true, cancelable: true, detail });
      viewNode.dispatchEvent(evt);

      let cancel = evt.defaultPrevented;
      if (detail.blockers.size) {
        try {
          let results = yield window.Promise.all(detail.blockers);
          cancel = cancel || results.some(val => val === false);
        } catch (e) {
          Components.utils.reportError(e);
          cancel = true;
        }
      }

      if (cancel) {
        return;
      }

      this._currentSubView = viewNode;

      // Now we have to transition the panel. There are a few parts to this:
      //
      // 1) The main view content gets shifted so that the center of the anchor
      //    node is at the left-most edge of the panel.
      // 2) The subview deck slides in so that it takes up almost all of the
      //    panel.
      // 3) If the subview is taller then the main panel contents, then the panel
      //    must grow to meet that new height. Otherwise, it must shrink.
      //
      // All three of these actions make use of CSS transformations, so they
      // should all occur simultaneously.
      this.node.setAttribute("viewtype", "subview");
      this._shiftMainView(aAnchor);

      this._mainViewHeight = this._viewStack.clientHeight;

      let newHeight = this._heightOfSubview(viewNode, this._subViews);
      this._setViewContainerHeight(newHeight);

      this._subViewObserver.observe(viewNode, {
        attributes: true,
        characterData: true,
        childList: true,
        subtree: true
      });
    }.bind(this));
  }

  _setViewContainerHeight(aHeight) {
    let container = this._viewContainer;
    this._transitioning = true;

    let onTransitionEnd = () => {
      container.removeEventListener("transitionend", onTransitionEnd);
      this._transitioning = false;
    };

    container.addEventListener("transitionend", onTransitionEnd);
    container.style.height = `${aHeight}px`;
  }

  _shiftMainView(aAnchor) {
    if (aAnchor) {
      // We need to find the edge of the anchor, relative to the main panel.
      // Then we need to add half the width of the anchor. This is the target
      // that we need to transition to.
      let anchorRect = aAnchor.getBoundingClientRect();
      let mainViewRect = this._mainViewContainer.getBoundingClientRect();
      let center = aAnchor.clientWidth / 2;
      let direction = aAnchor.ownerGlobal.getComputedStyle(aAnchor).direction;
      let edge;
      if (direction == "ltr") {
        edge = anchorRect.left - mainViewRect.left;
      } else {
        edge = mainViewRect.right - anchorRect.right;
      }

      // If the anchor is an element on the far end of the mainView we
      // don't want to shift the mainView too far, we would reveal empty
      // space otherwise.
      let cstyle = this.window.getComputedStyle(this.document.documentElement);
      let exitSubViewGutterWidth =
        cstyle.getPropertyValue("--panel-ui-exit-subview-gutter-width");
      let maxShift = mainViewRect.width - parseInt(exitSubViewGutterWidth);
      let target = Math.min(maxShift, edge + center);

      let neg = direction == "ltr" ? "-" : "";
      this._mainViewContainer.style.transform = `translateX(${neg}${target}px)`;
      aAnchor.setAttribute("panel-multiview-anchor", true);
    } else {
      this._mainViewContainer.style.transform = "";
      if (this.anchorElement)
        this.anchorElement.removeAttribute("panel-multiview-anchor");
    }
    this.anchorElement = aAnchor;
  }

  handleEvent(aEvent) {
    if (aEvent.type.startsWith("popup") && aEvent.target != this._panel) {
      // Shouldn't act on e.g. context menus being shown from within the panel.
      return;
    }
    switch (aEvent.type) {
      case "click":
        if (aEvent.originalTarget == this._clickCapturer) {
          this.showMainView();
        }
        break;
      case "overflow":
        if (aEvent.target.localName == "vbox") {
          // Resize the right view on the next tick.
          if (this.showingSubView) {
            this.window.setTimeout(this._syncContainerWithSubView.bind(this), 0);
          } else if (!this.transitioning) {
            this.window.setTimeout(this._syncContainerWithMainView.bind(this), 0);
          }
        }
        break;
      case "popupshowing":
        this.node.setAttribute("panelopen", "true");
        // Bug 941196 - The panel can get taller when opening a subview. Disabling
        // autoPositioning means that the panel won't jump around if an opened
        // subview causes the panel to exceed the dimensions of the screen in the
        // direction that the panel originally opened in. This property resets
        // every time the popup closes, which is why we have to set it each time.
        this._panel.autoPosition = false;
        this._syncContainerWithMainView();

        this._mainViewObserver.observe(this._mainView, {
          attributes: true,
          characterData: true,
          childList: true,
          subtree: true
        });

        break;
      case "popupshown":
        this._setMaxHeight();
        break;
      case "popuphidden":
        this.node.removeAttribute("panelopen");
        this._mainView.style.removeProperty("height");
        this.showMainView();
        this._mainViewObserver.disconnect();
        break;
    }
  }

  _shouldSetPosition() {
    return this.node.getAttribute("nosubviews") == "true";
  }

  _shouldSetHeight() {
    return this.node.getAttribute("nosubviews") != "true";
  }

  _setMaxHeight() {
    if (!this._shouldSetHeight())
      return;

    // Ignore the mutation that'll fire when we set the height of
    // the main view.
    this.ignoreMutations = true;
    this._mainView.style.height = this.node.getBoundingClientRect().height + "px";
    this.ignoreMutations = false;
  }

  _adjustContainerHeight() {
    if (!this.ignoreMutations && !this.showingSubView && !this._transitioning) {
      let height;
      if (this.showingSubViewAsMainView) {
        height = this._heightOfSubview(this._mainView);
      } else {
        height = this._mainView.scrollHeight;
      }
      this._viewContainer.style.height = height + "px";
    }
  }

  _syncContainerWithSubView() {
    // Check that this panel is still alive:
    if (!this._panel || !this._panel.parentNode) {
      return;
    }

    if (!this.ignoreMutations && this.showingSubView) {
      let newHeight = this._heightOfSubview(this._currentSubView, this._subViews);
      this._viewContainer.style.height = newHeight + "px";
    }
  }

  _syncContainerWithMainView() {
    // Check that this panel is still alive:
    if (!this._panel || !this._panel.parentNode) {
      return;
    }

    if (this._shouldSetPosition()) {
      this._panel.adjustArrowPosition();
    }

    if (this._shouldSetHeight()) {
      this._adjustContainerHeight();
    }
  }

  /**
   * Call this when the height of one of your views (the main view or a
   * subview) changes and you want the heights of the multiview and panel
   * to be the same as the view's height.
   * If the caller can give a hint of the expected height change with the
   * optional aExpectedChange parameter, it prevents flicker.
   */
  setHeightToFit(aExpectedChange) {
    // Set the max-height to zero, wait until the height is actually
    // updated, and then remove it.  If it's not removed, weird things can
    // happen, like widgets in the panel won't respond to clicks even
    // though they're visible.
    const {window} = this;
    let count = 5;
    let height = window.getComputedStyle(this.node).height;
    if (aExpectedChange)
      this.node.style.maxHeight = (parseInt(height, 10) + aExpectedChange) + "px";
    else
      this.node.style.maxHeight = "0";
    let interval = window.setInterval(() => {
      if (height != window.getComputedStyle(this.node).height || --count == 0) {
        window.clearInterval(interval);
        this.node.style.removeProperty("max-height");
      }
    }, 0);
  }

  _heightOfSubview(aSubview, aContainerToCheck) {
    function getFullHeight(element) {
      // XXXgijs: unfortunately, scrollHeight rounds values, and there's no alternative
      // that works with overflow: auto elements. Fortunately for us,
      // we have exactly 1 (potentially) scrolling element in here (the subview body),
      // and rounding 1 value is OK - rounding more than 1 and adding them means we get
      // off-by-1 errors. Now we might be off by a subpixel, but we care less about that.
      // So, use scrollHeight *only* if the element is vertically scrollable.
      let height;
      let elementCS;
      if (element.scrollTopMax) {
        height = element.scrollHeight;
        // Bounding client rects include borders, scrollHeight doesn't:
        elementCS = win.getComputedStyle(element);
        height += parseFloat(elementCS.borderTopWidth) +
                  parseFloat(elementCS.borderBottomWidth);
      } else {
        height = element.getBoundingClientRect().height;
        if (height > 0) {
          elementCS = win.getComputedStyle(element);
        }
      }
      if (elementCS) {
        // Include margins - but not borders or paddings because they
        // were dealt with above.
        height += parseFloat(elementCS.marginTop) + parseFloat(elementCS.marginBottom);
      }
      return height;
    }
    let win = aSubview.ownerGlobal;
    let body = aSubview.querySelector(".panel-subview-body");
    let height = getFullHeight(body || aSubview);
    if (body) {
      let header = aSubview.querySelector(".panel-subview-header");
      let footer = aSubview.querySelector(".panel-subview-footer");
      height += (header ? getFullHeight(header) : 0) +
                (footer ? getFullHeight(footer) : 0);
    }
    if (aContainerToCheck) {
      let containerCS = win.getComputedStyle(aContainerToCheck);
      height += parseFloat(containerCS.paddingTop) + parseFloat(containerCS.paddingBottom);
    }
    return Math.ceil(height);
  }
}
