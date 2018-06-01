/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { LocalizationHelper } = require("devtools/shared/l10n");

const Types = require("../types");

const BOXMODEL_STRINGS_URI = "devtools/client/locales/boxmodel.properties";
const BOXMODEL_L10N = new LocalizationHelper(BOXMODEL_STRINGS_URI);

const SHARED_STRINGS_URI = "devtools/client/locales/shared.properties";
const SHARED_L10N = new LocalizationHelper(SHARED_STRINGS_URI);

class BoxModelInfo extends PureComponent {
  static get propTypes() {
    return {
      boxModel: PropTypes.shape(Types.boxModel).isRequired,
      onToggleGeometryEditor: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onToggleGeometryEditor = this.onToggleGeometryEditor.bind(this);
  }

  onToggleGeometryEditor(e) {
    this.props.onToggleGeometryEditor();
  }

  render() {
    const { boxModel } = this.props;
    const { geometryEditorEnabled, layout } = boxModel;
    const {
      height = "-",
      isPositionEditable,
      position,
      width = "-",
    } = layout;

    let buttonClass = "layout-geometry-editor devtools-button";
    if (geometryEditorEnabled) {
      buttonClass += " checked";
    }

    return dom.div(
      {
        className: "boxmodel-info",
      },
      dom.span(
        {
          className: "boxmodel-element-size",
        },
        SHARED_L10N.getFormatStr("dimensions", width, height)
      ),
      dom.section(
        {
          className: "boxmodel-position-group",
        },
        isPositionEditable ?
          dom.button({
            className: buttonClass,
            title: BOXMODEL_L10N.getStr("boxmodel.geometryButton.tooltip"),
            onClick: this.onToggleGeometryEditor,
          })
          :
          null,
        dom.span(
          {
            className: "boxmodel-element-position",
          },
          position
        )
      )
    );
  }
}

module.exports = BoxModelInfo;
