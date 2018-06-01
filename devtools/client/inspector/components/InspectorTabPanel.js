/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

// Shortcuts
const { div } = dom;

/**
 * Helper panel component that is using an existing DOM node
 * as the content. It's used by Sidebar as well as SplitBox
 * components.
 */
class InspectorTabPanel extends Component {
  static get propTypes() {
    return {
      // ID of the node that should be rendered as the content.
      id: PropTypes.string.isRequired,
      // Optional prefix for panel IDs.
      idPrefix: PropTypes.string,
      // Optional mount callback.
      onMount: PropTypes.func,
      // Optional unmount callback.
      onUnmount: PropTypes.func,
    };
  }

  static get defaultProps() {
    return {
      idPrefix: "",
    };
  }

  componentDidMount() {
    const doc = this.refs.content.ownerDocument;
    const panel = doc.getElementById(this.props.idPrefix + this.props.id);

    // Append existing DOM node into panel's content.
    this.refs.content.appendChild(panel);

    if (this.props.onMount) {
      this.props.onMount(this.refs.content, this.props);
    }
  }

  componentWillUnmount() {
    const doc = this.refs.content.ownerDocument;
    const panels = doc.getElementById("tabpanels");

    if (this.props.onUnmount) {
      this.props.onUnmount(this.refs.content, this.props);
    }

    // Move panel's content node back into list of tab panels.
    panels.appendChild(this.refs.content.firstChild);
  }

  render() {
    return (
      div({
        ref: "content",
        className: "devtools-inspector-tab-panel",
      })
    );
  }
}

module.exports = InspectorTabPanel;
