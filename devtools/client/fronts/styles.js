/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cu } = require("chrome");
const {
  Front,
  FrontClassWithSpec,
  custom
} = require("devtools/shared/protocol.js");
const { pageStyleSpec } = require("devtools/shared/specs/styles.js");
const { Task } = require("resource://gre/modules/Task.jsm");

/**
 * PageStyleFront, the front object for the PageStyleActor
 */
const PageStyleFront = FrontClassWithSpec(pageStyleSpec, {
  initialize: function (conn, form, ctx, detail) {
    Front.prototype.initialize.call(this, conn, form, ctx, detail);
    this.inspector = this.parent();
  },

  form: function (form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this._form = form;
  },

  destroy: function () {
    Front.prototype.destroy.call(this);
  },

  get walker() {
    return this.inspector.walker;
  },

  get supportsAuthoredStyles() {
    return this._form.traits && this._form.traits.authoredStyles;
  },

  getMatchedSelectors: custom(function (node, property, options) {
    return this._getMatchedSelectors(node, property, options).then(ret => {
      return ret.matched;
    });
  }, {
    impl: "_getMatchedSelectors"
  }),

  getApplied: custom(Task.async(function* (node, options = {}) {
    // If the getApplied method doesn't recreate the style cache itself, this
    // means a call to cssLogic.highlight is required before trying to access
    // the applied rules. Issue a request to getLayout if this is the case.
    // See https://bugzilla.mozilla.org/show_bug.cgi?id=1103993#c16.
    if (!this._form.traits || !this._form.traits.getAppliedCreatesStyleCache) {
      yield this.getLayout(node);
    }
    let ret = yield this._getApplied(node, options);
    return ret.entries;
  }), {
    impl: "_getApplied"
  }),

  addNewRule: custom(function (node, pseudoClasses) {
    let addPromise;
    if (this.supportsAuthoredStyles) {
      addPromise = this._addNewRule(node, pseudoClasses, true);
    } else {
      addPromise = this._addNewRule(node, pseudoClasses);
    }
    return addPromise.then(ret => {
      return ret.entries[0];
    });
  }, {
    impl: "_addNewRule"
  })
});

exports.PageStyleFront = PageStyleFront;
