/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const {
  getSelectorFromGrip,
  translateNodeFrontToGrip,
} = require("devtools/client/inspector/shared/utils");
const { LocalizationHelper } = require("devtools/shared/l10n");

const Accordion = createFactory(require("./Accordion"));
const BoxModel = createFactory(require("devtools/client/inspector/boxmodel/components/BoxModel"));
const Flexbox = createFactory(require("devtools/client/inspector/flexbox/components/Flexbox"));
const Grid = createFactory(require("devtools/client/inspector/grids/components/Grid"));

const BoxModelTypes = require("devtools/client/inspector/boxmodel/types");
const FlexboxTypes = require("devtools/client/inspector/flexbox/types");
const GridTypes = require("devtools/client/inspector/grids/types");

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
      flexbox: PropTypes.shape(FlexboxTypes.flexbox).isRequired,
      getSwatchColorPickerTooltip: PropTypes.func.isRequired,
      grids: PropTypes.arrayOf(PropTypes.shape(GridTypes.grid)).isRequired,
      highlighterSettings: PropTypes.shape(GridTypes.highlighterSettings).isRequired,
      onHideBoxModelHighlighter: PropTypes.func.isRequired,
      onSetFlexboxOverlayColor: PropTypes.func.isRequired,
      onSetGridOverlayColor: PropTypes.func.isRequired,
      onShowBoxModelEditor: PropTypes.func.isRequired,
      onShowBoxModelHighlighter: PropTypes.func.isRequired,
      onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
      onShowGridOutlineHighlight: PropTypes.func.isRequired,
      onToggleFlexboxHighlighter: PropTypes.func.isRequired,
      onToggleGeometryEditor: PropTypes.func.isRequired,
      onToggleGridHighlighter: PropTypes.func.isRequired,
      onToggleShowGridAreas: PropTypes.func.isRequired,
      onToggleShowGridLineNumbers: PropTypes.func.isRequired,
      onToggleShowInfiniteLines: PropTypes.func.isRequired,
      setSelectedNode: PropTypes.func.isRequired,
      showBoxModelProperties: PropTypes.bool.isRequired,
    };
  }

  getFlexboxHeader(flexContainer) {
    if (!flexContainer.actorID) {
      // No flex container or flex item selected.
      return LAYOUT_L10N.getStr("flexbox.header");
    } else if (!flexContainer.flexItemShown) {
      // No flex item selected.
      return LAYOUT_L10N.getStr("flexbox.flexContainer");
    }

    const grip = translateNodeFrontToGrip(flexContainer.nodeFront);
    return LAYOUT_L10N.getFormatStr("flexbox.flexItemOf", getSelectorFromGrip(grip));
  }

  render() {
    const items = [
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
      // Since the flexbox panel is hidden behind a pref. We insert the flexbox container
      // to the first index of the accordion item list.
      items.splice(0, 0, {
        component: Flexbox,
        componentProps: {
          ...this.props,
          flexContainer: this.props.flexbox.flexContainer,
        },
        header: this.getFlexboxHeader(this.props.flexbox.flexContainer),
        opened: Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF),
        onToggled: () => {
          const opened =  Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF);
          Services.prefs.setBoolPref(FLEXBOX_OPENED_PREF, !opened);
        }
      });

      // If the current selected node is both a flex container and flex item. Render
      // an accordion with another Flexbox component where the flexbox to show is the
      // parent flex container of the current selected node.
      if (this.props.flexbox.flexItemContainer &&
          this.props.flexbox.flexItemContainer.actorID) {
        // Insert the parent flex container to the first index of the accordion item
        // list.
        items.splice(0, 0, {
          component: Flexbox,
          componentProps: {
            ...this.props,
            flexContainer: this.props.flexbox.flexItemContainer,
          },
          header: this.getFlexboxHeader(this.props.flexbox.flexItemContainer),
          opened: Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF),
          onToggled: () => {
            const opened =  Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF);
            Services.prefs.setBoolPref(FLEXBOX_OPENED_PREF, !opened);
          }
        });
      }
    }

    return (
      dom.div({ id: "layout-container" },
        Accordion({ items })
      )
    );
  }
}

module.exports = connect(state => state)(LayoutApp);
