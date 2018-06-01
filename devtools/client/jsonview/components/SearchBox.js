/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {
  const { Component } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");

  const { input } = dom;

  // For smooth incremental searching (in case the user is typing quickly).
  const searchDelay = 250;

  /**
   * This object represents a search box located at the
   * top right corner of the application.
   */
  class SearchBox extends Component {
    static get propTypes() {
      return {
        actions: PropTypes.object,
      };
    }

    constructor(props) {
      super(props);
      this.onSearch = this.onSearch.bind(this);
      this.doSearch = this.doSearch.bind(this);
    }

    onSearch(event) {
      const searchBox = event.target;
      const win = searchBox.ownerDocument.defaultView;

      if (this.searchTimeout) {
        win.clearTimeout(this.searchTimeout);
      }

      const callback = this.doSearch.bind(this, searchBox);
      this.searchTimeout = win.setTimeout(callback, searchDelay);
    }

    doSearch(searchBox) {
      this.props.actions.onSearch(searchBox.value);
    }

    render() {
      return (
        input({className: "searchBox devtools-filterinput",
               placeholder: JSONView.Locale.$STR("jsonViewer.filterJSON"),
               onChange: this.onSearch})
      );
    }
  }

  // Exports from this module
  exports.SearchBox = SearchBox;
});
