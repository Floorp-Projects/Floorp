/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, createFactory, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

const { LocalizationHelper } = require("devtools/shared/l10n");

const BoxModelEditable = createFactory(require("./BoxModelEditable"));
// Reps
const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const Rep = createFactory(REPS.Rep);

const Types = require("../types");

const BOXMODEL_STRINGS_URI = "devtools/client/locales/boxmodel.properties";
const BOXMODEL_L10N = new LocalizationHelper(BOXMODEL_STRINGS_URI);

const SHARED_STRINGS_URI = "devtools/client/locales/shared.properties";
const SHARED_L10N = new LocalizationHelper(SHARED_STRINGS_URI);

module.exports = createClass({

  displayName: "BoxModelMain",

  propTypes: {
    boxModel: PropTypes.shape(Types.boxModel).isRequired,
    setSelectedNode: PropTypes.func.isRequired,
    onHideBoxModelHighlighter: PropTypes.func.isRequired,
    onShowBoxModelEditor: PropTypes.func.isRequired,
    onShowBoxModelHighlighter: PropTypes.func.isRequired,
    onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  getBorderOrPaddingValue(property) {
    let { layout } = this.props.boxModel;
    return layout[property] ? parseFloat(layout[property]) : "-";
  },

  getHeightValue(property) {
    let { layout } = this.props.boxModel;

    if (property == undefined) {
      return "-";
    }

    property -= parseFloat(layout["border-top-width"]) +
                parseFloat(layout["border-bottom-width"]) +
                parseFloat(layout["padding-top"]) +
                parseFloat(layout["padding-bottom"]);
    property = parseFloat(property.toPrecision(6));

    return property;
  },

  getWidthValue(property) {
    let { layout } = this.props.boxModel;

    if (property == undefined) {
      return "-";
    }

    property -= parseFloat(layout["border-left-width"]) +
                parseFloat(layout["border-right-width"]) +
                parseFloat(layout["padding-left"]) +
                parseFloat(layout["padding-right"]);
    property = parseFloat(property.toPrecision(6));

    return property;
  },

  getMarginValue(property, direction) {
    let { layout } = this.props.boxModel;
    let autoMargins = layout.autoMargins || {};
    let value = "-";

    if (direction in autoMargins) {
      value = "auto";
    } else if (layout[property]) {
      value = parseFloat(layout[property]);
    }

    return value;
  },

  getPositionValue(property) {
    let { layout } = this.props.boxModel;

    if (layout.position === "static") {
      return "-";
    }
    return layout[property] ? parseFloat(layout[property]) : "-";
  },

  /**
   * While waiting for a reps fix in https://github.com/devtools-html/reps/issues/92,
   * translate nodeFront to a grip-like object that can be used with an ElementNode rep.
   *
   * @params  {NodeFront} nodeFront
   *          The NodeFront for which we want to create a grip-like object.
   * @returns {Object} a grip-like object that can be used with Reps.
   */
  translateNodeFrontToGrip(nodeFront) {
    let {
      attributes
    } = nodeFront;

    // The main difference between NodeFront and grips is that attributes are treated as
    // a map in grips and as an array in NodeFronts.
    let attributesMap = {};
    for (let { name, value } of attributes) {
      attributesMap[name] = value;
    }

    return {
      actor: nodeFront.actorID,
      preview: {
        attributes: attributesMap,
        attributesLength: attributes.length,
        // nodeName is already lowerCased in Node grips
        nodeName: nodeFront.nodeName.toLowerCase(),
        nodeType: nodeFront.nodeType,
      }
    };
  },

  onHighlightMouseOver(event) {
    let region = event.target.getAttribute("data-box");

    if (!region) {
      let el = event.target;

      do {
        el = el.parentNode;

        if (el && el.getAttribute("data-box")) {
          region = el.getAttribute("data-box");
          break;
        }
      } while (el.parentNode);

      this.props.onHideBoxModelHighlighter();
    }

    if (region === "offset-parent") {
      this.props.onHideBoxModelHighlighter();
      this.props.onShowBoxModelHighlighterForNode(this.props.boxModel.offsetParent);
      return;
    }

    this.props.onShowBoxModelHighlighter({
      region,
      showOnly: region,
      onlyRegionArea: true,
    });
  },

  render() {
    let {
      boxModel,
      setSelectedNode,
      onShowBoxModelEditor,
    } = this.props;
    let { layout, offsetParent } = boxModel;
    let { height, width, position } = layout;

    let displayOffsetParent = offsetParent && layout.position === "absolute";

    let borderTop = this.getBorderOrPaddingValue("border-top-width");
    let borderRight = this.getBorderOrPaddingValue("border-right-width");
    let borderBottom = this.getBorderOrPaddingValue("border-bottom-width");
    let borderLeft = this.getBorderOrPaddingValue("border-left-width");

    let paddingTop = this.getBorderOrPaddingValue("padding-top");
    let paddingRight = this.getBorderOrPaddingValue("padding-right");
    let paddingBottom = this.getBorderOrPaddingValue("padding-bottom");
    let paddingLeft = this.getBorderOrPaddingValue("padding-left");

    let displayPosition = layout.position && layout.position != "static";
    let positionTop = this.getPositionValue("top");
    let positionRight = this.getPositionValue("right");
    let positionBottom = this.getPositionValue("bottom");
    let positionLeft = this.getPositionValue("left");

    let marginTop = this.getMarginValue("margin-top", "top");
    let marginRight = this.getMarginValue("margin-right", "right");
    let marginBottom = this.getMarginValue("margin-bottom", "bottom");
    let marginLeft = this.getMarginValue("margin-left", "left");

    height = this.getHeightValue(height);
    width = this.getWidthValue(width);

    let contentBox = layout["box-sizing"] == "content-box" ?
      dom.p(
        {
          className: "boxmodel-size",
        },
        BoxModelEditable({
          box: "content",
          property: "width",
          textContent: width,
          onShowBoxModelEditor
        }),
        dom.span(
          {},
          "\u00D7"
        ),
        BoxModelEditable({
          box: "content",
          property: "height",
          textContent: height,
          onShowBoxModelEditor
        })
      )
      :
      dom.p(
        {
          className: "boxmodel-size",
        },
        dom.span(
          {
            title: BOXMODEL_L10N.getStr("boxmodel.content"),
          },
          SHARED_L10N.getFormatStr("dimensions", width, height)
        )
      );

    return dom.div(
      {
        className: "boxmodel-main",
        onMouseOver: this.onHighlightMouseOver,
        onMouseOut: this.props.onHideBoxModelHighlighter,
      },
      displayOffsetParent ?
        dom.span(
          {
            className: "boxmodel-offset-parent",
            "data-box": "offset-parent",
          },
          Rep(
            {
              defaultRep: offsetParent,
              mode: MODE.TINY,
              object: this.translateNodeFrontToGrip(offsetParent),
              onInspectIconClick: () => setSelectedNode(offsetParent, "box-model"),
            }
          )
        )
        :
        null,
      displayPosition ?
        dom.span(
          {
            className: "boxmodel-legend",
            "data-box": "position",
            title: BOXMODEL_L10N.getFormatStr("boxmodel.position", position),
          },
          BOXMODEL_L10N.getFormatStr("boxmodel.position", position)
        )
        :
        null,
      dom.div(
        {
          className: "boxmodel-box"
        },
        dom.span(
          {
            className: "boxmodel-legend",
            "data-box": "margin",
            title: BOXMODEL_L10N.getStr("boxmodel.margin"),
          },
          BOXMODEL_L10N.getStr("boxmodel.margin")
        ),
        dom.div(
          {
            className: "boxmodel-margins",
            "data-box": "margin",
            title: BOXMODEL_L10N.getStr("boxmodel.margin"),
          },
          dom.span(
            {
              className: "boxmodel-legend",
              "data-box": "border",
              title: BOXMODEL_L10N.getStr("boxmodel.border"),
            },
            BOXMODEL_L10N.getStr("boxmodel.border")
          ),
          dom.div(
            {
              className: "boxmodel-borders",
              "data-box": "border",
              title: BOXMODEL_L10N.getStr("boxmodel.border"),
            },
            dom.span(
              {
                className: "boxmodel-legend",
                "data-box": "padding",
                title: BOXMODEL_L10N.getStr("boxmodel.padding"),
              },
              BOXMODEL_L10N.getStr("boxmodel.padding")
            ),
            dom.div(
              {
                className: "boxmodel-paddings",
                "data-box": "padding",
                title: BOXMODEL_L10N.getStr("boxmodel.padding"),
              },
              dom.div({
                className: "boxmodel-contents",
                "data-box": "content",
                title: BOXMODEL_L10N.getStr("boxmodel.content"),
              })
            )
          )
        )
      ),
      displayPosition ?
        BoxModelEditable({
          box: "position",
          direction: "top",
          property: "position-top",
          textContent: positionTop,
          onShowBoxModelEditor,
        })
        :
        null,
      displayPosition ?
        BoxModelEditable({
          box: "position",
          direction: "right",
          property: "position-right",
          textContent: positionRight,
          onShowBoxModelEditor,
        })
        :
        null,
      displayPosition ?
        BoxModelEditable({
          box: "position",
          direction: "bottom",
          property: "position-bottom",
          textContent: positionBottom,
          onShowBoxModelEditor,
        })
        :
        null,
      displayPosition ?
        BoxModelEditable({
          box: "position",
          direction: "left",
          property: "position-left",
          textContent: positionLeft,
          onShowBoxModelEditor,
        })
        :
        null,
      BoxModelEditable({
        box: "margin",
        direction: "top",
        property: "margin-top",
        textContent: marginTop,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "margin",
        direction: "right",
        property: "margin-right",
        textContent: marginRight,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "margin",
        direction: "bottom",
        property: "margin-bottom",
        textContent: marginBottom,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "margin",
        direction: "left",
        property: "margin-left",
        textContent: marginLeft,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "border",
        direction: "top",
        property: "border-top-width",
        textContent: borderTop,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "border",
        direction: "right",
        property: "border-right-width",
        textContent: borderRight,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "border",
        direction: "bottom",
        property: "border-bottom-width",
        textContent: borderBottom,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "border",
        direction: "left",
        property: "border-left-width",
        textContent: borderLeft,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "padding",
        direction: "top",
        property: "padding-top",
        textContent: paddingTop,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "padding",
        direction: "right",
        property: "padding-right",
        textContent: paddingRight,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "padding",
        direction: "bottom",
        property: "padding-bottom",
        textContent: paddingBottom,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "padding",
        direction: "left",
        property: "padding-left",
        textContent: paddingLeft,
        onShowBoxModelEditor,
      }),
      contentBox
    );
  },

});
