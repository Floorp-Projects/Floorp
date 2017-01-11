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
   * Getter for the grid fragments data.
   */
  get gridFragments() {
    return this._form.gridFragments;
  }
});

const LayoutFront = FrontClassWithSpec(layoutSpec, {});

exports.GridFront = GridFront;
exports.LayoutFront = LayoutFront;
