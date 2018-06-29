/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const FontEditor = createFactory(require("./FontEditor"));
const FontOverview = createFactory(require("./FontOverview"));

const Types = require("../types");

class FontsApp extends PureComponent {
  static get propTypes() {
    return {
      fontData: PropTypes.shape(Types.fontData).isRequired,
      fontEditor: PropTypes.shape(Types.fontEditor).isRequired,
      fontEditorEnabled: PropTypes.bool.isRequired,
      fontOptions: PropTypes.shape(Types.fontOptions).isRequired,
      onInstanceChange: PropTypes.func.isRequired,
      onPreviewFonts: PropTypes.func.isRequired,
      onPropertyChange: PropTypes.func.isRequired,
      onToggleFontHighlight: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      fontData,
      fontEditor,
      fontEditorEnabled,
      fontOptions,
      onInstanceChange,
      onPreviewFonts,
      onPropertyChange,
      onToggleFontHighlight,
    } = this.props;

    return dom.div(
      {
        className: "theme-sidebar inspector-tabpanel",
        id: "sidebar-panel-fontinspector"
      },
      fontEditorEnabled && FontEditor({
        fontEditor,
        onInstanceChange,
        onPropertyChange,
        onToggleFontHighlight,
      }),
      FontOverview({
        fontData,
        fontOptions,
        onPreviewFonts,
        onToggleFontHighlight,
      })
    );
  }
}

module.exports = connect(state => state)(FontsApp);
