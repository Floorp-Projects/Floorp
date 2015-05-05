/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/devtools/Loader.jsm");
Cu.import("resource://gre/modules/devtools/Console.jsm");
Cu.import("resource:///modules/devtools/ViewHelpers.jsm");

devtools.lazyRequireGetter(this, "promise");
devtools.lazyRequireGetter(this, "EventEmitter",
                                 "devtools/toolkit/event-emitter");
devtools.lazyRequireGetter(this, "AnimationsFront",
                                 "devtools/server/actors/animation", true);

const require = devtools.require;

const STRINGS_URI = "chrome://browser/locale/devtools/animationinspector.properties";
const L10N = new ViewHelpers.L10N(STRINGS_URI);

// Global toolbox/inspector, set when startup is called.
let gToolbox, gInspector;

/**
 * Startup the animationinspector controller and view, called by the sidebar
 * widget when loading/unloading the iframe into the tab.
 */
let startup = Task.async(function*(inspector) {
  gInspector = inspector;
  gToolbox = inspector.toolbox;

  // Don't assume that AnimationsPanel is defined here, it's in another file.
  if (!typeof AnimationsPanel === "undefined") {
    throw new Error("AnimationsPanel was not loaded in the animationinspector window");
  }

  // Startup first initalizes the controller and then the panel, in sequence.
  // If you want to know when everything's ready, do:
  // AnimationsPanel.once(AnimationsPanel.PANEL_INITIALIZED)
  yield AnimationsController.initialize();
  yield AnimationsPanel.initialize();
});

/**
 * Shutdown the animationinspector controller and view, called by the sidebar
 * widget when loading/unloading the iframe into the tab.
 */
let shutdown = Task.async(function*() {
  yield AnimationsController.destroy();
  // Don't assume that AnimationsPanel is defined here, it's in another file.
  if (typeof AnimationsPanel !== "undefined") {
    yield AnimationsPanel.destroy();
  }
  gToolbox = gInspector = null;
});

// This is what makes the sidebar widget able to load/unload the panel.
function setPanel(panel) {
  return startup(panel).catch(Cu.reportError);
}
function destroy() {
  return shutdown().catch(Cu.reportError);
}

/**
 * The animationinspector controller's job is to retrieve AnimationPlayerFronts
 * from the server. It is also responsible for keeping the list of players up to
 * date when the node selection changes in the inspector, as well as making sure
 * no updates are done when the animationinspector sidebar panel is not visible.
 *
 * AnimationPlayerFronts are available in AnimationsController.animationPlayers.
 *
 * Note also that all AnimationPlayerFronts handled by the controller are set to
 * auto-refresh (except when the sidebar panel is not visible).
 *
 * Usage example:
 *
 * AnimationsController.on(AnimationsController.PLAYERS_UPDATED_EVENT, onPlayers);
 * function onPlayers() {
 *   for (let player of AnimationsController.animationPlayers) {
 *     // do something with player
 *   }
 * }
 */
let AnimationsController = {
  PLAYERS_UPDATED_EVENT: "players-updated",

  initialize: Task.async(function*() {
    if (this.initialized) {
      return this.initialized.promise;
    }
    this.initialized = promise.defer();

    this.onPanelVisibilityChange = this.onPanelVisibilityChange.bind(this);
    this.onNewNodeFront = this.onNewNodeFront.bind(this);
    this.onAnimationMutations = this.onAnimationMutations.bind(this);

    let target = gToolbox.target;
    this.animationsFront = new AnimationsFront(target.client, target.form);

    // Expose actor capabilities.
    this.hasToggleAll = yield target.actorHasMethod("animations", "toggleAll");
    this.hasSetCurrentTime = yield target.actorHasMethod("animationplayer",
                                                         "setCurrentTime");
    this.hasMutationEvents = yield target.actorHasMethod("animations",
                                                         "stopAnimationPlayerUpdates");
    this.hasSetPlaybackRate = yield target.actorHasMethod("animationplayer",
                                                          "setPlaybackRate");
    this.hasTargetNode = yield target.actorHasMethod("domwalker",
                                                     "getNodeFromActor");

    if (this.destroyed) {
      console.warn("Could not fully initialize the AnimationsController");
      return;
    }

    this.startListeners();
    yield this.onNewNodeFront();

    this.initialized.resolve();
  }),

  destroy: Task.async(function*() {
    if (!this.initialized) {
      return;
    }

    if (this.destroyed) {
      return this.destroyed.promise;
    }
    this.destroyed = promise.defer();

    this.stopListeners();
    yield this.destroyAnimationPlayers();
    this.nodeFront = null;

    if (this.animationsFront) {
      this.animationsFront.destroy();
      this.animationsFront = null;
    }

    this.destroyed.resolve();
  }),

  startListeners: function() {
    // Re-create the list of players when a new node is selected, except if the
    // sidebar isn't visible. And set the players to auto-refresh when needed.
    gInspector.selection.on("new-node-front", this.onNewNodeFront);
    gInspector.sidebar.on("select", this.onPanelVisibilityChange);
    gToolbox.on("select", this.onPanelVisibilityChange);
  },

  stopListeners: function() {
    gInspector.selection.off("new-node-front", this.onNewNodeFront);
    gInspector.sidebar.off("select", this.onPanelVisibilityChange);
    gToolbox.off("select", this.onPanelVisibilityChange);
    if (this.isListeningToMutations) {
      this.animationsFront.off("mutations", this.onAnimationMutations);
    }
  },

  isPanelVisible: function() {
    return gToolbox.currentToolId === "inspector" &&
           gInspector.sidebar &&
           gInspector.sidebar.getCurrentTabID() == "animationinspector";
  },

  onPanelVisibilityChange: Task.async(function*(e, id) {
    if (this.isPanelVisible()) {
      this.onNewNodeFront();
      this.startAllAutoRefresh();
    } else {
      this.stopAllAutoRefresh();
    }
  }),

  onNewNodeFront: Task.async(function*() {
    // Ignore if the panel isn't visible or the node selection hasn't changed.
    if (!this.isPanelVisible() || this.nodeFront === gInspector.selection.nodeFront) {
      return;
    }

    let done = gInspector.updating("animationscontroller");

    if(!gInspector.selection.isConnected() ||
       !gInspector.selection.isElementNode()) {
      yield this.destroyAnimationPlayers();
      this.emit(this.PLAYERS_UPDATED_EVENT);
      done();
      return;
    }

    this.nodeFront = gInspector.selection.nodeFront;
    yield this.refreshAnimationPlayers(this.nodeFront);
    this.emit(this.PLAYERS_UPDATED_EVENT, this.animationPlayers);

    done();
  }),

  /**
   * Toggle (pause/play) all animations in the current target.
   */
  toggleAll: function() {
    if (!this.hasToggleAll) {
      return promis.resolve();
    }

    return this.animationsFront.toggleAll().catch(Cu.reportError);
  },

  // AnimationPlayerFront objects are managed by this controller. They are
  // retrieved when refreshAnimationPlayers is called, stored in the
  // animationPlayers array, and destroyed when refreshAnimationPlayers is
  // called again.
  animationPlayers: [],

  refreshAnimationPlayers: Task.async(function*(nodeFront) {
    yield this.destroyAnimationPlayers();

    this.animationPlayers = yield this.animationsFront.getAnimationPlayersForNode(nodeFront);
    this.startAllAutoRefresh();

    // Start listening for animation mutations only after the first method call
    // otherwise events won't be sent.
    if (!this.isListeningToMutations && this.hasMutationEvents) {
      this.animationsFront.on("mutations", this.onAnimationMutations);
      this.isListeningToMutations = true;
    }
  }),

  onAnimationMutations: Task.async(function*(changes) {
    // Insert new players into this.animationPlayers when new animations are
    // added.
    for (let {type, player} of changes) {
      if (type === "added") {
        this.animationPlayers.push(player);
        player.startAutoRefresh();
      }

      if (type === "removed") {
        player.stopAutoRefresh();
        yield player.release();
        let index = this.animationPlayers.indexOf(player);
        this.animationPlayers.splice(index, 1);
      }
    }

    // Let the UI know the list has been updated.
    this.emit(this.PLAYERS_UPDATED_EVENT, this.animationPlayers);
  }),

  startAllAutoRefresh: function() {
    for (let front of this.animationPlayers) {
      front.startAutoRefresh();
    }
  },

  stopAllAutoRefresh: function() {
    for (let front of this.animationPlayers) {
      front.stopAutoRefresh();
    }
  },

  destroyAnimationPlayers: Task.async(function*() {
    // Let the server know that we're not interested in receiving updates about
    // players for the current node. We're either being destroyed or a new node
    // has been selected.
    if (this.hasMutationEvents) {
      yield this.animationsFront.stopAnimationPlayerUpdates();
    }
    this.stopAllAutoRefresh();
    for (let front of this.animationPlayers) {
      yield front.release();
    }
    this.animationPlayers = [];
  })
};

EventEmitter.decorate(AnimationsController);
