/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const React = require("devtools/client/shared/vendor/react");
const { l10n } = require("../utils");

// For smooth incremental searching (in case the user is typing quickly).
const searchDelay = 250;

// Shortcuts
const { input } = React.DOM;
const PropTypes = React.PropTypes;

/**
 * This object represents a search box located at the
 * top right corner of the application.
 */
var SearchBox = React.createClass({
  displayName: "SearchBox",

  propTypes: {
    onSearch: PropTypes.func,
  },

  componentWillUnmount: function () {
    // Clean up an existing timeout.
    if (this.searchTimeout) {
      window.clearTimeout(this.searchTimeout);
    }
  },

  onSearch: function (event) {
    let searchBox = event.target;

    // Clean up an existing timeout before creating a new one.
    if (this.searchTimeout) {
      window.clearTimeout(this.searchTimeout);
    }

    // Execute the search after a timeout. It makes the UX
    // smoother if the user is typing quickly.
    this.searchTimeout = window.setTimeout(() => {
      this.searchTimeout = null;
      this.props.onSearch(searchBox.value);
    }, searchDelay);
  },

  render: function () {
    return (
      input({
        className: "dom-searchbox devtools-filterinput",
        placeholder: l10n.getStr("dom.filterDOMPanel"),
        onChange: this.onSearch
      })
    );
  }
});

// Exports from this module
module.exports = SearchBox;
