/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("../types");

class FontEditor extends PureComponent {
  static get propTypes() {
    return {
      fontEditor: PropTypes.shape(Types.fontEditor).isRequired,
    };
  }

  render() {
    const { selector } = this.props.fontEditor;

    return dom.div(
      {
        className: "theme-sidebar inspector-tabpanel",
        id: "sidebar-panel-fonteditor"
      }, `Placeholder for Font Editor panel for selector: ${selector}`
    );
  }
}

module.exports = FontEditor;
