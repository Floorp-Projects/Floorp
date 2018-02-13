/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const FontList = createFactory(require("./FontList"));

const { getStr } = require("../utils/l10n");
const Types = require("../types");

class FontsApp extends PureComponent {
  static get propTypes() {
    return {
      fonts: PropTypes.arrayOf(PropTypes.shape(Types.font)).isRequired,
      fontOptions: PropTypes.shape(Types.fontOptions).isRequired,
      onPreviewFonts: PropTypes.func.isRequired,
    };
  }

  render() {
    let {
      fonts,
      fontOptions,
      onPreviewFonts,
    } = this.props;

    return dom.div(
      {
        className: "theme-sidebar inspector-tabpanel",
        id: "sidebar-panel-fontinspector"
      },
      fonts.length ?
        FontList({
          fonts,
          fontOptions,
          onPreviewFonts
        })
        :
        dom.div(
          {
            className: "devtools-sidepanel-no-result"
          },
          getStr("fontinspector.noFontsOnSelectedElement")
        )
    );
  }
}

module.exports = connect(state => state)(FontsApp);
