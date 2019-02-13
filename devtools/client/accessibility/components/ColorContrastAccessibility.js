/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { div, span, h3 } = require("devtools/client/shared/vendor/react-dom-factories");
const LearnMoreLink = createFactory(require("./LearnMoreLink"));

const { A11Y_CONTRAST_LEARN_MORE_LINK } = require("../constants");
const { L10N } = require("../utils/l10n");

/**
 * Component that renders a colour contrast value along with a swatch preview of what the
 * text and background colours are.
 */
class ContrastValueClass extends Component {
  static get propTypes() {
    return {
      backgroundColor: PropTypes.array.isRequired,
      color: PropTypes.array.isRequired,
      isLargeText: PropTypes.bool.isRequired,
      value: PropTypes.number.isRequired,
    };
  }

  render() {
    const {
      backgroundColor,
      color,
      isLargeText,
      value,
    } = this.props;

    const className = [
      "accessibility-contrast-value",
      getContrastRatioScore(value, isLargeText),
    ].join(" ");

    return (
      span({
        className,
        role: "presentation",
        style: {
          "--accessibility-contrast-color": `rgba(${color})`,
          "--accessibility-contrast-bg": `rgba(${backgroundColor})`,
        },
      }, value.toFixed(2))
    );
  }
}

const ContrastValue = createFactory(ContrastValueClass);

/**
 * Component that renders labeled colour contrast values together with the large text
 * indiscator.
 */
class ColorContrastAccessibilityClass extends Component {
  static get propTypes() {
    return {
      error: PropTypes.string,
      isLargeText: PropTypes.bool.isRequired,
      color: PropTypes.array.isRequired,
      value: PropTypes.number,
      min: PropTypes.number,
      max: PropTypes.number,
      backgroundColor: PropTypes.array,
      backgroundColorMin: PropTypes.array,
      backgroundColorMax: PropTypes.array,
    };
  }

  render() {
    const {
      error,
      isLargeText,
      color,
      value, backgroundColor,
      min, backgroundColorMin,
      max, backgroundColorMax,
    } = this.props;

    const children = [];

    if (error) {
      children.push(span({
        className: "accessibility-color-contrast-error",
        role: "presentation",
      }, L10N.getStr("accessibility.contrast.error")));

      return (div({
        role: "presentation",
        className: "accessibility-color-contrast",
      }, ...children));
    }

    if (value) {
      children.push(ContrastValue({ isLargeText, color, backgroundColor, value }));
    } else {
      children.push(
        ContrastValue(
          { isLargeText, color, backgroundColor: backgroundColorMin, value: min }),
        div({
          role: "presentation",
          className: "accessibility-color-contrast-separator",
        }),
        ContrastValue(
          { isLargeText, color, backgroundColor: backgroundColorMax, value: max }),
      );
    }

    if (isLargeText) {
      children.push(
        span({
          className: "accessibility-color-contrast-large-text",
          role: "presentation",
          title: L10N.getStr("accessibility.contrast.large.title"),
        }, L10N.getStr("accessibility.contrast.large.text"))
      );
    }

    return (
      div(
        {
          role: "presentation",
          className: "accessibility-color-contrast",
        },
        ...children
      )
    );
  }
}

const ColorContrastAccessibility = createFactory(ColorContrastAccessibilityClass);

class ContrastAnnotationClass extends Component {
  static get propTypes() {
    return {
      isLargeText: PropTypes.bool.isRequired,
      value: PropTypes.number,
      min: PropTypes.number,
    };
  }

  render() {
    const { isLargeText, min, value } = this.props;
    const score = getContrastRatioScore(value || min, isLargeText);

    return (
      LearnMoreLink(
        {
          className: "accessibility-color-contrast-annotation",
          href: A11Y_CONTRAST_LEARN_MORE_LINK,
          learnMoreStringKey: "accessibility.learnMore",
          l10n: L10N,
          messageStringKey: `accessibility.contrast.annotation.${score}`,
        }
      )
    );
  }
}

const ContrastAnnotation = createFactory(ContrastAnnotationClass);

class ColorContrastCheck extends Component {
  static get propTypes() {
    return {
      error: PropTypes.string.isRequired,
    };
  }

  render() {
    const { error } = this.props;

    return (
      div({
        role: "presentation",
        className: "accessibility-color-contrast-check",
      },
        h3({
          className: "accessibility-color-contrast-header",
        }, L10N.getStr("accessibility.contrast.header")),
        ColorContrastAccessibility(this.props),
        !error && ContrastAnnotation(this.props)
      )
    );
  }
}

/**
 * Get contrast ratio score.
 * ratio.
 * @param  {Number} value
 *         Value of the contrast ratio for a given accessible object.
 * @param  {Boolean} isLargeText
 *         True if the accessible object contains large text.
 * @return {String}
 *         Represents the appropriate contrast ratio score.
 */
function getContrastRatioScore(value, isLargeText) {
  const levels = isLargeText ? { AA: 3, AAA: 4.5 } : { AA: 4.5, AAA: 7 };

  let score = "fail";
  if (value >= levels.AAA) {
    score = "AAA";
  } else if (value >= levels.AA) {
    score = "AA";
  }

  return score;
}

module.exports = {
  ColorContrastAccessibility: ColorContrastAccessibilityClass,
  ColorContrastCheck,
};
