/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { FrontClassWithSpec } = require("devtools/shared/protocol");
const { gridSpec, layoutSpec } = require("devtools/shared/specs/layout");

const GridFront = FrontClassWithSpec(gridSpec, {
  form: function (form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this._form = form;
  },

  /**
   * In some cases, the GridActor already knows the NodeActor ID of the node where the
   * grid is located. In such cases, this getter returns the NodeFront for it.
   */
  get containerNodeFront() {
    if (!this._form.containerNodeActorID) {
      return null;
    }

    return this.conn.getActor(this._form.containerNodeActorID);
  },

  /**
   * Getter for the grid fragments data.
   */
  get gridFragments() {
    return this._form.gridFragments;
  }
});

const LayoutFront = FrontClassWithSpec(layoutSpec, {});

exports.GridFront = GridFront;
exports.LayoutFront = LayoutFront;
