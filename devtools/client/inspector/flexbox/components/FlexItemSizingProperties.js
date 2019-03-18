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
   * Rounds some size in pixels and render it.
   * The rendered value will end with 'px' (unless the dimension is 0 in which case the
   * unit will be omitted)
   *
   * @param  {Number} value
   *         The number to be rounded
   * @param  {Boolean} prependPlusSign
   *         If set to true, the + sign will be printed before a positive value
   * @return {Object}
   *         The React component representing this rounded size
   */
  renderSize(value, prependPlusSign) {
    if (value == 0) {
      return dom.span({ className: "value" }, "0");
    }

    value = (Math.round(value * 100) / 100);
    if (prependPlusSign && value > 0) {
      value = "+" + value;
    }

    return (
      dom.span({ className: "value" },
        value,
        dom.span({ className: "unit" }, "px")
      )
    );
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
    return dom.span({ className: "css-property-link" }, `(${name}: ${value})`);
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

  renderBaseSizeSection({ mainBaseSize, clampState }, properties, dimension) {
    const flexBasisValue = properties["flex-basis"];
    const dimensionValue = properties[dimension];

    let title = getStr("flexbox.itemSizing.baseSizeSectionHeader");
    let property = null;

    if (flexBasisValue) {
      // If flex-basis is defined, then that's what is used for the base size.
      property = this.renderCssProperty("flex-basis", flexBasisValue);
    } else if (dimensionValue) {
      // If not and width/height is defined, then that's what defines the base size.
      property = this.renderCssProperty(dimension, dimensionValue);
    } else {
      // Finally, if nothing is set, then the base size is the max-content size.
      // In this case replace the section's title.
      title = getStr("flexbox.itemSizing.itemContentSize");
    }

    const className = "section base";
    return (
      dom.li({ className: className + (property ? "" : " no-property") },
        dom.span({ className: "name" },
          title,
          property
        ),
        this.renderSize(mainBaseSize)
      )
    );
  }

  renderFlexibilitySection(flexItemSizing, mainFinalSize, properties, computedStyle) {
    const {
      mainDeltaSize,
      mainBaseSize,
      lineGrowthState,
    } = flexItemSizing;

    // Don't display anything if all interesting sizes are 0.
    if (!mainFinalSize && !mainBaseSize && !mainDeltaSize) {
      return null;
    }

    // Also don't display anything if the item did not grow or shrink.
    const grew = mainDeltaSize > 0;
    const shrank = mainDeltaSize < 0;
    if (!grew && !shrank) {
      return null;
    }

    const definedFlexGrow = properties["flex-grow"];
    const computedFlexGrow = computedStyle.flexGrow;
    const definedFlexShrink = properties["flex-shrink"];
    const computedFlexShrink = computedStyle.flexShrink;

    const reasons = [];

    // Tell users whether the item was set to grow or shrink.
    if (computedFlexGrow && lineGrowthState === "growing") {
      reasons.push(getStr("flexbox.itemSizing.setToGrow"));
    }
    if (computedFlexShrink && lineGrowthState === "shrinking") {
      reasons.push(getStr("flexbox.itemSizing.setToShrink"));
    }
    if (!computedFlexGrow && !grew && !shrank && lineGrowthState === "growing") {
      reasons.push(getStr("flexbox.itemSizing.notSetToGrow"));
    }
    if (!computedFlexShrink && !grew && !shrank && lineGrowthState === "shrinking") {
      reasons.push(getStr("flexbox.itemSizing.notSetToShrink"));
    }

    let property = null;

    if (grew && definedFlexGrow && computedFlexGrow) {
      // If the item grew it's normally because it was set to grow (flex-grow is non 0).
      property = this.renderCssProperty("flex-grow", definedFlexGrow);
    } else if (shrank && definedFlexShrink && computedFlexShrink) {
      // If the item shrank it's either because flex-shrink is non 0.
      property = this.renderCssProperty("flex-shrink", definedFlexShrink);
    } else if (shrank && computedFlexShrink) {
      // Or also because it's default value is 1 anyway.
      property = this.renderCssProperty("flex-shrink", computedFlexShrink, true);
    }

    // Don't display the section at all if there's nothing useful to show users.
    if (!property && !reasons.length) {
      return null;
    }

    const className = "section flexibility";
    return (
      dom.li({ className: className + (property ? "" : " no-property") },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.flexibilitySectionHeader"),
          property
        ),
        this.renderSize(mainDeltaSize, true),
        this.renderReasons(reasons)
      )
    );
  }

  renderMinimumSizeSection(flexItemSizing, properties, dimension) {
    const { clampState, mainMinSize, mainDeltaSize } = flexItemSizing;
    const grew = mainDeltaSize > 0;
    const shrank = mainDeltaSize < 0;
    const minDimensionValue = properties[`min-${dimension}`];

    // We only display the minimum size when the item actually violates that size during
    // layout & is clamped.
    if (clampState !== "clamped_to_min") {
      return null;
    }

    const reasons = [];
    if (grew || shrank) {
      // The item may have wanted to grow less, but was min-clamped to a larger size.
      // Or the item may have wanted to shrink more but was min-clamped to a larger size.
      reasons.push(getStr("flexbox.itemSizing.clampedToMin"));
    }

    return (
      dom.li({ className: "section min" },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.minSizeSectionHeader"),
          minDimensionValue.length ?
           this.renderCssProperty(`min-${dimension}`, minDimensionValue) :
           null
        ),
        this.renderSize(mainMinSize),
        this.renderReasons(reasons)
      )
    );
  }

  renderMaximumSizeSection(flexItemSizing, properties, dimension) {
    const { clampState, mainMaxSize, mainDeltaSize } = flexItemSizing;
    const grew = mainDeltaSize > 0;
    const shrank = mainDeltaSize < 0;
    const maxDimensionValue = properties[`max-${dimension}`];

    if (clampState !== "clamped_to_max") {
      return null;
    }

    const reasons = [];
    if (grew || shrank) {
      // The item may have wanted to grow more than it did, because it was max-clamped.
      // Or the item may have wanted shrink more, but it was clamped to its max size.
      reasons.push(getStr("flexbox.itemSizing.clampedToMax"));
    }

    return (
      dom.li({ className: "section max" },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.maxSizeSectionHeader"),
          maxDimensionValue.length ?
            this.renderCssProperty(`max-${dimension}`, maxDimensionValue) :
            null
        ),
        this.renderSize(mainMaxSize),
        this.renderReasons(reasons)
      )
    );
  }

  renderFinalSizeSection(mainFinalSize) {
    return (
      dom.li({ className: "section final no-property" },
        dom.span({ className: "name" },
          getStr("flexbox.itemSizing.finalSizeSectionHeader")
        ),
        this.renderSize(mainFinalSize)
      )
    );
  }

  render() {
    const {
      flexItem,
    } = this.props;
    const {
      computedStyle,
      flexItemSizing,
      properties,
    } = flexItem;
    const {
      mainAxisDirection,
      mainBaseSize,
      mainDeltaSize,
      mainMaxSize,
      mainMinSize,
    } = flexItemSizing;
    const dimension = mainAxisDirection.startsWith("horizontal") ? "width" : "height";

    // Calculate the final size. This is base + delta, then clamped by min or max.
    let mainFinalSize = mainBaseSize + mainDeltaSize;
    mainFinalSize = Math.max(mainFinalSize, mainMinSize);
    mainFinalSize =
      mainMaxSize === null ?
        mainFinalSize :
        Math.min(mainFinalSize, mainMaxSize);

    return (
      dom.ul({ className: "flex-item-sizing" },
        this.renderBaseSizeSection(flexItemSizing, properties, dimension),
        this.renderFlexibilitySection(flexItemSizing, mainFinalSize, properties,
          computedStyle),
        this.renderMinimumSizeSection(flexItemSizing, properties, dimension),
        this.renderMaximumSizeSection(flexItemSizing, properties, dimension),
        this.renderFinalSizeSection(mainFinalSize)
      )
    );
  }
}

module.exports = FlexItemSizingProperties;
