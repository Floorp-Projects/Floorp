/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

const FontEditor = createFactory(
  require("resource://devtools/client/inspector/fonts/components/FontEditor.js")
);
const FontOverview = createFactory(
  require("resource://devtools/client/inspector/fonts/components/FontOverview.js")
);

const Types = require("resource://devtools/client/inspector/fonts/types.js");

class FontsApp extends PureComponent {
  static get propTypes() {
    return {
      fontData: PropTypes.shape(Types.fontData).isRequired,
      fontEditor: PropTypes.shape(Types.fontEditor).isRequired,
      fontOptions: PropTypes.shape(Types.fontOptions).isRequired,
      onInstanceChange: PropTypes.func.isRequired,
      onPreviewTextChange: PropTypes.func.isRequired,
      onPropertyChange: PropTypes.func.isRequired,
      onToggleFontHighlight: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      fontData,
      fontEditor,
      fontOptions,
      onInstanceChange,
      onPreviewTextChange,
      onPropertyChange,
      onToggleFontHighlight,
    } = this.props;

    return dom.div(
      {
        className: "theme-sidebar inspector-tabpanel",
        id: "sidebar-panel-fontinspector",
      },
      FontEditor({
        fontEditor,
        onInstanceChange,
        onPropertyChange,
        onToggleFontHighlight,
      }),
      FontOverview({
        fontData,
        fontOptions,
        onPreviewTextChange,
        onToggleFontHighlight,
      })
    );
  }
}

module.exports = connect(state => state)(FontsApp);
