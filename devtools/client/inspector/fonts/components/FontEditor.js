/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FontMeta = createFactory(require("./FontMeta"));
const FontPropertyValue = createFactory(require("./FontPropertyValue"));
const FontSize = createFactory(require("./FontSize"));
const FontStyle = createFactory(require("./FontStyle"));
const FontWeight = createFactory(require("./FontWeight"));

const { getStr } = require("../utils/l10n");
const Types = require("../types");

// Maximum number of font families to be shown by default. Any others will be hidden
// under a collapsed <details> element with a toggle to reveal them.
const MAX_FONTS = 3;

class FontEditor extends PureComponent {
  static get propTypes() {
    return {
      fontEditor: PropTypes.shape(Types.fontEditor).isRequired,
      onInstanceChange: PropTypes.func.isRequired,
      onPropertyChange: PropTypes.func.isRequired,
      onToggleFontHighlight: PropTypes.func.isRequired,
    };
  }

  /**
   * Naive implementation to get increment step for variable font axis that ensures
   * a wide spectrum of precision based on range of values between min and max.
   *
   * @param  {Number|String} min
   *         Minumum value for range.
   * @param  {Number|String} max
   *         Maximum value for range.
   * @return {String}
   *         Step value used in range input for font axis.
   */
  getAxisStep(min, max) {
    let step = 1;
    const delta = parseInt(max, 10) - parseInt(min, 10);

    if (delta <= 1) {
      step = 0.001;
    } else if (delta <= 10) {
      step = 0.01;
    } else if (delta <= 100) {
      step = 0.1;
    }

    return step.toString();
  }

  /**
   * Get a container with the rendered FontPropertyValue components with editing controls
   * for of the given variable font axes. If no axes were given, return null.
   * If an axis has a value in the fontEditor store (i.e.: it was declared in CSS or
   * it was changed using the font editor), use its value, otherwise use the font axis
   * default.
   *
   * @param  {Array} fontAxes
   *         Array of font axis instances
   * @param  {Object} editedAxes
   *         Object with axes and values edited by the user or predefined in the CSS
   *         declaration for font-variation-settings.
   * @return {DOMNode|null}
   */
  renderAxes(fontAxes = [], editedAxes) {
    if (!fontAxes.length) {
      return null;
    }

    const controls = fontAxes.map(axis => {
      return FontPropertyValue({
        key: axis.tag,
        min: axis.minValue,
        max: axis.maxValue,
        value: editedAxes[axis.tag] || axis.defaultValue,
        step: this.getAxisStep(axis.minValue, axis.maxValue),
        label: axis.name,
        name: axis.tag,
        onChange: this.props.onPropertyChange,
        unit: null
      });
    });

    return dom.div(
      {
        className: "font-axes-controls"
      },
      controls
    );
  }

  renderFamilesNotUsed(familiesNotUsed = []) {
    if (!familiesNotUsed.length) {
      return null;
    }

    const familiesList = familiesNotUsed.map(family => {
      return dom.div(
        {
          className: "font-family-unused",
        },
        family
      );
    });

    return dom.details(
      {},
      dom.summary(
        {},
        getStr("fontinspector.familiesUnusedLabel")
      ),
      familiesList
    );
  }

  /**
   * Render font family, font name, and metadata for all fonts used on selected node.
   *
   * @param {Array} fonts
   *        Fonts used on selected node.
   * @param {Array} families
   *        Font familes declared on selected node.
   * @return {DOMNode}
   */
  renderFontFamily(fonts, families) {
    if (!fonts.length) {
      return null;
    }

    const topUsedFontsList = this.renderFontList(fonts.slice(0, MAX_FONTS));
    const moreUsedFontsList = this.renderFontList(fonts.slice(MAX_FONTS, fonts.length));
    const moreUsedFonts = moreUsedFontsList === null
      ? null
      : dom.details({},
          dom.summary({},
            dom.span({ className: "label-open" }, getStr("fontinspector.showMore")),
            dom.span({ className: "label-close" }, getStr("fontinspector.showLess"))
          ),
          moreUsedFontsList
        );

    return dom.label(
      {
        className: "font-control font-control-family",
      },
      dom.span(
        {
          className: "font-control-label",
        },
        getStr("fontinspector.fontFamilyLabel")
      ),
      dom.div(
        {
          className: "font-control-box",
        },
        topUsedFontsList,
        moreUsedFonts,
        this.renderFamilesNotUsed(families.notUsed)
      )
    );
  }

  /**
   * Given an array of fonts, get an unordered list with rendered FontMeta components.
   * If the array of fonts is empty, return null.
   *
   * @param {Array} fonts
   *        Array of objects with information about fonts used on the selected node.
   * @return {DOMNode|null}
   */
  renderFontList(fonts = []) {
    if (!fonts.length) {
      return null;
    }

    return dom.ul(
      {
        className: "fonts-list"
      },
      fonts.map(font => {
        return dom.li(
          {},
          FontMeta({
            font,
            key: font.name,
            onToggleFontHighlight: this.props.onToggleFontHighlight
          })
        );
      })
    );
  }

  renderFontSize(value) {
    return value && FontSize({
      onChange: this.props.onPropertyChange,
      value,
    });
  }

  renderFontStyle(value) {
    return value && FontStyle({
      onChange: this.props.onPropertyChange,
      value,
    });
  }

  renderFontWeight(value) {
    return value && FontWeight({
      onChange: this.props.onPropertyChange,
      value,
    });
  }

  /**
   * Get a dropdown which allows selecting between variation instances defined by a font.
   *
   * @param {Array} fontInstances
   *        Named variation instances as provided with the font file.
   * @param {Object} selectedInstance
   *        Object with information about the currently selected variation instance.
   *        Example:
   *        {
   *          name: "Custom",
   *          values: []
   *        }
   * @return {DOMNode}
   */
  renderInstances(fontInstances = [], selectedInstance) {
    // Append a "Custom" instance entry which represents the latest manual axes changes.
    const customInstance = {
      name: getStr("fontinspector.customInstanceName"),
      values: this.props.fontEditor.customInstanceValues
    };
    fontInstances = [ ...fontInstances, customInstance ];

    // Generate the <option> elements for the dropdown.
    const instanceOptions = fontInstances.map(instance =>
      dom.option(
        {
          value: instance.name,
          selected: instance.name === selectedInstance.name ? "selected" : null,
        },
        instance.name
      )
    );

    // Generate the dropdown.
    const instanceSelect = dom.select(
      {
        className: "font-control-input",
        onChange: (e) => {
          const instance = fontInstances.find(inst => e.target.value === inst.name);
          instance && this.props.onInstanceChange(instance.name, instance.values);
        }
      },
      instanceOptions
    );

    return dom.label(
      {
        className: "font-control",
      },
      dom.span(
        {
          className: "font-control-label",
        },
        getStr("fontinspector.fontInstanceLabel")
      ),
      instanceSelect
    );
  }

  renderWarning(warning) {
    return dom.div(
      {
        id: "font-editor"
      },
      dom.div(
        {
          className: "devtools-sidepanel-no-result"
        },
        warning
      )
    );
  }

  render() {
    const { fontEditor } = this.props;
    const { fonts, families, axes, instance, properties, warning } = fontEditor;
    // Pick the first font to show editor controls regardless of how many fonts are used.
    const font = fonts[0];
    const hasFontAxes = font && font.variationAxes;
    const hasFontInstances = font && font.variationInstances
      && font.variationInstances.length > 0;
    const hasSlantOrItalicAxis = hasFontAxes && font.variationAxes.find(axis => {
      return axis.tag === "slnt" || axis.tag === "ital";
    });
    const hasWeightAxis = hasFontAxes && font.variationAxes.find(axis => {
      return axis.tag === "wght";
    });

    // Show the empty state with a warning message when a used font was not found.
    if (!font) {
      return this.renderWarning(warning);
    }

    return dom.div(
      {
        id: "font-editor"
      },
      // Always render UI for font family, format and font file URL.
      this.renderFontFamily(fonts, families),
      // Render UI for font variation instances if they are defined.
      hasFontInstances && this.renderInstances(font.variationInstances, instance),
      // Always render UI for font size.
      this.renderFontSize(properties["font-size"]),
      // Render UI for font weight if no "wght" registered axis is defined.
      !hasWeightAxis && this.renderFontWeight(properties["font-weight"]),
      // Render UI for font style if no "slnt" or "ital" registered axis is defined.
      !hasSlantOrItalicAxis && this.renderFontStyle(properties["font-style"]),
      // Render UI for each variable font axis if any are defined.
      hasFontAxes && this.renderAxes(font.variationAxes, axes)
    );
  }
}

module.exports = FontEditor;
