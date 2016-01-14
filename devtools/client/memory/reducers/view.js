/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { actions, viewState } = require("../constants");

const handlers = Object.create(null);

handlers[actions.CHANGE_VIEW] = function (_, { view }) {
  return view;
};

module.exports = function (view = viewState.CENSUS, action) {
  const handler = handlers[action.type];
  return handler ? handler(view, action) : view;
};
