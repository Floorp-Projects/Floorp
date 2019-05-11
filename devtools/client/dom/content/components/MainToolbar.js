/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const SearchBox = createFactory(require("devtools/client/shared/components/SearchBox"));

const { l10n } = require("../utils");

// Actions
const { fetchProperties } = require("../actions/grips");
const { setVisibilityFilter } = require("../actions/filter");

/**
 * This template is responsible for rendering a toolbar
 * within the 'Headers' panel.
 */
class MainToolbar extends Component {
  static get propTypes() {
    return {
      object: PropTypes.any.isRequired,
      dispatch: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onRefresh = this.onRefresh.bind(this);
    this.onSearch = this.onSearch.bind(this);
  }

  onRefresh() {
    this.props.dispatch(fetchProperties(this.props.object));
  }

  onSearch(value) {
    this.props.dispatch(setVisibilityFilter(value));
  }

  render() {
    return (
      dom.div({ className: "devtools-toolbar devtools-input-toolbar" },
        SearchBox({
          key: "filter",
          delay: 250,
          onChange: this.onSearch,
          placeholder: l10n.getStr("dom.filterDOMPanel"),
          type: "filter",
        }),
        dom.span({ className: "devtools-separator" }),
        dom.button({
          key: "refresh",
          className: "refresh devtools-button",
          id: "dom-refresh-button",
          title: l10n.getStr("dom.refresh"),
          onClick: this.onRefresh,
        }),
      )
    );
  }
}

// Exports from this module
module.exports = MainToolbar;
