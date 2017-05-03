/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, createFactory, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

const { LocalizationHelper } = require("devtools/shared/l10n");

const ComputedProperty = createFactory(require("./ComputedProperty"));

const Types = require("../types");

const BOXMODEL_STRINGS_URI = "devtools/client/locales/boxmodel.properties";
const BOXMODEL_L10N = new LocalizationHelper(BOXMODEL_STRINGS_URI);

module.exports = createClass({

  displayName: "BoxModelProperties",

  propTypes: {
    boxModel: PropTypes.shape(Types.boxModel).isRequired,
    setSelectedNode: PropTypes.func.isRequired,
    onHideBoxModelHighlighter: PropTypes.func.isRequired,
    onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  getInitialState() {
    return {
      isOpen: true,
    };
  },

  /**
   * Various properties can display a reference element. E.g. position displays an offset
   * parent if its value is other than fixed or static. Or z-index displays a stacking
   * context, etc.
   * This returns the right element if there needs to be one, and one was passed in the
   * props.
   *
   * @return {Object} An object with 2 properties:
   * - referenceElement {NodeFront}
   * - referenceElementType {String}
   */
  getReferenceElement(propertyName) {
    let value = this.props.boxModel.layout[propertyName];

    if (propertyName === "position" &&
        value !== "static" && value !== "fixed" &&
        this.props.boxModel.offsetParent) {
      return {
        referenceElement: this.props.boxModel.offsetParent,
        referenceElementType: BOXMODEL_L10N.getStr("boxmodel.offsetParent")
      };
    }

    return {};
  },

  onToggleExpander() {
    this.setState({
      isOpen: !this.state.isOpen,
    });
  },

  render() {
    let {
      boxModel,
      setSelectedNode,
      onHideBoxModelHighlighter,
      onShowBoxModelHighlighterForNode,
    } = this.props;
    let { layout } = boxModel;

    let layoutInfo = ["box-sizing", "display", "float",
                      "line-height", "position", "z-index"];

    const properties = layoutInfo.map(info => {
      let { referenceElement, referenceElementType } = this.getReferenceElement(info);

      return ComputedProperty({
        name: info,
        key: info,
        value: layout[info],
        referenceElement,
        referenceElementType,
        setSelectedNode,
        onHideBoxModelHighlighter,
        onShowBoxModelHighlighterForNode,
      });
    });

    return dom.div(
      {
        className: "boxmodel-properties",
      },
      dom.div(
        {
          className: "boxmodel-properties-header",
          onDoubleClick: this.onToggleExpander,
        },
        dom.span(
          {
            className: "boxmodel-properties-expander theme-twisty",
            open: this.state.isOpen,
            onClick: this.onToggleExpander,
          }
        ),
        BOXMODEL_L10N.getStr("boxmodel.propertiesLabel")
      ),
      dom.div(
        {
          className: "boxmodel-properties-wrapper",
          hidden: !this.state.isOpen,
          tabIndex: 0,
        },
        properties
      )
    );
  },

});
