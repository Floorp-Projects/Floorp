/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {

const React = require("react");

const DOM = React.DOM;

// For smooth incremental searching (in case the user is typing quickly).
const searchDelay = 250;

/**
 * This object represents a search box located at the
 * top right corner of the application.
 */
var SearchBox = React.createClass({
  displayName: "SearchBox",

  render: function() {
    return (
      DOM.input({className: "searchBox",
        placeholder: Locale.$STR("jsonViewer.filterJSON"),
        onChange: this.onSearch})
    )
  },

  onSearch: function(event) {
    var searchBox = event.target;
    var win = searchBox.ownerDocument.defaultView;

    if (this.searchTimeout) {
      win.clearTimeout(this.searchTimeout);
    }

    var callback = this.doSearch.bind(this, searchBox);
    this.searchTimeout = win.setTimeout(callback, searchDelay);
  },

  doSearch: function(searchBox) {
    this.props.actions.onSearch(searchBox.value);
  }
});

// Exports from this module
exports.SearchBox = SearchBox;
});
