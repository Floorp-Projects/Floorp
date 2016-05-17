/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const React = require("devtools/client/shared/vendor/react");
const { l10n } = require("../utils");

// Reps
const { createFactories } = require("devtools/client/shared/components/reps/rep-utils");
const { Toolbar, ToolbarButton } = createFactories(require("devtools/client/jsonview/components/reps/toolbar"));

// DOM Panel
const SearchBox = React.createFactory(require("../components/search-box"));

// Actions
const { fetchProperties } = require("../actions/grips");
const { setVisibilityFilter } = require("../actions/filter");

// Shortcuts
const PropTypes = React.PropTypes;

/**
 * This template is responsible for rendering a toolbar
 * within the 'Headers' panel.
 */
var MainToolbar = React.createClass({
  displayName: "MainToolbar",

  propTypes: {
    object: PropTypes.any.isRequired,
    dispatch: PropTypes.func.isRequired,
  },

  onRefresh: function () {
    this.props.dispatch(fetchProperties(this.props.object));
  },

  onSearch: function (value) {
    this.props.dispatch(setVisibilityFilter(value));
  },

  render: function () {
    return (
      Toolbar({},
        ToolbarButton({
          className: "btn refresh",
          onClick: this.onRefresh},
          l10n.getStr("dom.refresh")
        ),
        SearchBox({
          onSearch: this.onSearch
        })
      )
    );
  }
});

// Exports from this module
module.exports = MainToolbar;
