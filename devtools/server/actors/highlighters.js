/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci } = require("chrome");

const EventEmitter = require("devtools/shared/event-emitter");
const events = require("sdk/event/core");
const protocol = require("devtools/shared/protocol");
const { Arg, Option, method, RetVal } = protocol;
const { isWindowIncluded } = require("devtools/shared/layout/utils");
const { highlighterSpec, customHighlighterSpec } = require("devtools/shared/specs/highlighters");
const { isXUL, isNodeValid } = require("./highlighters/utils/markup");
const { SimpleOutlineHighlighter } = require("./highlighters/simple-outline");

const HIGHLIGHTER_PICKED_TIMER = 1000;

/**
 * The registration mechanism for highlighters provide a quick way to
 * have modular highlighters, instead of a hard coded list.
 * It allow us to split highlighers in sub modules, and add them dynamically
 * using add-on (useful for 3rd party developers, or prototyping)
 *
 * Note that currently, highlighters added using add-ons, can only work on
 * Firefox desktop, or Fennec if the same add-on is installed in both.
 */
const highlighterTypes = new Map();

/**
 * Returns `true` if a highlighter for the given `typeName` is registered,
 * `false` otherwise.
 */
const isTypeRegistered = (typeName) => highlighterTypes.has(typeName);
exports.isTypeRegistered = isTypeRegistered;

/**
 * Registers a given constructor as highlighter, for the `typeName` given.
 * If no `typeName` is provided, is looking for a `typeName` property in
 * the prototype's constructor.
 */
const register = (constructor, typeName = constructor.prototype.typeName) => {
  if (!typeName) {
    throw Error("No type's name found, or provided.");
  }

  if (highlighterTypes.has(typeName)) {
    throw Error(`${typeName} is already registered.`);
  }

  highlighterTypes.set(typeName, constructor);
};
exports.register = register;

/**
 * The Highlighter is the server-side entry points for any tool that wishes to
 * highlight elements in some way in the content document.
 *
 * A little bit of vocabulary:
 * - <something>HighlighterActor classes are the actors that can be used from
 *   the client. They do very little else than instantiate a given
 *   <something>Highlighter and use it to highlight elements.
 * - <something>Highlighter classes aren't actors, they're just JS classes that
 *   know how to create and attach the actual highlighter elements on top of the
 *   content
 *
 * The most used highlighter actor is the HighlighterActor which can be
 * conveniently retrieved via the InspectorActor's 'getHighlighter' method.
 * The InspectorActor will always return the same instance of
 * HighlighterActor if asked several times and this instance is used in the
 * toolbox to highlighter elements's box-model from the markup-view,
 * layout-view, console, debugger, ... as well as select elements with the
 * pointer (pick).
 *
 * Other types of highlighter actors exist and can be accessed via the
 * InspectorActor's 'getHighlighterByType' method.
 */

/**
 * The HighlighterActor class
 */
var HighlighterActor = exports.HighlighterActor = protocol.ActorClassWithSpec(highlighterSpec, {
  initialize: function(inspector, autohide) {
    protocol.Actor.prototype.initialize.call(this, null);

    this._autohide = autohide;
    this._inspector = inspector;
    this._walker = this._inspector.walker;
    this._tabActor = this._inspector.tabActor;
    this._highlighterEnv = new HighlighterEnvironment();
    this._highlighterEnv.initFromTabActor(this._tabActor);

    this._highlighterReady = this._highlighterReady.bind(this);
    this._highlighterHidden = this._highlighterHidden.bind(this);
    this._onNavigate = this._onNavigate.bind(this);

    this._createHighlighter();

    // Listen to navigation events to switch from the BoxModelHighlighter to the
    // SimpleOutlineHighlighter, and back, if the top level window changes.
    events.on(this._tabActor, "navigate", this._onNavigate);
  },

  get conn() {
    return this._inspector && this._inspector.conn;
  },

  form: function() {
    return {
      actor: this.actorID,
      traits: {
        autoHideOnDestroy: true
      }
    };
  },

  _createHighlighter: function() {
    this._isPreviousWindowXUL = isXUL(this._tabActor.window);

    if (!this._isPreviousWindowXUL) {
      this._highlighter = new BoxModelHighlighter(this._highlighterEnv,
                                                  this._inspector);
      this._highlighter.on("ready", this._highlighterReady);
      this._highlighter.on("hide", this._highlighterHidden);
    } else {
      this._highlighter = new SimpleOutlineHighlighter(this._highlighterEnv);
    }
  },

  _destroyHighlighter: function() {
    if (this._highlighter) {
      if (!this._isPreviousWindowXUL) {
        this._highlighter.off("ready", this._highlighterReady);
        this._highlighter.off("hide", this._highlighterHidden);
      }
      this._highlighter.destroy();
      this._highlighter = null;
    }
  },

  _onNavigate: function({isTopLevel}) {
    // Skip navigation events for non top-level windows, or if the document
    // doesn't exist anymore.
    if (!isTopLevel || !this._tabActor.window.document.documentElement) {
      return;
    }

    // Only rebuild the highlighter if the window type changed.
    if (isXUL(this._tabActor.window) !== this._isPreviousWindowXUL) {
      this._destroyHighlighter();
      this._createHighlighter();
    }
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);

    this.hideBoxModel();
    this._destroyHighlighter();
    events.off(this._tabActor, "navigate", this._onNavigate);

    this._highlighterEnv.destroy();
    this._highlighterEnv = null;

    this._autohide = null;
    this._inspector = null;
    this._walker = null;
    this._tabActor = null;
  },

  /**
   * Display the box model highlighting on a given NodeActor.
   * There is only one instance of the box model highlighter, so calling this
   * method several times won't display several highlighters, it will just move
   * the highlighter instance to these nodes.
   *
   * @param NodeActor The node to be highlighted
   * @param Options See the request part for existing options. Note that not
   * all options may be supported by all types of highlighters.
   */
  showBoxModel: function(node, options = {}) {
    if (node && isNodeValid(node.rawNode)) {
      this._highlighter.show(node.rawNode, options);
    } else {
      this._highlighter.hide();
    }
  },

  /**
   * Hide the box model highlighting if it was shown before
   */
  hideBoxModel: function() {
    this._highlighter.hide();
  },

  /**
   * Returns `true` if the event was dispatched from a window included in
   * the current highlighter environment; or if the highlighter environment has
   * chrome privileges
   *
   * The method is specifically useful on B2G, where we do not want that events
   * from app or main process are processed if we're inspecting the content.
   *
   * @param {Event} event
   *          The event to allow
   * @return {Boolean}
   */
  _isEventAllowed: function({view}) {
    let { window } = this._highlighterEnv;

    return window instanceof Ci.nsIDOMChromeWindow ||
          isWindowIncluded(window, view);
  },

  /**
   * Pick a node on click, and highlight hovered nodes in the process.
   *
   * This method doesn't respond anything interesting, however, it starts
   * mousemove, and click listeners on the content document to fire
   * events and let connected clients know when nodes are hovered over or
   * clicked.
   *
   * Once a node is picked, events will cease, and listeners will be removed.
   */
  _isPicking: false,
  _hoveredNode: null,
  _currentNode: null,

  pick: function() {
    if (this._isPicking) {
      return null;
    }
    this._isPicking = true;

    this._preventContentEvent = event => {
      event.stopPropagation();
      event.preventDefault();
    };

    this._onPick = event => {
      this._preventContentEvent(event);

      if (!this._isEventAllowed(event)) {
        return;
      }

      this._stopPickerListeners();
      this._isPicking = false;
      if (this._autohide) {
        this._tabActor.window.setTimeout(() => {
          this._highlighter.hide();
        }, HIGHLIGHTER_PICKED_TIMER);
      }
      if (!this._currentNode) {
        this._currentNode = this._findAndAttachElement(event);
      }
      events.emit(this._walker, "picker-node-picked", this._currentNode);
    };

    this._onHovered = event => {
      this._preventContentEvent(event);

      if (!this._isEventAllowed(event)) {
        return;
      }

      this._currentNode = this._findAndAttachElement(event);
      if (this._hoveredNode !== this._currentNode.node) {
        this._highlighter.show(this._currentNode.node.rawNode);
        events.emit(this._walker, "picker-node-hovered", this._currentNode);
        this._hoveredNode = this._currentNode.node;
      }
    };

    this._onKey = event => {
      if (!this._currentNode || !this._isPicking) {
        return;
      }

      this._preventContentEvent(event);

      if (!this._isEventAllowed(event)) {
        return;
      }

      let currentNode = this._currentNode.node.rawNode;

      /**
       * KEY: Action/scope
       * LEFT_KEY: wider or parent
       * RIGHT_KEY: narrower or child
       * ENTER/CARRIAGE_RETURN: Picks currentNode
       * ESC: Cancels picker, picks currentNode
       */
      switch (event.keyCode) {
        // Wider.
        case Ci.nsIDOMKeyEvent.DOM_VK_LEFT:
          if (!currentNode.parentElement) {
            return;
          }
          currentNode = currentNode.parentElement;
          break;

        // Narrower.
        case Ci.nsIDOMKeyEvent.DOM_VK_RIGHT:
          if (!currentNode.children.length) {
            return;
          }

          // Set firstElementChild by default
          let child = currentNode.firstElementChild;
          // If currentNode is parent of hoveredNode, then
          // previously selected childNode is set
          let hoveredNode = this._hoveredNode.rawNode;
          for (let sibling of currentNode.children) {
            if (sibling.contains(hoveredNode) || sibling === hoveredNode) {
              child = sibling;
            }
          }

          currentNode = child;
          break;

        // Select the element.
        case Ci.nsIDOMKeyEvent.DOM_VK_RETURN:
          this._onPick(event);
          return;

        // Cancel pick mode.
        case Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE:
          this.cancelPick();
          events.emit(this._walker, "picker-node-canceled");
          return;

        default: return;
      }

      // Store currently attached element
      this._currentNode = this._walker.attachElement(currentNode);
      this._highlighter.show(this._currentNode.node.rawNode);
      events.emit(this._walker, "picker-node-hovered", this._currentNode);
    };

    this._startPickerListeners();

    return null;
  },

  _findAndAttachElement: function(event) {
    // originalTarget allows access to the "real" element before any retargeting
    // is applied, such as in the case of XBL anonymous elements.  See also
    // https://developer.mozilla.org/docs/XBL/XBL_1.0_Reference/Anonymous_Content#Event_Flow_and_Targeting
    let node = event.originalTarget || event.target;
    return this._walker.attachElement(node);
  },

  _startPickerListeners: function() {
    let target = this._highlighterEnv.pageListenerTarget;
    target.addEventListener("mousemove", this._onHovered, true);
    target.addEventListener("click", this._onPick, true);
    target.addEventListener("mousedown", this._preventContentEvent, true);
    target.addEventListener("mouseup", this._preventContentEvent, true);
    target.addEventListener("dblclick", this._preventContentEvent, true);
    target.addEventListener("keydown", this._onKey, true);
    target.addEventListener("keyup", this._preventContentEvent, true);
  },

  _stopPickerListeners: function() {
    let target = this._highlighterEnv.pageListenerTarget;
    target.removeEventListener("mousemove", this._onHovered, true);
    target.removeEventListener("click", this._onPick, true);
    target.removeEventListener("mousedown", this._preventContentEvent, true);
    target.removeEventListener("mouseup", this._preventContentEvent, true);
    target.removeEventListener("dblclick", this._preventContentEvent, true);
    target.removeEventListener("keydown", this._onKey, true);
    target.removeEventListener("keyup", this._preventContentEvent, true);
  },

  _highlighterReady: function() {
    events.emit(this._inspector.walker, "highlighter-ready");
  },

  _highlighterHidden: function() {
    events.emit(this._inspector.walker, "highlighter-hide");
  },

  cancelPick: function() {
    if (this._isPicking) {
      this._highlighter.hide();
      this._stopPickerListeners();
      this._isPicking = false;
      this._hoveredNode = null;
    }
  }
});

/**
 * A generic highlighter actor class that instantiate a highlighter given its
 * type name and allows to show/hide it.
 */
var CustomHighlighterActor = exports.CustomHighlighterActor = protocol.ActorClassWithSpec(customHighlighterSpec, {
  /**
   * Create a highlighter instance given its typename
   * The typename must be one of HIGHLIGHTER_CLASSES and the class must
   * implement constructor(tabActor), show(node), hide(), destroy()
   */
  initialize: function(inspector, typeName) {
    protocol.Actor.prototype.initialize.call(this, null);

    this._inspector = inspector;

    let constructor = highlighterTypes.get(typeName);
    if (!constructor) {
      let list = [...highlighterTypes.keys()];

      throw new Error(`${typeName} isn't a valid highlighter class (${list})`);
    }

    // The assumption is that all custom highlighters need the canvasframe
    // container to append their elements, so if this is a XUL window, bail out.
    if (!isXUL(this._inspector.tabActor.window)) {
      this._highlighterEnv = new HighlighterEnvironment();
      this._highlighterEnv.initFromTabActor(inspector.tabActor);
      this._highlighter = new constructor(this._highlighterEnv);
    } else {
      throw new Error("Custom " + typeName +
        "highlighter cannot be created in a XUL window");
    }
  },

  get conn() {
    return this._inspector && this._inspector.conn;
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
    this.finalize();
    this._inspector = null;
  },

  release: function() {},

  /**
   * Show the highlighter.
   * This calls through to the highlighter instance's |show(node, options)|
   * method.
   *
   * Most custom highlighters are made to highlight DOM nodes, hence the first
   * NodeActor argument (NodeActor as in
   * devtools/server/actor/inspector).
   * Note however that some highlighters use this argument merely as a context
   * node: the RectHighlighter for instance uses it to calculate the absolute
   * position of the provided rect. The SelectHighlighter uses it as a base node
   * to run the provided CSS selector on.
   *
   * @param {NodeActor} The node to be highlighted
   * @param {Object} Options for the custom highlighter
   * @return {Boolean} True, if the highlighter has been successfully shown
   * (FF41+)
   */
  show: function(node, options) {
    if (!node || !isNodeValid(node.rawNode) || !this._highlighter) {
      return false;
    }

    return this._highlighter.show(node.rawNode, options);
  },

  /**
   * Hide the highlighter if it was shown before
   */
  hide: function() {
    if (this._highlighter) {
      this._highlighter.hide();
    }
  },

  /**
   * Kill this actor. This method is called automatically just before the actor
   * is destroyed.
   */
  finalize: function() {
    if (this._highlighter) {
      this._highlighter.destroy();
      this._highlighter = null;
    }

    if (this._highlighterEnv) {
      this._highlighterEnv.destroy();
      this._highlighterEnv = null;
    }
  }
});

/**
 * The HighlighterEnvironment is an object that holds all the required data for
 * highlighters to work: the window, docShell, event listener target, ...
 * It also emits "will-navigate" and "navigate" events, similarly to the
 * TabActor.
 *
 * It can be initialized either from a TabActor (which is the most frequent way
 * of using it, since highlighters are usually initialized by the
 * HighlighterActor or CustomHighlighterActor, which have a tabActor reference).
 * It can also be initialized just with a window object (which is useful for
 * when a highlighter is used outside of the debugger server context, for
 * instance from a gcli command).
 */
function HighlighterEnvironment() {
  this.relayTabActorNavigate = this.relayTabActorNavigate.bind(this);
  this.relayTabActorWillNavigate = this.relayTabActorWillNavigate.bind(this);

  EventEmitter.decorate(this);
}

exports.HighlighterEnvironment = HighlighterEnvironment;

HighlighterEnvironment.prototype = {
  initFromTabActor: function(tabActor) {
    this._tabActor = tabActor;
    events.on(this._tabActor, "navigate", this.relayTabActorNavigate);
    events.on(this._tabActor, "will-navigate", this.relayTabActorWillNavigate);
  },

  initFromWindow: function(win) {
    this._win = win;

    // We need a progress listener to know when the window will navigate/has
    // navigated.
    let self = this;
    this.listener = {
      QueryInterface: XPCOMUtils.generateQI([
        Ci.nsIWebProgressListener,
        Ci.nsISupportsWeakReference,
        Ci.nsISupports
      ]),

      onStateChange: function(progress, request, flag) {
        let isStart = flag & Ci.nsIWebProgressListener.STATE_START;
        let isStop = flag & Ci.nsIWebProgressListener.STATE_STOP;
        let isWindow = flag & Ci.nsIWebProgressListener.STATE_IS_WINDOW;
        let isDocument = flag & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;

        if (progress.DOMWindow !== win) {
          return;
        }

        if (isDocument && isStart) {
          // One of the earliest events that tells us a new URI is being loaded
          // in this window.
          self.emit("will-navigate", {
            window: win,
            isTopLevel: true
          });
        }
        if (isWindow && isStop) {
          self.emit("navigate", {
            window: win,
            isTopLevel: true
          });
        }
      }
    };

    this.webProgress.addProgressListener(this.listener,
      Ci.nsIWebProgress.NOTIFY_STATUS |
      Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
      Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT);
  },

  get isInitialized() {
    return this._win || this._tabActor;
  },

  get window() {
    if (!this.isInitialized) {
      throw new Error("Initialize HighlighterEnvironment with a tabActor " +
        "or window first");
    }
    return this._tabActor ? this._tabActor.window : this._win;
  },

  get document() {
    return this.window.document;
  },

  get docShell() {
    return this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIWebNavigation)
                      .QueryInterface(Ci.nsIDocShell);
  },

  get webProgress() {
    return this.docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                        .getInterface(Ci.nsIWebProgress);
  },

  /**
   * Get the right target for listening to events on the page.
   * - If the environment was initialized from a TabActor *and* if we're in the
   *   Browser Toolbox (to inspect firefox desktop): the tabActor is the
   *   RootActor, in which case, the window property can be used to listen to
   *   events.
   * - With firefox desktop, that tabActor is a BrowserTabActor, and with B2G,
   *   a ContentActor (which overrides BrowserTabActor). In both cases we use
   *   the chromeEventHandler which gives us a target we can use to listen to
   *   events, even from nested iframes.
   * - If the environment was initialized from a window, we also use the
   *   chromeEventHandler.
   */
  get pageListenerTarget() {
    if (this._tabActor && this._tabActor.isRootActor) {
      return this.window;
    }
    return this.docShell.chromeEventHandler;
  },

  relayTabActorNavigate: function(data) {
    this.emit("navigate", data);
  },

  relayTabActorWillNavigate: function(data) {
    this.emit("will-navigate", data);
  },

  destroy: function() {
    if (this._tabActor) {
      events.off(this._tabActor, "navigate", this.relayTabActorNavigate);
      events.off(this._tabActor, "will-navigate", this.relayTabActorWillNavigate);
    }

    // In case the environment was initialized from a window, we need to remove
    // the progress listener.
    if (this._win) {
      try {
        this.webProgress.removeProgressListener(this.listener);
      } catch (e) {
        // Which may fail in case the window was already destroyed.
      }
    }

    this._tabActor = null;
    this._win = null;
  }
};

const { BoxModelHighlighter } = require("./highlighters/box-model");
register(BoxModelHighlighter);
exports.BoxModelHighlighter = BoxModelHighlighter;

const { CssTransformHighlighter } = require("./highlighters/css-transform");
register(CssTransformHighlighter);
exports.CssTransformHighlighter = CssTransformHighlighter;

const { SelectorHighlighter } = require("./highlighters/selector");
register(SelectorHighlighter);
exports.SelectorHighlighter = SelectorHighlighter;

const { RectHighlighter } = require("./highlighters/rect");
register(RectHighlighter);
exports.RectHighlighter = RectHighlighter;

const { GeometryEditorHighlighter } = require("./highlighters/geometry-editor");
register(GeometryEditorHighlighter);
exports.GeometryEditorHighlighter = GeometryEditorHighlighter;

const { RulersHighlighter } = require("./highlighters/rulers");
register(RulersHighlighter);
exports.RulersHighlighter = RulersHighlighter;

const { MeasuringToolHighlighter } = require("./highlighters/measuring-tool");
register(MeasuringToolHighlighter);
exports.MeasuringToolHighlighter = MeasuringToolHighlighter;
