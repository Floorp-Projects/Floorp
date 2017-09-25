/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM, createClass, PropTypes } = require("devtools/client/shared/vendor/react");

// Shortcuts
const { div } = DOM;

/**
 * Helper panel component that is using an existing DOM node
 * as the content. It's used by Sidebar as well as SplitBox
 * components.
 */
var InspectorTabPanel = createClass({
  displayName: "InspectorTabPanel",

  propTypes: {
    // ID of the node that should be rendered as the content.
    id: PropTypes.string.isRequired,
    // Optional prefix for panel IDs.
    idPrefix: PropTypes.string,
    // Optional mount callback
    onMount: PropTypes.func,
  },

  getDefaultProps: function () {
    return {
      idPrefix: "",
    };
  },

  componentDidMount: function () {
    let doc = this.refs.content.ownerDocument;
    let panel = doc.getElementById(this.props.idPrefix + this.props.id);

    // Append existing DOM node into panel's content.
    this.refs.content.appendChild(panel);

    if (this.props.onMount) {
      this.props.onMount(this.refs.content, this.props);
    }
  },

  componentWillUnmount: function () {
    let doc = this.refs.content.ownerDocument;
    let panels = doc.getElementById("tabpanels");

    // Move panel's content node back into list of tab panels.
    panels.appendChild(this.refs.content.firstChild);
  },

  render: function () {
    return (
      div({
        ref: "content",
        className: "devtools-inspector-tab-panel",
      })
    );
  }
});

module.exports = InspectorTabPanel;
