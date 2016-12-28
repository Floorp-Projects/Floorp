/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const SearchBox = require("devtools/client/shared/components/search-box");
const { L10N } = require("../l10n");
const Actions = require("../actions/index");
const { FREETEXT_FILTER_SEARCH_DELAY } = require("../constants");

module.exports = connect(
  (state) => ({
    delay: FREETEXT_FILTER_SEARCH_DELAY,
    keyShortcut: L10N.getStr("netmonitor.toolbar.filterFreetext.key"),
    placeholder: L10N.getStr("netmonitor.toolbar.filterFreetext.label"),
    type: "filter",
  }),
  (dispatch) => ({
    onChange: (url) => {
      dispatch(Actions.setRequestFilterText(url));
    },
  })
)(SearchBox);
