/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AnimationsFront } = require("devtools/shared/fronts/animation");
const { createElement, createFactory } = require("devtools/client/shared/vendor/react");
const { Provider } = require("devtools/client/shared/vendor/react-redux");

const App = createFactory(require("./components/App"));
const { updateAnimations } = require("./actions/animations");

class AnimationInspector {
  constructor(inspector) {
    this.inspector = inspector;
    this.update = this.update.bind(this);

    this.init();
  }

  init() {
    const target = this.inspector.target;
    this.animationsFront = new AnimationsFront(target.client, target.form);

    const provider = createElement(Provider, {
      id: "newanimationinspector",
      key: "newanimationinspector",
      store: this.inspector.store
    }, App());
    this.provider = provider;

    this.inspector.selection.on("new-node-front", this.update);
    this.inspector.sidebar.on("newanimationinspector-selected", this.update);
  }

  destroy() {
    this.inspector.selection.off("new-node-front", this.update);
    this.inspector.sidebar.off("newanimationinspector-selected", this.update);

    this.inspector = null;
  }

  async update() {
    const selection = this.inspector.selection;
    const animations =
      selection.isConnected() && selection.isElementNode()
      ? await this.animationsFront.getAnimationPlayersForNode(selection.nodeFront)
      : [];

    this.inspector.store.dispatch(updateAnimations(animations));
  }
}

module.exports = AnimationInspector;
