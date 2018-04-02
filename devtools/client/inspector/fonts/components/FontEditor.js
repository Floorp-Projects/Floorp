/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FontAxis = createFactory(require("./FontAxis"));

const Types = require("../types");

class FontEditor extends PureComponent {
  static get propTypes() {
    return {
      fontEditor: PropTypes.shape(Types.fontEditor).isRequired,
      onAxisUpdate: PropTypes.func.isRequired,
    };
  }

  /**
   * Naive implementation to get increment step for variable font axis that ensures
   * a wide spectrum of precision based on range of values between min and max.
   *
   * @param {Number|String} min
   *        Minumum value for range.
   * @param {Number|String} max
   *        Maximum value for range.
   * @return {String}
   *        Step value used in range input for font axis.
   */
  getAxisStep(min, max) {
    let step = 1;
    let delta = parseInt(max, 10) - parseInt(min, 10);

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
   * Get an array of FontAxis components for of the given variable font axis instances.
   * If an axis is defined in the fontEditor store, use its value, else use the default.
   *
   * @param {Array} fontAxes
   *        Array of font axis instances
   * @param {Object} editedAxes
   *        Object with axes and values edited by the user or predefined in the CSS
   *        declaration for font-variation-settings.
   * @return {Array}
  *          Array of FontAxis components
   */
  renderAxes(fontAxes = [], editedAxes) {
    return fontAxes.map(axis => {
      return FontAxis({
        min: axis.minValue,
        max: axis.maxValue,
        value: editedAxes[axis.tag] || axis.defaultValue,
        step: this.getAxisStep(axis.minValue, axis.maxValue),
        label: axis.name,
        name: axis.tag,
        onChange: this.props.onAxisUpdate,
        showInput: true
      });
    });
  }

  // Placeholder for non-variable font UI.
  // Bug https://bugzilla.mozilla.org/show_bug.cgi?id=1450695
  renderPlaceholder() {
    return dom.div({}, "No fonts with variation axes apply to this element.");
  }

  render() {
    const { fonts, axes } = this.props.fontEditor;
    // For MVP use ony first font to show axes if available.
    // Future implementations will allow switching between multiple fonts.
    const fontAxes = (fonts[0] && fonts[0].variationAxes) ? fonts[0].variationAxes : null;

    return dom.div(
      {
        className: "theme-sidebar inspector-tabpanel",
        id: "sidebar-panel-fontinspector"
      },
      fontAxes ?
        this.renderAxes(fontAxes, axes)
        :
        this.renderPlaceholder()
    );
  }
}

module.exports = FontEditor;
