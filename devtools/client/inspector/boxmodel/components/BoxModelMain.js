/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, createFactory, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");
const { findDOMNode } = require("devtools/client/shared/vendor/react-dom");
const { KeyCodes } = require("devtools/client/shared/keycodes");

const { LocalizationHelper } = require("devtools/shared/l10n");

const BoxModelEditable = createFactory(require("./BoxModelEditable"));

// Reps
const { REPS, MODE } = require("devtools/client/shared/components/reps/reps");
const { Rep } = REPS;

const Types = require("../types");

const BOXMODEL_STRINGS_URI = "devtools/client/locales/boxmodel.properties";
const BOXMODEL_L10N = new LocalizationHelper(BOXMODEL_STRINGS_URI);

const SHARED_STRINGS_URI = "devtools/client/locales/shared.properties";
const SHARED_L10N = new LocalizationHelper(SHARED_STRINGS_URI);

module.exports = createClass({

  displayName: "BoxModelMain",

  propTypes: {
    boxModel: PropTypes.shape(Types.boxModel).isRequired,
    boxModelContainer: PropTypes.object,
    setSelectedNode: PropTypes.func.isRequired,
    onHideBoxModelHighlighter: PropTypes.func.isRequired,
    onShowBoxModelEditor: PropTypes.func.isRequired,
    onShowBoxModelHighlighter: PropTypes.func.isRequired,
    onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  getInitialState() {
    return {
      activeDescendant: null,
      focusable: false,
    };
  },

  componentDidUpdate() {
    let displayPosition = this.getDisplayPosition();
    let isContentBox = this.getContextBox();

    this.layouts = {
      "position": new Map([
        [KeyCodes.DOM_VK_ESCAPE, this.positionLayout],
        [KeyCodes.DOM_VK_DOWN, this.marginLayout],
        [KeyCodes.DOM_VK_RETURN, this.positionEditable],
        [KeyCodes.DOM_VK_UP, null],
        ["click", this.positionLayout]
      ]),
      "margin": new Map([
        [KeyCodes.DOM_VK_ESCAPE, this.marginLayout],
        [KeyCodes.DOM_VK_DOWN, this.borderLayout],
        [KeyCodes.DOM_VK_RETURN, this.marginEditable],
        [KeyCodes.DOM_VK_UP, displayPosition ? this.positionLayout : null],
        ["click", this.marginLayout]
      ]),
      "border": new Map([
        [KeyCodes.DOM_VK_ESCAPE, this.borderLayout],
        [KeyCodes.DOM_VK_DOWN, this.paddingLayout],
        [KeyCodes.DOM_VK_RETURN, this.borderEditable],
        [KeyCodes.DOM_VK_UP, this.marginLayout],
        ["click", this.borderLayout]
      ]),
      "padding": new Map([
        [KeyCodes.DOM_VK_ESCAPE, this.paddingLayout],
        [KeyCodes.DOM_VK_DOWN, isContentBox ? this.contentLayout : null],
        [KeyCodes.DOM_VK_RETURN, this.paddingEditable],
        [KeyCodes.DOM_VK_UP, this.borderLayout],
        ["click", this.paddingLayout]
      ]),
      "content": new Map([
        [KeyCodes.DOM_VK_ESCAPE, this.contentLayout],
        [KeyCodes.DOM_VK_DOWN, null],
        [KeyCodes.DOM_VK_RETURN, this.contentEditable],
        [KeyCodes.DOM_VK_UP, this.paddingLayout],
        ["click", this.contentLayout]
      ])
    };
  },

  getAriaActiveDescendant() {
    let { activeDescendant } = this.state;

    if (!activeDescendant) {
      let displayPosition = this.getDisplayPosition();
      let nextLayout = displayPosition ? this.positionLayout : this.marginLayout;
      activeDescendant = nextLayout.getAttribute("data-box");
      this.setAriaActive(nextLayout);
    }

    return activeDescendant;
  },

  getBorderOrPaddingValue(property) {
    let { layout } = this.props.boxModel;
    return layout[property] ? parseFloat(layout[property]) : "-";
  },

  /**
   * Returns true if the layout box sizing is context box and false otherwise.
   */
  getContextBox() {
    let { layout } = this.props.boxModel;
    return layout["box-sizing"] == "content-box";
  },

  /**
   * Returns true if the position is displayed and false otherwise.
   */
  getDisplayPosition() {
    let { layout } = this.props.boxModel;
    return layout.position && layout.position != "static";
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
      value = autoMargins[direction];
    } else if (layout[property]) {
      let parsedValue = parseFloat(layout[property]);

      if (Number.isNaN(parsedValue)) {
        // Not a number. We use the raw string.
        // Useful for pseudo-elements with auto margins since they
        // don't appear in autoMargins.
        value = layout[property];
      } else {
        value = parsedValue;
      }
    }

    return value;
  },

  getPositionValue(property) {
    let { layout } = this.props.boxModel;
    let value = "-";

    if (!layout[property]) {
      return value;
    }

    let parsedValue = parseFloat(layout[property]);

    if (Number.isNaN(parsedValue)) {
      // Not a number. We use the raw string.
      value = layout[property];
    } else {
      value = parsedValue;
    }

    return value;
  },

  /**
   * Move the focus to the next/previous editable element of the current layout.
   *
   * @param  {Element} target
   *         Node to be observed
   * @param  {Boolean} shiftKey
   *         Determines if shiftKey was pressed
   * @param  {String} level
   *         Current active layout
   */
  moveFocus: function ({ target, shiftKey }, level) {
    let editBoxes = [
      ...findDOMNode(this).querySelectorAll(`[data-box="${level}"].boxmodel-editable`)
    ];
    let editingMode = target.tagName === "input";
    // target.nextSibling is input field
    let position = editingMode ? editBoxes.indexOf(target.nextSibling)
                               : editBoxes.indexOf(target);

    if (position === editBoxes.length - 1 && !shiftKey) {
      position = 0;
    } else if (position === 0 && shiftKey) {
      position = editBoxes.length - 1;
    } else {
      shiftKey ? position-- : position++;
    }

    let editBox = editBoxes[position];
    editBox.focus();

    if (editingMode) {
      editBox.click();
    }
  },

  /**
   * Active aria-level set to current layout.
   *
   * @param  {Element} nextLayout
   *         Element of next layout that user has navigated to
   */
  setAriaActive(nextLayout) {
    let { boxModelContainer } = this.props;

    // We set this attribute for testing purposes.
    if (boxModelContainer) {
      boxModelContainer.setAttribute("activedescendant", nextLayout.className);
    }

    this.setState({
      activeDescendant: nextLayout.getAttribute("data-box"),
    });
  },

  /**
   * While waiting for a reps fix in https://github.com/devtools-html/reps/issues/92,
   * translate nodeFront to a grip-like object that can be used with an ElementNode rep.
   *
   * @param  {NodeFront} nodeFront
   *         The NodeFront for which we want to create a grip-like object.
   * @return {Object} a grip-like object that can be used with Reps.
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

  /**
   * Handle keyboard navigation and focus for box model layouts.
   *
   * Updates active layout on arrow key navigation
   * Focuses next layout's editboxes on enter key
   * Unfocuses current layout's editboxes when active layout changes
   * Controls tabbing between editBoxes
   *
   * @param  {Event} event
   *         The event triggered by a keypress on the box model
   */
  onKeyDown(event) {
    let { target, keyCode } = event;
    let isEditable = target._editable || target.editor;

    let level = this.getAriaActiveDescendant();
    let editingMode = target.tagName === "input";

    switch (keyCode) {
      case KeyCodes.DOM_VK_RETURN:
        if (!isEditable) {
          this.setState({ focusable: true });
          let editableBox = this.layouts[level].get(keyCode);
          if (editableBox) {
            editableBox.boxModelEditable.focus();
          }
        }
        break;
      case KeyCodes.DOM_VK_DOWN:
      case KeyCodes.DOM_VK_UP:
        if (!editingMode) {
          event.preventDefault();
          this.setState({ focusable: false });

          let nextLayout = this.layouts[level].get(keyCode);
          this.setAriaActive(nextLayout);

          if (target && target._editable) {
            target.blur();
          }

          this.props.boxModelContainer.focus();
        }
        break;
      case KeyCodes.DOM_VK_TAB:
        if (isEditable) {
          event.preventDefault();
          this.moveFocus(event, level);
        }
        break;
      case KeyCodes.DOM_VK_ESCAPE:
        if (target._editable) {
          event.preventDefault();
          event.stopPropagation();
          this.setState({ focusable: false });
          this.props.boxModelContainer.focus();
        }
        break;
      default:
        break;
    }
  },

  /**
   * Update aria-active on mouse click.
   *
   * @param  {Event} event
   *         The event triggered by a mouse click on the box model
   */
  onLevelClick(event) {
    let { target } = event;
    let displayPosition = this.getDisplayPosition();
    let isContentBox = this.getContextBox();

    // Avoid switching the aria active descendant to the position or content layout
    // if those are not editable.
    if ((!displayPosition && target == this.positionLayout) ||
        (!isContentBox && target == this.contentLayout)) {
      return;
    }

    let nextLayout = this.layouts[target.getAttribute("data-box")].get("click");
    this.setAriaActive(nextLayout);

    if (target && target._editable) {
      target.blur();
    }
  },

  render() {
    let {
      boxModel,
      setSelectedNode,
      onShowBoxModelEditor,
    } = this.props;
    let { layout, offsetParent } = boxModel;
    let { height, width, position } = layout;
    let { activeDescendant: level, focusable } = this.state;

    let displayOffsetParent = offsetParent && layout.position === "absolute";

    let borderTop = this.getBorderOrPaddingValue("border-top-width");
    let borderRight = this.getBorderOrPaddingValue("border-right-width");
    let borderBottom = this.getBorderOrPaddingValue("border-bottom-width");
    let borderLeft = this.getBorderOrPaddingValue("border-left-width");

    let paddingTop = this.getBorderOrPaddingValue("padding-top");
    let paddingRight = this.getBorderOrPaddingValue("padding-right");
    let paddingBottom = this.getBorderOrPaddingValue("padding-bottom");
    let paddingLeft = this.getBorderOrPaddingValue("padding-left");

    let displayPosition = this.getDisplayPosition();
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
      dom.div(
        {
          className: "boxmodel-size",
        },
        BoxModelEditable({
          box: "content",
          focusable,
          level,
          property: "width",
          ref: editable => {
            this.contentEditable = editable;
          },
          textContent: width,
          onShowBoxModelEditor
        }),
        dom.span(
          {},
          "\u00D7"
        ),
        BoxModelEditable({
          box: "content",
          focusable,
          level,
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
        "data-box": "position",
        ref: div => {
          this.positionLayout = div;
        },
        onClick: this.onLevelClick,
        onKeyDown: this.onKeyDown,
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
            ref: div => {
              this.marginLayout = div;
            },
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
              ref: div => {
                this.borderLayout = div;
              },
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
                ref: div => {
                  this.paddingLayout = div;
                },
              },
              dom.div({
                className: "boxmodel-contents",
                "data-box": "content",
                title: BOXMODEL_L10N.getStr("boxmodel.content"),
                ref: div => {
                  this.contentLayout = div;
                },
              })
            )
          )
        )
      ),
      displayPosition ?
        BoxModelEditable({
          box: "position",
          direction: "top",
          focusable,
          level,
          property: "position-top",
          ref: editable => {
            this.positionEditable = editable;
          },
          textContent: positionTop,
          onShowBoxModelEditor,
        })
        :
        null,
      displayPosition ?
        BoxModelEditable({
          box: "position",
          direction: "right",
          focusable,
          level,
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
          focusable,
          level,
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
          focusable,
          level,
          property: "position-left",
          textContent: positionLeft,
          onShowBoxModelEditor,
        })
        :
        null,
      BoxModelEditable({
        box: "margin",
        direction: "top",
        focusable,
        level,
        property: "margin-top",
        ref: editable => {
          this.marginEditable = editable;
        },
        textContent: marginTop,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "margin",
        direction: "right",
        focusable,
        level,
        property: "margin-right",
        textContent: marginRight,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "margin",
        direction: "bottom",
        focusable,
        level,
        property: "margin-bottom",
        textContent: marginBottom,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "margin",
        direction: "left",
        focusable,
        level,
        property: "margin-left",
        textContent: marginLeft,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "border",
        direction: "top",
        focusable,
        level,
        property: "border-top-width",
        ref: editable => {
          this.borderEditable = editable;
        },
        textContent: borderTop,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "border",
        direction: "right",
        focusable,
        level,
        property: "border-right-width",
        textContent: borderRight,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "border",
        direction: "bottom",
        focusable,
        level,
        property: "border-bottom-width",
        textContent: borderBottom,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "border",
        direction: "left",
        focusable,
        level,
        property: "border-left-width",
        textContent: borderLeft,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "padding",
        direction: "top",
        focusable,
        level,
        property: "padding-top",
        ref: editable => {
          this.paddingEditable = editable;
        },
        textContent: paddingTop,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "padding",
        direction: "right",
        focusable,
        level,
        property: "padding-right",
        textContent: paddingRight,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "padding",
        direction: "bottom",
        focusable,
        level,
        property: "padding-bottom",
        textContent: paddingBottom,
        onShowBoxModelEditor,
      }),
      BoxModelEditable({
        box: "padding",
        direction: "left",
        focusable,
        level,
        property: "padding-left",
        textContent: paddingLeft,
        onShowBoxModelEditor,
      }),
      contentBox
    );
  },

});
