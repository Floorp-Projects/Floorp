/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Ci, Cu } = require("chrome");

const EventEmitter = require("devtools/shared/event-emitter");
const protocol = require("devtools/shared/protocol");
const { customHighlighterSpec } = require("devtools/shared/specs/highlighters");

loader.lazyRequireGetter(
  this,
  "isXUL",
  "devtools/server/actors/highlighters/utils/markup",
  true
);

/**
 * The registration mechanism for highlighters provides a quick way to
 * have modular highlighters instead of a hard coded list.
 */
const highlighterTypes = new Map();

/**
 * Returns `true` if a highlighter for the given `typeName` is registered,
 * `false` otherwise.
 */
const isTypeRegistered = typeName => highlighterTypes.has(typeName);
exports.isTypeRegistered = isTypeRegistered;

/**
 * Registers a given constructor as highlighter, for the `typeName` given.
 */
const registerHighlighter = (typeName, modulePath) => {
  if (highlighterTypes.has(typeName)) {
    throw Error(`${typeName} is already registered.`);
  }

  highlighterTypes.set(typeName, modulePath);
};

/**
 * CustomHighlighterActor is a generic Actor that instantiates a custom implementation of
 * a highlighter class given its type name which must be registered in `highlighterTypes`.
 * CustomHighlighterActor proxies calls to methods of the highlighter class instance:
 * constructor(targetActor), show(node, options), hide(), destroy()
 */
exports.CustomHighlighterActor = protocol.ActorClassWithSpec(
  customHighlighterSpec,
  {
    /**
     * Create a highlighter instance given its typeName.
     */
    initialize(parent, typeName) {
      protocol.Actor.prototype.initialize.call(this, null);

      this._parent = parent;

      const modulePath = highlighterTypes.get(typeName);
      if (!modulePath) {
        const list = [...highlighterTypes.keys()];

        throw new Error(
          `${typeName} isn't a valid highlighter class (${list})`
        );
      }

      const constructor = require(modulePath)[typeName];
      // The assumption is that custom highlighters either need the canvasframe
      // container to append their elements and thus a non-XUL window or they have
      // to define a static XULSupported flag that indicates that the highlighter
      // supports XUL windows. Otherwise, bail out.
      if (!isXUL(this._parent.targetActor.window) || constructor.XULSupported) {
        this._highlighterEnv = new HighlighterEnvironment();
        this._highlighterEnv.initFromTargetActor(parent.targetActor);
        this._highlighter = new constructor(this._highlighterEnv);
        if (this._highlighter.on) {
          this._highlighter.on(
            "highlighter-event",
            this._onHighlighterEvent.bind(this)
          );
        }
      } else {
        throw new Error(
          "Custom " + typeName + "highlighter cannot be created in a XUL window"
        );
      }
    },

    get conn() {
      return this._parent && this._parent.conn;
    },

    destroy() {
      protocol.Actor.prototype.destroy.call(this);
      this.finalize();
      this._parent = null;
    },

    release() {},

    /**
     * Get current instance of the highlighter object.
     */
    get instance() {
      return this._highlighter;
    },

    /**
     * Show the highlighter.
     * This calls through to the highlighter instance's |show(node, options)|
     * method.
     *
     * Most custom highlighters are made to highlight DOM nodes, hence the first
     * NodeActor argument (NodeActor as in devtools/server/actor/inspector).
     * Note however that some highlighters use this argument merely as a context
     * node: The SelectorHighlighter for instance uses it as a base node to run the
     * provided CSS selector on.
     *
     * @param {NodeActor} The node to be highlighted
     * @param {Object} Options for the custom highlighter
     * @return {Boolean} True, if the highlighter has been successfully shown
     */
    show(node, options) {
      if (!this._highlighter) {
        return null;
      }

      const rawNode = node?.rawNode;

      return this._highlighter.show(rawNode, options);
    },

    /**
     * Hide the highlighter if it was shown before
     */
    hide() {
      if (this._highlighter) {
        this._highlighter.hide();
      }
    },

    /**
     * Upon receiving an event from the highlighter, forward it to the client.
     */
    _onHighlighterEvent(data) {
      this.emit("highlighter-event", data);
    },

    /**
     * Destroy the custom highlighter implementation.
     * This method is called automatically just before the actor is destroyed.
     */
    finalize() {
      if (this._highlighter) {
        if (this._highlighter.off) {
          this._highlighter.off(
            "highlighter-event",
            this._onHighlighterEvent.bind(this)
          );
        }
        this._highlighter.destroy();
        this._highlighter = null;
      }

      if (this._highlighterEnv) {
        this._highlighterEnv.destroy();
        this._highlighterEnv = null;
      }
    },
  }
);

/**
 * The HighlighterEnvironment is an object that holds all the required data for
 * highlighters to work: the window, docShell, event listener target, ...
 * It also emits "will-navigate", "navigate" and "window-ready" events,
 * similarly to the WindowGlobalTargetActor.
 *
 * It can be initialized either from a WindowGlobalTargetActor (which is the
 * most frequent way of using it, since highlighters are initialized by
 * CustomHighlighterActor, which has a targetActor reference).
 * It can also be initialized just with a window object (which is
 * useful for when a highlighter is used outside of the devtools server context.
 */
function HighlighterEnvironment() {
  this.relayTargetActorWindowReady = this.relayTargetActorWindowReady.bind(
    this
  );
  this.relayTargetActorNavigate = this.relayTargetActorNavigate.bind(this);
  this.relayTargetActorWillNavigate = this.relayTargetActorWillNavigate.bind(
    this
  );

  EventEmitter.decorate(this);
}

exports.HighlighterEnvironment = HighlighterEnvironment;

HighlighterEnvironment.prototype = {
  initFromTargetActor(targetActor) {
    this._targetActor = targetActor;
    this._targetActor.on("window-ready", this.relayTargetActorWindowReady);
    this._targetActor.on("navigate", this.relayTargetActorNavigate);
    this._targetActor.on("will-navigate", this.relayTargetActorWillNavigate);
  },

  initFromWindow(win) {
    this._win = win;

    // We need a progress listener to know when the window will navigate/has
    // navigated.
    const self = this;
    this.listener = {
      QueryInterface: ChromeUtils.generateQI([
        "nsIWebProgressListener",
        "nsISupportsWeakReference",
      ]),

      onStateChange(progress, request, flag) {
        const isStart = flag & Ci.nsIWebProgressListener.STATE_START;
        const isStop = flag & Ci.nsIWebProgressListener.STATE_STOP;
        const isWindow = flag & Ci.nsIWebProgressListener.STATE_IS_WINDOW;
        const isDocument = flag & Ci.nsIWebProgressListener.STATE_IS_DOCUMENT;

        if (progress.DOMWindow !== win) {
          return;
        }

        if (isDocument && isStart) {
          // One of the earliest events that tells us a new URI is being loaded
          // in this window.
          self.emit("will-navigate", {
            window: win,
            isTopLevel: true,
          });
        }
        if (isWindow && isStop) {
          self.emit("navigate", {
            window: win,
            isTopLevel: true,
          });
        }
      },
    };

    this.webProgress.addProgressListener(
      this.listener,
      Ci.nsIWebProgress.NOTIFY_STATE_WINDOW |
        Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT
    );
  },

  get isInitialized() {
    return this._win || this._targetActor;
  },

  get isXUL() {
    return isXUL(this.window);
  },

  get window() {
    if (!this.isInitialized) {
      throw new Error(
        "Initialize HighlighterEnvironment with a targetActor " +
          "or window first"
      );
    }
    const win = this._targetActor ? this._targetActor.window : this._win;

    try {
      return Cu.isDeadWrapper(win) ? null : win;
    } catch (e) {
      // win is null
      return null;
    }
  },

  get document() {
    return this.window && this.window.document;
  },

  get docShell() {
    return this.window && this.window.docShell;
  },

  get webProgress() {
    return (
      this.docShell &&
      this.docShell
        .QueryInterface(Ci.nsIInterfaceRequestor)
        .getInterface(Ci.nsIWebProgress)
    );
  },

  /**
   * Get the right target for listening to events on the page.
   * - If the environment was initialized from a WindowGlobalTargetActor
   *   *and* if we're in the Browser Toolbox (to inspect Firefox Desktop): the
   *   targetActor is the RootActor, in which case, the window property can be
   *   used to listen to events.
   * - With Firefox Desktop, the targetActor is a WindowGlobalTargetActor, and we use
   *   the chromeEventHandler which gives us a target we can use to listen to
   *   events, even from nested iframes.
   * - If the environment was initialized from a window, we also use the
   *   chromeEventHandler.
   */
  get pageListenerTarget() {
    if (this._targetActor && this._targetActor.isRootActor) {
      return this.window;
    }
    return this.docShell && this.docShell.chromeEventHandler;
  },

  relayTargetActorWindowReady(data) {
    this.emit("window-ready", data);
  },

  relayTargetActorNavigate(data) {
    this.emit("navigate", data);
  },

  relayTargetActorWillNavigate(data) {
    this.emit("will-navigate", data);
  },

  destroy() {
    if (this._targetActor) {
      this._targetActor.off("window-ready", this.relayTargetActorWindowReady);
      this._targetActor.off("navigate", this.relayTargetActorNavigate);
      this._targetActor.off("will-navigate", this.relayTargetActorWillNavigate);
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

    this._targetActor = null;
    this._win = null;
  },
};

// This constant object is created to make the calls array more
// readable. Otherwise, linting rules force some array defs to span 4
// lines instead, which is much harder to parse.
const HIGHLIGHTERS = {
  accessible: "devtools/server/actors/highlighters/accessible",
  boxModel: "devtools/server/actors/highlighters/box-model",
  cssGrid: "devtools/server/actors/highlighters/css-grid",
  cssTransform: "devtools/server/actors/highlighters/css-transform",
  eyeDropper: "devtools/server/actors/highlighters/eye-dropper",
  flexbox: "devtools/server/actors/highlighters/flexbox",
  fonts: "devtools/server/actors/highlighters/fonts",
  geometryEditor: "devtools/server/actors/highlighters/geometry-editor",
  measuringTool: "devtools/server/actors/highlighters/measuring-tool",
  pausedDebugger: "devtools/server/actors/highlighters/paused-debugger",
  rulers: "devtools/server/actors/highlighters/rulers",
  selector: "devtools/server/actors/highlighters/selector",
  shapes: "devtools/server/actors/highlighters/shapes",
  tabbingOrder: "devtools/server/actors/highlighters/tabbing-order",
};

// Each array in this array is called as register(arr[0], arr[1]).
const registerCalls = [
  ["AccessibleHighlighter", HIGHLIGHTERS.accessible],
  ["BoxModelHighlighter", HIGHLIGHTERS.boxModel],
  ["CssGridHighlighter", HIGHLIGHTERS.cssGrid],
  ["CssTransformHighlighter", HIGHLIGHTERS.cssTransform],
  ["EyeDropper", HIGHLIGHTERS.eyeDropper],
  ["FlexboxHighlighter", HIGHLIGHTERS.flexbox],
  ["FontsHighlighter", HIGHLIGHTERS.fonts],
  ["GeometryEditorHighlighter", HIGHLIGHTERS.geometryEditor],
  ["MeasuringToolHighlighter", HIGHLIGHTERS.measuringTool],
  ["PausedDebuggerOverlay", HIGHLIGHTERS.pausedDebugger],
  ["RulersHighlighter", HIGHLIGHTERS.rulers],
  ["SelectorHighlighter", HIGHLIGHTERS.selector],
  ["ShapesHighlighter", HIGHLIGHTERS.shapes],
  ["TabbingOrderHighlighter", HIGHLIGHTERS.tabbingOrder],
];

// Register each highlighter above.
registerCalls.forEach(arr => {
  registerHighlighter(arr[0], arr[1]);
});
