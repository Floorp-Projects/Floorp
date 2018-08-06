/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PAGE_SELECTED,
  PAGES,
} = require("../constants");

const Actions = require("./index");

function selectPage(page) {
  return async (dispatch, getState) => {
    dispatch({ type: PAGE_SELECTED, page });

    if (page === PAGES.THIS_FIREFOX) {
      dispatch(Actions.connectRuntime());
    } else {
      dispatch(Actions.disconnectRuntime());
    }
  };
}

module.exports = {
  selectPage,
};
