/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { safeAsyncMethod } = require("devtools/shared/async-utils");
const {
  FrontClassWithSpec,
  registerFront,
} = require("devtools/shared/protocol");
const {
  flexboxSpec,
  flexItemSpec,
  gridSpec,
  layoutSpec,
} = require("devtools/shared/specs/layout");

class FlexboxFront extends FrontClassWithSpec(flexboxSpec) {
  form(form) {
    this._form = form;
  }

  /**
   * In some cases, the FlexboxActor already knows the NodeActor ID of the node where the
   * flexbox is located. In such cases, this getter returns the NodeFront for it.
   */
  get containerNodeFront() {
    if (!this._form.containerNodeActorID) {
      return null;
    }

    return this.conn.getFrontByID(this._form.containerNodeActorID);
  }

  /**
   * Get the WalkerFront instance that owns this FlexboxFront.
   */
  get walkerFront() {
    return this.parentFront.walkerFront;
  }

  /**
   * Get the computed style properties for the flex container.
   */
  get properties() {
    return this._form.properties;
  }
}

class FlexItemFront extends FrontClassWithSpec(flexItemSpec) {
  form(form) {
    this._form = form;
  }

  /**
   * Get the flex item sizing data.
   */
  get flexItemSizing() {
    return this._form.flexItemSizing;
  }

  /**
   * In some cases, the FlexItemActor already knows the NodeActor ID of the node where the
   * flex item is located. In such cases, this getter returns the NodeFront for it.
   */
  get nodeFront() {
    if (!this._form.nodeActorID) {
      return null;
    }

    return this.conn.getFrontByID(this._form.nodeActorID);
  }

  /**
   * Get the WalkerFront instance that owns this FlexItemFront.
   */
  get walkerFront() {
    return this.parentFront.walkerFront;
  }

  /**
   * Get the computed style properties for the flex item.
   */
  get computedStyle() {
    return this._form.computedStyle;
  }

  /**
   * Get the style properties for the flex item.
   */
  get properties() {
    return this._form.properties;
  }
}

class GridFront extends FrontClassWithSpec(gridSpec) {
  form(form) {
    this._form = form;
  }

  /**
   * In some cases, the GridActor already knows the NodeActor ID of the node where the
   * grid is located. In such cases, this getter returns the NodeFront for it.
   */
  get containerNodeFront() {
    if (!this._form.containerNodeActorID) {
      return null;
    }

    return this.conn.getFrontByID(this._form.containerNodeActorID);
  }

  /**
   * Get the WalkerFront instance that owns this GridFront.
   */
  get walkerFront() {
    return this.parentFront.walkerFront;
  }

  /**
   * Get the text direction of the grid container.
   */
  get direction() {
    return this._form.direction;
  }

  /**
   * Getter for the grid fragments data.
   */
  get gridFragments() {
    return this._form.gridFragments;
  }

  /**
   * Get whether or not the grid is a subgrid.
   */
  get isSubgrid() {
    return !!this._form.isSubgrid;
  }

  /**
   * Get the writing mode of the grid container.
   */
  get writingMode() {
    return this._form.writingMode;
  }
}

class LayoutFront extends FrontClassWithSpec(layoutSpec) {
  constructor(client, targetFront, parentFront) {
    super(client, targetFront, parentFront);

    this.getAllGrids = safeAsyncMethod(
      this.getAllGrids.bind(this),
      () => this.isDestroyed(),
      []
    );
  }
  /**
   * Get the WalkerFront instance that owns this LayoutFront.
   */
  get walkerFront() {
    return this.parentFront;
  }

  getAllGrids() {
    if (!this.walkerFront.rootNode) {
      return [];
    }
    return this.getGrids(this.walkerFront.rootNode);
  }
}

exports.FlexboxFront = FlexboxFront;
registerFront(FlexboxFront);
exports.FlexItemFront = FlexItemFront;
registerFront(FlexItemFront);
exports.GridFront = GridFront;
registerFront(GridFront);
exports.LayoutFront = LayoutFront;
registerFront(LayoutFront);
