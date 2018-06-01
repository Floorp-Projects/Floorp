/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getStr } = require("devtools/client/inspector/layout/utils/l10n");

const Types = require("../types");

class GridDisplaySettings extends PureComponent {
  static get propTypes() {
    return {
      highlighterSettings: PropTypes.shape(Types.highlighterSettings).isRequired,
      onToggleShowGridAreas: PropTypes.func.isRequired,
      onToggleShowGridLineNumbers: PropTypes.func.isRequired,
      onToggleShowInfiniteLines: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onShowGridAreasCheckboxClick = this.onShowGridAreasCheckboxClick.bind(this);
    this.onShowGridLineNumbersCheckboxClick =
      this.onShowGridLineNumbersCheckboxClick.bind(this);
    this.onShowInfiniteLinesCheckboxClick =
      this.onShowInfiniteLinesCheckboxClick.bind(this);
  }

  onShowGridAreasCheckboxClick() {
    const {
      highlighterSettings,
      onToggleShowGridAreas,
    } = this.props;

    onToggleShowGridAreas(!highlighterSettings.showGridAreasOverlay);
  }

  onShowGridLineNumbersCheckboxClick() {
    const {
      highlighterSettings,
      onToggleShowGridLineNumbers,
    } = this.props;

    onToggleShowGridLineNumbers(!highlighterSettings.showGridLineNumbers);
  }

  onShowInfiniteLinesCheckboxClick() {
    const {
      highlighterSettings,
      onToggleShowInfiniteLines,
    } = this.props;

    onToggleShowInfiniteLines(!highlighterSettings.showInfiniteLines);
  }

  render() {
    const {
      highlighterSettings,
    } = this.props;

    return dom.div(
      {
        className: "grid-container",
      },
      dom.span(
        {},
        getStr("layout.gridDisplaySettings")
      ),
      dom.ul(
        {},
        dom.li(
          {
            className: "grid-settings-item",
          },
          dom.label(
            {},
            dom.input(
              {
                id: "grid-setting-show-grid-line-numbers",
                type: "checkbox",
                checked: highlighterSettings.showGridLineNumbers,
                onChange: this.onShowGridLineNumbersCheckboxClick,
              }
            ),
            getStr("layout.displayLineNumbers")
          )
        ),
        dom.li(
          {
            className: "grid-settings-item",
          },
          dom.label(
           {},
           dom.input(
             {
               id: "grid-setting-show-grid-areas",
               type: "checkbox",
               checked: highlighterSettings.showGridAreasOverlay,
               onChange: this.onShowGridAreasCheckboxClick,
             }
           ),
           getStr("layout.displayAreaNames")
          )
        ),
        dom.li(
          {
            className: "grid-settings-item",
          },
          dom.label(
            {},
            dom.input(
              {
                id: "grid-setting-extend-grid-lines",
                type: "checkbox",
                checked: highlighterSettings.showInfiniteLines,
                onChange: this.onShowInfiniteLinesCheckboxClick,
              }
            ),
            getStr("layout.extendLinesInfinitely")
          )
        )
      )
    );
  }
}

module.exports = GridDisplaySettings;
