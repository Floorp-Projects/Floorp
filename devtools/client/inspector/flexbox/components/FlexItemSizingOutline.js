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

  renderPoint(name) {
    return dom.div({ className: `flex-outline-point ${name}`, "data-label": name });
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

    // Sort all of the dimensions in order to come up with a grid track template.
    // Make mainDeltaSize start from the same point as the other ones so we can compare.
    let sizes = [
      { name: "basis-end", size: mainBaseSize },
      { name: "final-end", size: mainFinalSize },
    ];

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

    sizes = sizes.sort((a, b) => a.size - b.size);

    let gridTemplateColumns = "[final-start basis-start";
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
          this.renderPoint("basis"),
          this.renderPoint("final"),
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
