/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { colorUtils } = require("devtools/shared/css/color");

const Types = require("../types");

class FlexItemSizingOutline extends PureComponent {
  static get propTypes() {
    return {
      color: PropTypes.string.isRequired,
      flexDirection: PropTypes.string.isRequired,
      flexItem: PropTypes.shape(Types.flexItem).isRequired,
    };
  }

  renderBasisOutline(mainBaseSize) {
    return (
      dom.div({
        className: "flex-outline-basis" + (!mainBaseSize ? " zero-basis" : ""),
        style: {
          color: colorUtils.setAlpha(this.props.color, 0.4),
        },
      })
    );
  }

  renderDeltaOutline(mainDeltaSize) {
    if (!mainDeltaSize) {
      return null;
    }

    return (
      dom.div({
        className: "flex-outline-delta",
        style: {
          backgroundColor: colorUtils.setAlpha(this.props.color, 0.1),
        },
      })
    );
  }

  renderFinalOutline(mainFinalSize, mainMaxSize, mainMinSize, isClamped) {
    return (
      dom.div({
        className: "flex-outline-final" + (isClamped ? " clamped" : ""),
      })
    );
  }

  renderPoint(className, label = className) {
    return dom.div({ className: `flex-outline-point ${className}`, "data-label": label });
  }

  render() {
    const {
      flexItemSizing,
    } = this.props.flexItem;
    const {
      mainBaseSize,
      mainDeltaSize,
      mainMaxSize,
      mainMinSize,
      clampState,
    } = flexItemSizing;

    const isRow = this.props.flexDirection.startsWith("row");

    // Calculate the final size. This is base + delta, then clamped by min or max.
    let mainFinalSize = mainBaseSize + mainDeltaSize;
    mainFinalSize = Math.max(mainFinalSize, mainMinSize);
    mainFinalSize = Math.min(mainFinalSize, mainMaxSize);

    // Just don't display anything if there isn't anything useful.
    if (!mainFinalSize && !mainBaseSize && !mainDeltaSize) {
      return null;
    }

    // The max size is only interesting to show if it did clamp the item.
    const showMax = clampState === "clamped_to_max";

    // The min size is only really interesting if it actually clamped the item.
    // TODO: We might also want to check if the min-size property is defined.
    const showMin = clampState === "clamped_to_min";

    // Create an array of all of the sizes we want to display that we can use to create
    // a grid track template.
    let sizes = [
      { name: "basis-start", size: 0 },
      { name: "basis-end", size: mainBaseSize },
      { name: "final-start", size: 0 },
      { name: "final-end", size: mainFinalSize },
    ];

    // Because mainDeltaSize is just a delta from base, make sure to make it absolute like
    // the other sizes in the array, so we can later sort all sizes in the same way.
    if (mainDeltaSize > 0) {
      sizes.push({ name: "delta-start", size: mainBaseSize });
      sizes.push({ name: "delta-end", size: mainBaseSize + mainDeltaSize });
    } else {
      sizes.push({ name: "delta-start", size: mainBaseSize + mainDeltaSize });
      sizes.push({ name: "delta-end", size: mainBaseSize });
    }

    if (showMax) {
      sizes.push({ name: "max", size: mainMaxSize });
    }
    if (showMin) {
      sizes.push({ name: "min", size: mainMinSize });
    }

    // Sort all of the dimensions so we can create the grid track template correctly.
    sizes = sizes.sort((a, b) => a.size - b.size);

    // In some cases, the delta-start may be negative (when an item wanted to shrink more
    // than the item's base size). As a negative value would break the grid track template
    // offset all values so they're all positive.
    const offsetBy = sizes.reduce((acc, curr) => curr.size < acc ? curr.size : acc, 0);
    sizes = sizes.map(entry => ({ size: entry.size - offsetBy, name: entry.name }));

    let gridTemplateColumns = "[";
    let accumulatedSize = 0;
    for (const { name, size } of sizes) {
      const breadth = Math.round(size - accumulatedSize);
      if (breadth === 0) {
        gridTemplateColumns += ` ${name}`;
        continue;
      }

      gridTemplateColumns += `] ${breadth}fr [${name}`;
      accumulatedSize = size;
    }
    gridTemplateColumns += "]";

    // Check the final and basis points to see if they are the same and if so, combine
    // them into a single rendered point.
    const renderedBaseAndFinalPoints = [];
    if (mainFinalSize === mainBaseSize) {
      renderedBaseAndFinalPoints.push(this.renderPoint("basisfinal", "basis/final"));
    } else {
      renderedBaseAndFinalPoints.push(this.renderPoint("basis"));
      renderedBaseAndFinalPoints.push(this.renderPoint("final"));
    }

    return (
      dom.div({ className: "flex-outline-container" },
        dom.div(
          {
            className: "flex-outline" +
                       (isRow ? " row" : " column") +
                       (mainDeltaSize > 0 ? " growing" : " shrinking"),
            style: {
              color: this.props.color,
              gridTemplateColumns,
            },
          },
          renderedBaseAndFinalPoints,
          showMin ? this.renderPoint("min") : null,
          showMax ? this.renderPoint("max") : null,
          this.renderBasisOutline(mainBaseSize),
          this.renderDeltaOutline(mainDeltaSize),
          this.renderFinalOutline(mainFinalSize, mainMaxSize, mainMinSize,
                                  clampState !== "unclamped")
        )
      )
    );
  }
}

module.exports = FlexItemSizingOutline;
