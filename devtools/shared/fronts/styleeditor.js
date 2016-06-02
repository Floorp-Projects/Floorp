/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { SimpleStringFront } = require("devtools/server/actors/string");
const { Front, FrontClassWithSpec } = require("devtools/shared/protocol");
const { oldStyleSheetSpec } = require("devtools/shared/specs/styleeditor");
const promise = require("promise");
const events = require("sdk/event/core");

/**
 * StyleSheetFront is the client-side counterpart to a StyleSheetActor.
 */
const OldStyleSheetFront = FrontClassWithSpec(oldStyleSheetSpec, {
  initialize: function (conn, form, ctx, detail) {
    Front.prototype.initialize.call(this, conn, form, ctx, detail);

    this._onPropertyChange = this._onPropertyChange.bind(this);
    events.on(this, "property-change", this._onPropertyChange);
  },

  destroy: function () {
    events.off(this, "property-change", this._onPropertyChange);

    Front.prototype.destroy.call(this);
  },

  _onPropertyChange: function (property, value) {
    this._form[property] = value;
  },

  form: function (form, detail) {
    if (detail === "actorid") {
      this.actorID = form;
      return;
    }
    this.actorID = form.actor;
    this._form = form;
  },

  getText: function () {
    let deferred = promise.defer();

    events.once(this, "source-load", (source) => {
      let longStr = new SimpleStringFront(source);
      deferred.resolve(longStr);
    });
    this.fetchSource();

    return deferred.promise;
  },

  getOriginalSources: function () {
    return promise.resolve([]);
  },

  get href() {
    return this._form.href;
  },
  get nodeHref() {
    return this._form.nodeHref;
  },
  get disabled() {
    return !!this._form.disabled;
  },
  get title() {
    return this._form.title;
  },
  get isSystem() {
    return this._form.system;
  },
  get styleSheetIndex() {
    return this._form.styleSheetIndex;
  },
  get ruleCount() {
    return this._form.ruleCount;
  }
});

exports.OldStyleSheetFront = OldStyleSheetFront;
