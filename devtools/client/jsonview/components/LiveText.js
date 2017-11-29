/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const { Component } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
  const { pre } = require("devtools/client/shared/vendor/react-dom-factories");

  /**
   * This object represents a live DOM text node in a <pre>.
   */
  class LiveText extends Component {
    static get propTypes() {
      return {
        data: PropTypes.instanceOf(Text),
      };
    }

    componentDidMount() {
      this.componentDidUpdate();
    }

    componentDidUpdate() {
      let el = findDOMNode(this);
      if (el.firstChild === this.props.data) {
        return;
      }
      el.textContent = "";
      el.append(this.props.data);
    }

    render() {
      return pre({className: "data"});
    }
  }

  // Exports from this module
  exports.LiveText = LiveText;
});
