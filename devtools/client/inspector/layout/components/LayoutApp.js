/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { LocalizationHelper } = require("devtools/shared/l10n");

const BoxModel = createFactory(require("devtools/client/inspector/boxmodel/components/BoxModel"));
const Flexbox = createFactory(require("devtools/client/inspector/flexbox/components/Flexbox"));
const Grid = createFactory(require("devtools/client/inspector/grids/components/Grid"));

const BoxModelTypes = require("devtools/client/inspector/boxmodel/types");
const GridTypes = require("devtools/client/inspector/grids/types");

const Accordion = createFactory(require("./Accordion"));

const BOXMODEL_STRINGS_URI = "devtools/client/locales/boxmodel.properties";
const BOXMODEL_L10N = new LocalizationHelper(BOXMODEL_STRINGS_URI);

const LAYOUT_STRINGS_URI = "devtools/client/locales/layout.properties";
const LAYOUT_L10N = new LocalizationHelper(LAYOUT_STRINGS_URI);

const FLEXBOX_ENABLED_PREF = "devtools.flexboxinspector.enabled";

const BOXMODEL_OPENED_PREF = "devtools.layout.boxmodel.opened";
const FLEXBOX_OPENED_PREF = "devtools.layout.flexbox.opened";
const GRID_OPENED_PREF = "devtools.layout.grid.opened";

class LayoutApp extends PureComponent {
  static get propTypes() {
    return {
      boxModel: PropTypes.shape(BoxModelTypes.boxModel).isRequired,
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      grids: PropTypes.arrayOf(PropTypes.shape(GridTypes.grid)).isRequired,
      highlighterSettings: PropTypes.shape(GridTypes.highlighterSettings).isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      showBoxModelProperties: PropTypes.bool.isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onSetFlexboxOverlayColor: PropTypes.func.isRequired,
      onSetGridOverlayColor: PropTypes.func.isRequired,
      onShowBoxModelEditor: PropTypes.func.isRequired,
      onShowBoxModelHighlighter: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onToggleGridHighlighter: PropTypes.func.isRequired,
      onToggleShowGridLineNumbers: PropTypes.func.isRequired,
      onToggleShowInfiniteLines: PropTypes.func.isRequired,
    };
  }

  render() {
    let items = [
      {
        component: Grid,
        componentProps: this.props,
        header: LAYOUT_L10N.getStr("layout.header"),
        opened: Services.prefs.getBoolPref(GRID_OPENED_PREF),
        onToggled: () => {
          const opened = Services.prefs.getBoolPref(GRID_OPENED_PREF);
          Services.prefs.setBoolPref(GRID_OPENED_PREF, !opened);
        }
      },
      {
        component: BoxModel,
        componentProps: this.props,
        header: BOXMODEL_L10N.getStr("boxmodel.title"),
        opened: Services.prefs.getBoolPref(BOXMODEL_OPENED_PREF),
        onToggled: () => {
          const opened = Services.prefs.getBoolPref(BOXMODEL_OPENED_PREF);
          Services.prefs.setBoolPref(BOXMODEL_OPENED_PREF, !opened);
        }
      },
    ];

    if (Services.prefs.getBoolPref(FLEXBOX_ENABLED_PREF)) {
      items = [
        {
          component: Flexbox,
          componentProps: this.props,
          header: LAYOUT_L10N.getStr("flexbox.header"),
          opened: Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF),
          onToggled: () => {
            const opened =  Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF);
            Services.prefs.setBoolPref(FLEXBOX_OPENED_PREF, !opened);
          }
        },
        ...items
      ];
    }

    return dom.div(
      { id: "layout-container" },
      Accordion({ items })
    );
  }
}

module.exports = connect(state => state)(LayoutApp);
