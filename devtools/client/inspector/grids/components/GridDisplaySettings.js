/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");
const { getStr } = require("../utils/l10n");

module.exports = createClass({

  displayName: "GridDisplaySettings",

  propTypes: {
    highlighterSettings: PropTypes.shape(Types.highlighterSettings).isRequired,
    onToggleShowGridAreas: PropTypes.func.isRequired,
    onToggleShowGridLineNumbers: PropTypes.func.isRequired,
    onToggleShowInfiniteLines: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  onShowGridAreasCheckboxClick() {
    let {
      highlighterSettings,
      onToggleShowGridAreas,
    } = this.props;

    onToggleShowGridAreas(!highlighterSettings.showGridAreasOverlay);
  },

  onShowGridLineNumbersCheckboxClick() {
    let {
      highlighterSettings,
      onToggleShowGridLineNumbers,
    } = this.props;

    onToggleShowGridLineNumbers(!highlighterSettings.showGridLineNumbers);
  },

  onShowInfiniteLinesCheckboxClick() {
    let {
      highlighterSettings,
      onToggleShowInfiniteLines,
    } = this.props;

    onToggleShowInfiniteLines(!highlighterSettings.showInfiniteLines);
  },

  render() {
    let {
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
                id: "grid-setting-extend-grid-lines",
                type: "checkbox",
                checked: highlighterSettings.showInfiniteLines,
                onChange: this.onShowInfiniteLinesCheckboxClick,
              }
            ),
            getStr("layout.extendGridLinesInfinitely")
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
                id: "grid-setting-show-grid-line-numbers",
                type: "checkbox",
                checked: highlighterSettings.showGridLineNumbers,
                onChange: this.onShowGridLineNumbersCheckboxClick,
              }
            ),
            getStr("layout.displayNumbersOnLines")
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
           getStr("layout.displayGridAreas")
          )
        )
      )
    );
  },

});
