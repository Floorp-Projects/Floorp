/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");

  const { input } = dom;

  // For smooth incremental searching (in case the user is typing quickly).
  const searchDelay = 250;

  /**
   * This object represents a search box located at the
   * top right corner of the application.
   */
  let SearchBox = createClass({
    displayName: "SearchBox",

    propTypes: {
      actions: PropTypes.object,
    },

    onSearch: function (event) {
      let searchBox = event.target;
      let win = searchBox.ownerDocument.defaultView;

      if (this.searchTimeout) {
        win.clearTimeout(this.searchTimeout);
      }

      let callback = this.doSearch.bind(this, searchBox);
      this.searchTimeout = win.setTimeout(callback, searchDelay);
    },

    doSearch: function (searchBox) {
      this.props.actions.onSearch(searchBox.value);
    },

    render: function () {
      return (
        input({className: "devtools-filterinput",
               placeholder: Locale.$STR("jsonViewer.filterJSON"),
               onChange: this.onSearch})
      );
    },
  });

  // Exports from this module
  exports.SearchBox = SearchBox;
});
