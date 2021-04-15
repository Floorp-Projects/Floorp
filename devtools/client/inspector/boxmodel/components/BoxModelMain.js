/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { KeyCodes } = require("devtools/client/shared/keycodes");
const { LocalizationHelper } = require("devtools/shared/l10n");

const BoxModelEditable = createFactory(
  require("devtools/client/inspector/boxmodel/components/BoxModelEditable")
);

const Types = require("devtools/client/inspector/boxmodel/types");

const {
  highlightSelectedNode,
  unhighlightNode,
} = require("devtools/client/inspector/boxmodel/actions/box-model-highlighter");

const SHARED_STRINGS_URI = "devtools/client/locales/shared.properties";
const SHARED_L10N = new LocalizationHelper(SHARED_STRINGS_URI);

class BoxModelMain extends PureComponent {
  static get propTypes() {
    return {
      boxModel: PropTypes.shape(Types.boxModel).isRequired,
      boxModelContainer: PropTypes.object,
      dispatch: PropTypes.func.isRequired,
      onShowBoxModelEditor: PropTypes.func.isRequired,
      onShowRulePreviewTooltip: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      activeDescendant: null,
      focusable: false,
    };

    this.getActiveDescendant = this.getActiveDescendant.bind(this);
    this.getBorderOrPaddingValue = this.getBorderOrPaddingValue.bind(this);
    this.getContextBox = this.getContextBox.bind(this);
    this.getDisplayPosition = this.getDisplayPosition.bind(this);
    this.getHeightValue = this.getHeightValue.bind(this);
    this.getMarginValue = this.getMarginValue.bind(this);
    this.getPositionValue = this.getPositionValue.bind(this);
    this.getWidthValue = this.getWidthValue.bind(this);
    this.moveFocus = this.moveFocus.bind(this);
    this.onHighlightMouseOver = this.onHighlightMouseOver.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.onLevelClick = this.onLevelClick.bind(this);
    this.setActive = this.setActive.bind(this);
  }

  componentDidUpdate() {
    const displayPosition = this.getDisplayPosition();
    const isContentBox = this.getContextBox();

    this.layouts = {
      position: new Map([
        [KeyCodes.DOM_VK_ESCAPE, this.positionLayout],
        [KeyCodes.DOM_VK_DOWN, this.marginLayout],
        [KeyCodes.DOM_VK_RETURN, this.positionEditable],
        [KeyCodes.DOM_VK_UP, null],
        ["click", this.positionLayout],
      ]),
      margin: new Map([
        [KeyCodes.DOM_VK_ESCAPE, this.marginLayout],
        [KeyCodes.DOM_VK_DOWN, this.borderLayout],
        [KeyCodes.DOM_VK_RETURN, this.marginEditable],
        [KeyCodes.DOM_VK_UP, displayPosition ? this.positionLayout : null],
        ["click", this.marginLayout],
      ]),
      border: new Map([
        [KeyCodes.DOM_VK_ESCAPE, this.borderLayout],
        [KeyCodes.DOM_VK_DOWN, this.paddingLayout],
        [KeyCodes.DOM_VK_RETURN, this.borderEditable],
        [KeyCodes.DOM_VK_UP, this.marginLayout],
        ["click", this.borderLayout],
      ]),
      padding: new Map([
        [KeyCodes.DOM_VK_ESCAPE, this.paddingLayout],
        [KeyCodes.DOM_VK_DOWN, isContentBox ? this.contentLayout : null],
        [KeyCodes.DOM_VK_RETURN, this.paddingEditable],
        [KeyCodes.DOM_VK_UP, this.borderLayout],
        ["click", this.paddingLayout],
      ]),
      content: new Map([
        [KeyCodes.DOM_VK_ESCAPE, this.contentLayout],
        [KeyCodes.DOM_VK_DOWN, null],
        [KeyCodes.DOM_VK_RETURN, this.contentEditable],
        [KeyCodes.DOM_VK_UP, this.paddingLayout],
        ["click", this.contentLayout],
      ]),
    };
  }

  getActiveDescendant() {
    let { activeDescendant } = this.state;

    if (!activeDescendant) {
      const displayPosition = this.getDisplayPosition();
      const nextLayout = displayPosition
        ? this.positionLayout
        : this.marginLayout;
      activeDescendant = nextLayout.getAttribute("data-box");
      this.setActive(nextLayout);
    }

    return activeDescendant;
  }

  getBorderOrPaddingValue(property) {
    const { layout } = this.props.boxModel;
    return layout[property] ? parseFloat(layout[property]) : "-";
  }

  /**
   * Returns true if the layout box sizing is context box and false otherwise.
   */
  getContextBox() {
    const { layout } = this.props.boxModel;
    return layout["box-sizing"] == "content-box";
  }

  /**
   * Returns true if the position is displayed and false otherwise.
   */
  getDisplayPosition() {
    const { layout } = this.props.boxModel;
    return layout.position && layout.position != "static";
  }

  getHeightValue(property) {
    if (property == undefined) {
      return "-";
    }

    const { layout } = this.props.boxModel;

    property -=
      parseFloat(layout["border-top-width"]) +
      parseFloat(layout["border-bottom-width"]) +
      parseFloat(layout["padding-top"]) +
      parseFloat(layout["padding-bottom"]);
    property = parseFloat(property.toPrecision(6));

    return property;
  }

  getMarginValue(property, direction) {
    const { layout } = this.props.boxModel;
    const autoMargins = layout.autoMargins || {};
    let value = "-";

    if (direction in autoMargins) {
      value = autoMargins[direction];
    } else if (layout[property]) {
      const parsedValue = parseFloat(layout[property]);

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
  }

  getPositionValue(property) {
    const { layout } = this.props.boxModel;
    let value = "-";

    if (!layout[property]) {
      return value;
    }

    const parsedValue = parseFloat(layout[property]);

    if (Number.isNaN(parsedValue)) {
      // Not a number. We use the raw string.
      value = layout[property];
    } else {
      value = parsedValue;
    }

    return value;
  }

  getWidthValue(property) {
    if (property == undefined) {
      return "-";
    }

    const { layout } = this.props.boxModel;

    property -=
      parseFloat(layout["border-left-width"]) +
      parseFloat(layout["border-right-width"]) +
      parseFloat(layout["padding-left"]) +
      parseFloat(layout["padding-right"]);
    property = parseFloat(property.toPrecision(6));

    return property;
  }

  /**
   * Move the focus to the next/previous editable element of the current layout.
   *
   * @param  {Element} target
   *         Node to be observed
   * @param  {Boolean} shiftKey
   *         Determines if shiftKey was pressed
   */
  moveFocus({ target, shiftKey }) {
    const editBoxes = [
      ...this.positionLayout.querySelectorAll("[data-box].boxmodel-editable"),
    ];
    const editingMode = target.tagName === "input";
    // target.nextSibling is input field
    let position = editingMode
      ? editBoxes.indexOf(target.nextSibling)
      : editBoxes.indexOf(target);

    if (position === editBoxes.length - 1 && !shiftKey) {
      position = 0;
    } else if (position === 0 && shiftKey) {
      position = editBoxes.length - 1;
    } else {
      shiftKey ? position-- : position++;
    }

    const editBox = editBoxes[position];
    this.setActive(editBox);
    editBox.focus();

    if (editingMode) {
      editBox.click();
    }
  }

  /**
   * Active level set to current layout.
   *
   * @param  {Element} nextLayout
   *         Element of next layout that user has navigated to
   */
  setActive(nextLayout) {
    const { boxModelContainer } = this.props;

    // We set this attribute for testing purposes.
    if (boxModelContainer) {
      boxModelContainer.dataset.activeDescendantClassName =
        nextLayout.className;
    }

    this.setState({
      activeDescendant: nextLayout.getAttribute("data-box"),
    });
  }

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

      this.props.dispatch(unhighlightNode());
    }

    this.props.dispatch(
      highlightSelectedNode({
        region,
        showOnly: region,
        onlyRegionArea: true,
      })
    );

    event.preventDefault();
  }

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
    const { target, keyCode } = event;
    const isEditable = target._editable || target.editor;

    const level = this.getActiveDescendant();
    const editingMode = target.tagName === "input";

    switch (keyCode) {
      case KeyCodes.DOM_VK_RETURN:
        if (!isEditable) {
          this.setState({ focusable: true }, () => {
            const editableBox = this.layouts[level].get(keyCode);
            if (editableBox) {
              editableBox.boxModelEditable.focus();
            }
          });
        }
        break;
      case KeyCodes.DOM_VK_DOWN:
      case KeyCodes.DOM_VK_UP:
        if (!editingMode) {
          event.preventDefault();
          event.stopPropagation();
          this.setState({ focusable: false }, () => {
            const nextLayout = this.layouts[level].get(keyCode);

            if (!nextLayout) {
              return;
            }

            this.setActive(nextLayout);

            if (target?._editable) {
              target.blur();
            }

            this.props.boxModelContainer.focus();
          });
        }
        break;
      case KeyCodes.DOM_VK_TAB:
        if (isEditable) {
          event.preventDefault();
          this.moveFocus(event);
        }
        break;
      case KeyCodes.DOM_VK_ESCAPE:
        if (target._editable) {
          event.preventDefault();
          event.stopPropagation();
          this.setState({ focusable: false }, () => {
            this.props.boxModelContainer.focus();
          });
        }
        break;
      default:
        break;
    }
  }

  /**
   * Update active on mouse click.
   *
   * @param  {Event} event
   *         The event triggered by a mouse click on the box model
   */
  onLevelClick(event) {
    const { target } = event;
    const displayPosition = this.getDisplayPosition();
    const isContentBox = this.getContextBox();

    // Avoid switching the active descendant to the position or content layout
    // if those are not editable.
    if (
      (!displayPosition && target == this.positionLayout) ||
      (!isContentBox && target == this.contentLayout)
    ) {
      return;
    }

    const nextLayout = this.layouts[target.getAttribute("data-box")].get(
      "click"
    );
    this.setActive(nextLayout);

    if (target?._editable) {
      target.blur();
    }
  }

  render() {
    const {
      boxModel,
      dispatch,
      onShowBoxModelEditor,
      onShowRulePreviewTooltip,
    } = this.props;
    const { layout } = boxModel;
    let { height, width } = layout;
    const { activeDescendant: level, focusable } = this.state;

    const borderTop = this.getBorderOrPaddingValue("border-top-width");
    const borderRight = this.getBorderOrPaddingValue("border-right-width");
    const borderBottom = this.getBorderOrPaddingValue("border-bottom-width");
    const borderLeft = this.getBorderOrPaddingValue("border-left-width");

    const paddingTop = this.getBorderOrPaddingValue("padding-top");
    const paddingRight = this.getBorderOrPaddingValue("padding-right");
    const paddingBottom = this.getBorderOrPaddingValue("padding-bottom");
    const paddingLeft = this.getBorderOrPaddingValue("padding-left");

    const displayPosition = this.getDisplayPosition();
    const positionTop = this.getPositionValue("top");
    const positionRight = this.getPositionValue("right");
    const positionBottom = this.getPositionValue("bottom");
    const positionLeft = this.getPositionValue("left");

    const marginTop = this.getMarginValue("margin-top", "top");
    const marginRight = this.getMarginValue("margin-right", "right");
    const marginBottom = this.getMarginValue("margin-bottom", "bottom");
    const marginLeft = this.getMarginValue("margin-left", "left");

    height = this.getHeightValue(height);
    width = this.getWidthValue(width);

    const contentBox =
      layout["box-sizing"] == "content-box"
        ? dom.div(
            { className: "boxmodel-size" },
            BoxModelEditable({
              box: "content",
              focusable,
              level,
              property: "width",
              ref: editable => {
                this.contentEditable = editable;
              },
              textContent: width,
              onShowBoxModelEditor,
              onShowRulePreviewTooltip,
            }),
            dom.span({}, "\u00D7"),
            BoxModelEditable({
              box: "content",
              focusable,
              level,
              property: "height",
              textContent: height,
              onShowBoxModelEditor,
              onShowRulePreviewTooltip,
            })
          )
        : dom.p(
            {
              className: "boxmodel-size",
              id: "boxmodel-size-id",
            },
            dom.span(
              { title: "content" },
              SHARED_L10N.getFormatStr("dimensions", width, height)
            )
          );

    return dom.div(
      {
        className: "boxmodel-main devtools-monospace",
        "data-box": "position",
        ref: div => {
          this.positionLayout = div;
        },
        onClick: this.onLevelClick,
        onKeyDown: this.onKeyDown,
        onMouseOver: this.onHighlightMouseOver,
        onMouseOut: () => dispatch(unhighlightNode()),
      },
      displayPosition
        ? dom.span(
            {
              className: "boxmodel-legend",
              "data-box": "position",
              title: "position",
            },
            "position"
          )
        : null,
      dom.div(
        { className: "boxmodel-box" },
        dom.span(
          {
            className: "boxmodel-legend",
            "data-box": "margin",
            title: "margin",
            role: "region",
            "aria-level": "1", // margin, outermost box
            "aria-owns":
              "margin-top-id margin-right-id margin-bottom-id margin-left-id margins-div",
          },
          "margin"
        ),
        dom.div(
          {
            className: "boxmodel-margins",
            id: "margins-div",
            "data-box": "margin",
            title: "margin",
            ref: div => {
              this.marginLayout = div;
            },
          },
          dom.span(
            {
              className: "boxmodel-legend",
              "data-box": "border",
              title: "border",
              role: "region",
              "aria-level": "2", // margin -> border, second box
              "aria-owns":
                "border-top-width-id border-right-width-id border-bottom-width-id border-left-width-id borders-div",
            },
            "border"
          ),
          dom.div(
            {
              className: "boxmodel-borders",
              id: "borders-div",
              "data-box": "border",
              title: "border",
              ref: div => {
                this.borderLayout = div;
              },
            },
            dom.span(
              {
                className: "boxmodel-legend",
                "data-box": "padding",
                title: "padding",
                role: "region",
                "aria-level": "3", // margin -> border -> padding
                "aria-owns":
                  "padding-top-id padding-right-id padding-bottom-id padding-left-id padding-div",
              },
              "padding"
            ),
            dom.div(
              {
                className: "boxmodel-paddings",
                id: "padding-div",
                "data-box": "padding",
                title: "padding",
                "aria-owns": "boxmodel-contents-id",
                ref: div => {
                  this.paddingLayout = div;
                },
              },
              dom.div({
                className: "boxmodel-contents",
                id: "boxmodel-contents-id",
                "data-box": "content",
                title: "content",
                role: "region",
                "aria-level": "4", // margin -> border -> padding -> content
                "aria-label": SHARED_L10N.getFormatStr(
                  "boxModelSize.accessibleLabel",
                  width,
                  height
                ),
                "aria-owns": "boxmodel-size-id",
                ref: div => {
                  this.contentLayout = div;
                },
              })
            )
          )
        )
      ),
      displayPosition
        ? BoxModelEditable({
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
            onShowRulePreviewTooltip,
          })
        : null,
      displayPosition
        ? BoxModelEditable({
            box: "position",
            direction: "right",
            focusable,
            level,
            property: "position-right",
            textContent: positionRight,
            onShowBoxModelEditor,
            onShowRulePreviewTooltip,
          })
        : null,
      displayPosition
        ? BoxModelEditable({
            box: "position",
            direction: "bottom",
            focusable,
            level,
            property: "position-bottom",
            textContent: positionBottom,
            onShowBoxModelEditor,
            onShowRulePreviewTooltip,
          })
        : null,
      displayPosition
        ? BoxModelEditable({
            box: "position",
            direction: "left",
            focusable,
            level,
            property: "position-left",
            textContent: positionLeft,
            onShowBoxModelEditor,
            onShowRulePreviewTooltip,
          })
        : null,
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
        onShowRulePreviewTooltip,
      }),
      BoxModelEditable({
        box: "margin",
        direction: "right",
        focusable,
        level,
        property: "margin-right",
        textContent: marginRight,
        onShowBoxModelEditor,
        onShowRulePreviewTooltip,
      }),
      BoxModelEditable({
        box: "margin",
        direction: "bottom",
        focusable,
        level,
        property: "margin-bottom",
        textContent: marginBottom,
        onShowBoxModelEditor,
        onShowRulePreviewTooltip,
      }),
      BoxModelEditable({
        box: "margin",
        direction: "left",
        focusable,
        level,
        property: "margin-left",
        textContent: marginLeft,
        onShowBoxModelEditor,
        onShowRulePreviewTooltip,
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
        onShowRulePreviewTooltip,
      }),
      BoxModelEditable({
        box: "border",
        direction: "right",
        focusable,
        level,
        property: "border-right-width",
        textContent: borderRight,
        onShowBoxModelEditor,
        onShowRulePreviewTooltip,
      }),
      BoxModelEditable({
        box: "border",
        direction: "bottom",
        focusable,
        level,
        property: "border-bottom-width",
        textContent: borderBottom,
        onShowBoxModelEditor,
        onShowRulePreviewTooltip,
      }),
      BoxModelEditable({
        box: "border",
        direction: "left",
        focusable,
        level,
        property: "border-left-width",
        textContent: borderLeft,
        onShowBoxModelEditor,
        onShowRulePreviewTooltip,
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
        onShowRulePreviewTooltip,
      }),
      BoxModelEditable({
        box: "padding",
        direction: "right",
        focusable,
        level,
        property: "padding-right",
        textContent: paddingRight,
        onShowBoxModelEditor,
        onShowRulePreviewTooltip,
      }),
      BoxModelEditable({
        box: "padding",
        direction: "bottom",
        focusable,
        level,
        property: "padding-bottom",
        textContent: paddingBottom,
        onShowBoxModelEditor,
        onShowRulePreviewTooltip,
      }),
      BoxModelEditable({
        box: "padding",
        direction: "left",
        focusable,
        level,
        property: "padding-left",
        textContent: paddingLeft,
        onShowBoxModelEditor,
        onShowRulePreviewTooltip,
      }),
      contentBox
    );
  }
}

module.exports = BoxModelMain;
