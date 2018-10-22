/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getStr } = require("devtools/client/inspector/layout/utils/l10n");

const Types = require("../types");

class FlexItemSizingProperties extends PureComponent {
  static get propTypes() {
    return {
      flexDirection: PropTypes.string.isRequired,
      flexItem: PropTypes.shape(Types.flexItem).isRequired,
    };
  }

  /**
   * Rounds some dimension in pixels and returns a string to be displayed to the user.
   * The string will end with 'px'. If the number is 0, the string "0" is returned.
   *
   * @param  {Number} value
   *         The number to be rounded
   * @return {String}
   *         Representation of the rounded number
   */
  getRoundedDimension(value) {
    if (value == 0) {
      return "0";
    }
    return (Math.round(value * 100) / 100) + "px";
  }

  /**
   * Format the flexibility value into a meaningful value for the UI.
   * If the item grew, then prepend a + sign, if it shrank, prepend a - sign.
   * If it didn't flex, return "0".
   *
   * @param  {Boolean} grew
   *         Whether the item grew or not
   * @param  {Number} value
   *         The amount of pixels the item flexed
   * @return {String}
   *         Representation of the flexibility value
   */
  getFlexibilityValueString(grew, mainDeltaSize) {
    const value = this.getRoundedDimension(mainDeltaSize);

    if (grew) {
      return "+" + value;
    }

    return value;
  }

  /**
   * Render an authored CSS property.
   *
   * @param  {String} name
   *         The name for this CSS property
   * @param  {String} value
   *         The property value
   * @param  {Booleam} isDefaultValue
   *         Whether the value come from the browser default style
   * @return {Object}
   *         The React component representing this CSS property
   */
  renderCssProperty(name, value, isDefaultValue) {
    return (
      dom.span({ className: "css-property-link" },
        dom.span({ className: "theme-fg-color5" }, name),
        ": ",
        dom.span({ className: "theme-fg-color1" }, value),
        ";"
      )
    );
  }

  /**
   * Render a list of sentences to be displayed in the UI as reasons why a certain sizing
   * value happened.
   *
   * @param  {Array} sentences
   *         The list of sentences as Strings
   * @return {Object}
   *         The React component representing these sentences
   */
  renderReasons(sentences) {
    return (
      dom.ul({ className: "reasons" },
        sentences.map(sentence => dom.li({}, sentence))
      )
    );
  }

  renderBaseSizeSection({ mainBaseSize, mainMinSize }, properties, dimension) {
    const flexBasisValue = properties["flex-basis"];
    const dimensionValue = properties[dimension];
    const minDimensionValue = properties[`min-${dimension}`];
    const hasMinClamping = mainMinSize && mainMinSize === mainBaseSize;

    let property = null;
    let reason = null;

    if (hasMinClamping && minDimensionValue) {
      // If min clamping happened, then the base size is going to be that value.
      // TODO: this isn't going to be necessarily true after bug 1498273 is fixed.
      property = this.renderCssProperty(`min-${dimension}`, minDimensionValue);
    } else if (flexBasisValue && !hasMinClamping) {
      // If flex-basis is defined, then that's what is used for the base size.
      property = this.renderCssProperty("flex-basis", flexBasisValue);
    } else if (dimensionValue) {
      // If not and width/height is defined, then that's what defines the base size.
      property = this.renderCssProperty(dimension, dimensionValue);
    } else {
      // Finally, if nothing is set, then the base size is the max-content size.
      reason = this.renderReasons(
        [getStr("flexbox.itemSizing.itemBaseSizeFromContent")]);
    }

    return (
      dom.li({ className: property ? "section" : "section no-property" },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.baseSizeSectionHeader")
        ),
        dom.span({ className: "value theme-fg-color1" },
          this.getRoundedDimension(mainBaseSize)
        ),
        property,
        reason
      )
    );
  }

  renderFlexibilitySection(flexItemSizing, properties) {
    const {
      mainDeltaSize,
      mainBaseSize,
      mainFinalSize,
      lineGrowthState
    } = flexItemSizing;

    // Don't attempt to display anything useful if everything is 0.
    if (!mainFinalSize && !mainBaseSize && !mainDeltaSize) {
      return null;
    }

    const flexGrow = properties["flex-grow"];
    const flexGrow0 = parseFloat(flexGrow) === 0;
    const flexShrink = properties["flex-shrink"];
    const flexShrink0 = parseFloat(flexShrink) === 0;
    const grew = mainDeltaSize > 0;
    const shrank = mainDeltaSize < 0;
    // TODO: replace this with the new clamping state that the API will return once bug
    // 1498273 is fixed.
    const wasClamped = mainDeltaSize + mainBaseSize !== mainFinalSize;

    const reasons = [];

    // First output a sentence for telling users about whether there was enough room or
    // not on the line.
    if (lineGrowthState === "growing") {
      reasons.push(getStr("flexbox.itemSizing.extraRoomOnLine"));
    } else if (lineGrowthState === "shrinking") {
      reasons.push(getStr("flexbox.itemSizing.notEnoughRoomOnLine"));
    }

    // Then tell users whether the item was set to grow, shrink or none of them.
    if (flexGrow && !flexGrow0 && lineGrowthState !== "shrinking") {
      reasons.push(getStr("flexbox.itemSizing.setToGrow"));
    }
    if (flexShrink && !flexShrink0 && lineGrowthState !== "growing") {
      reasons.push(getStr("flexbox.itemSizing.setToShrink"));
    }
    if (!grew && !shrank && lineGrowthState === "growing") {
      reasons.push(getStr("flexbox.itemSizing.notSetToGrow"));
    }
    if (!grew && !shrank && lineGrowthState === "shrinking") {
      reasons.push(getStr("flexbox.itemSizing.notSetToShrink"));
    }

    let property = null;

    if (grew) {
      // If the item grew.
      if (flexGrow) {
        // It's normally because it was set to grow (flex-grow is non 0).
        property = this.renderCssProperty("flex-grow", flexGrow);
      }

      if (wasClamped) {
        // It may have wanted to grow more than it did, because it was later max-clamped.
        reasons.push(getStr("flexbox.itemSizing.growthAttemptWhenClamped"));
      }
    } else if (shrank) {
      // If the item shrank.
      if (flexShrink && !flexShrink0) {
        // It's either because flex-shrink is non 0.
        property = this.renderCssProperty("flex-shrink", flexShrink);
      } else {
        // Or also because it's default value is 1 anyway.
        property = this.renderCssProperty("flex-shrink", "1", true);
      }

      if (wasClamped) {
        // It might have wanted to shrink more (to accomodate all items) but couldn't
        // because it was later min-clamped.
        reasons.push(getStr("flexbox.itemSizing.shrinkAttemptWhenClamped"));
      }
    } else if (lineGrowthState === "growing" && flexGrow && !flexGrow0) {
      // The item did not grow or shrink. There was room on the line and flex-grow was
      // set, other items have likely used up all of the space.
      property = this.renderCssProperty("flex-grow", flexGrow);
      reasons.push(getStr("flexbox.itemSizing.growthAttemptButSiblings"));
    } else if (lineGrowthState === "shrinking") {
      // The item did not grow or shrink and there wasn't enough room on the line.
      if (!flexShrink0) {
        // flex-shrink was set (either defined in CSS, or via its default value of 1).
        // but the item didn't shrink.
        if (flexShrink) {
          property = this.renderCssProperty("flex-shrink", flexShrink);
        } else {
          property = this.renderCssProperty("flex-shrink", 1, true);
        }

        reasons.push(getStr("flexbox.itemSizing.shrinkAttemptButCouldnt"));

        if (wasClamped) {
          // Maybe it was clamped.
          reasons.push(getStr("flexbox.itemSizing.shrinkAttemptWhenClamped"));
        }
      } else {
        // flex-shrink was set to 0, so it didn't shrink.
        property = this.renderCssProperty("flex-shrink", flexShrink);
      }
    }

    // Don't display the section at all if there's nothing useful to show users.
    if (!property && !reasons.length) {
      return null;
    }

    return (
      dom.li({ className: property ? "section" : "section no-property" },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.flexibilitySectionHeader")
        ),
        dom.span({ className: "value theme-fg-color1" },
          this.getFlexibilityValueString(grew, mainDeltaSize)
        ),
        property,
        this.renderReasons(reasons)
      )
    );
  }

  renderMinimumSizeSection({ mainMinSize, mainFinalSize }, properties, dimension) {
    // We only display the minimum size when the item actually violates that size during
    // layout & is clamped.
    // For now, we detect this by checking that the min-size is the same as the final size
    // and that a min-size is actually defined in CSS.
    // TODO: replace this with the new clamping state that the API will return once bug
    // 1498273 is fixed.
    const minDimensionValue = properties[`min-${dimension}`];
    if (mainMinSize !== mainFinalSize || !minDimensionValue) {
      return null;
    }

    return (
      dom.li({ className: "section" },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.minSizeSectionHeader")
        ),
        dom.span({ className: "value theme-fg-color1" },
          this.getRoundedDimension(mainMinSize)
        ),
        this.renderCssProperty(`min-${dimension}`, minDimensionValue)
      )
    );
  }

  renderMaximumSizeSection({ mainMaxSize, mainFinalSize }, properties, dimension) {
    // TODO: replace this with the new clamping state that the API will return once bug
    // 1498273 is fixed.
    if (mainMaxSize !== mainFinalSize) {
      return null;
    }

    const maxDimensionValue = properties[`max-${dimension}`];

    return (
      dom.li({ className: "section" },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.maxSizeSectionHeader")
        ),
        dom.span({ className: "value theme-fg-color1" },
          this.getRoundedDimension(mainMaxSize)
        ),
        this.renderCssProperty(`max-${dimension}`, maxDimensionValue)
      )
    );
  }

  renderFinalSizeSection({ mainFinalSize }) {
    return (
      dom.li({ className: "section no-property" },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.finalSizeSectionHeader")
        ),
        dom.span({ className: "value theme-fg-color1" },
          this.getRoundedDimension(mainFinalSize)
        )
      )
    );
  }

  render() {
    const {
      flexDirection,
      flexItem,
    } = this.props;
    const {
      flexItemSizing,
      properties,
    } = flexItem;
    const {
      mainBaseSize,
      mainDeltaSize,
      mainMaxSize,
      mainMinSize,
    } = flexItemSizing;
    const dimension = flexDirection.startsWith("row") ? "width" : "height";

    // Calculate the final size. This is base + delta, then clamped by min or max.
    let mainFinalSize = mainBaseSize + mainDeltaSize;
    mainFinalSize = Math.max(mainFinalSize, mainMinSize);
    mainFinalSize = Math.min(mainFinalSize, mainMaxSize);
    flexItemSizing.mainFinalSize = mainFinalSize;

    return (
      dom.ul({ className: "flex-item-sizing" },
        this.renderBaseSizeSection(flexItemSizing, properties, dimension),
        this.renderFlexibilitySection(flexItemSizing, properties),
        this.renderMinimumSizeSection(flexItemSizing, properties, dimension),
        this.renderMaximumSizeSection(flexItemSizing, properties, dimension),
        this.renderFinalSizeSection(flexItemSizing)
      )
    );
  }
}

module.exports = FlexItemSizingProperties;
