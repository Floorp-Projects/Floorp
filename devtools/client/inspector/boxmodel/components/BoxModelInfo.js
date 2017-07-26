/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { LocalizationHelper } = require("devtools/shared/l10n");
const { addons, createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");

const BOXMODEL_STRINGS_URI = "devtools/client/locales/boxmodel.properties";
const BOXMODEL_L10N = new LocalizationHelper(BOXMODEL_STRINGS_URI);

const SHARED_STRINGS_URI = "devtools/client/locales/shared.properties";
const SHARED_L10N = new LocalizationHelper(SHARED_STRINGS_URI);

module.exports = createClass({

  displayName: "BoxModelInfo",

  propTypes: {
    boxModel: PropTypes.shape(Types.boxModel).isRequired,
    onToggleGeometryEditor: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  onToggleGeometryEditor(e) {
    this.props.onToggleGeometryEditor();
  },

  render() {
    let { boxModel } = this.props;
    let { geometryEditorEnabled, layout } = boxModel;
    let {
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
  },

});
