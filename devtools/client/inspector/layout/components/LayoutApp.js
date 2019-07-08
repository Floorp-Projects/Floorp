/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  createFactory,
  createRef,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const {
  getSelectorFromGrip,
  translateNodeFrontToGrip,
} = require("devtools/client/inspector/shared/utils");
const { LocalizationHelper } = require("devtools/shared/l10n");

const Accordion = createFactory(require("./Accordion"));
const BoxModel = createFactory(
  require("devtools/client/inspector/boxmodel/components/BoxModel")
);
const Flexbox = createFactory(
  require("devtools/client/inspector/flexbox/components/Flexbox")
);
const Grid = createFactory(
  require("devtools/client/inspector/grids/components/Grid")
);

const BoxModelTypes = require("devtools/client/inspector/boxmodel/types");
const FlexboxTypes = require("devtools/client/inspector/flexbox/types");
const GridTypes = require("devtools/client/inspector/grids/types");

const BOXMODEL_STRINGS_URI = "devtools/client/locales/boxmodel.properties";
const BOXMODEL_L10N = new LocalizationHelper(BOXMODEL_STRINGS_URI);

const LAYOUT_STRINGS_URI = "devtools/client/locales/layout.properties";
const LAYOUT_L10N = new LocalizationHelper(LAYOUT_STRINGS_URI);

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
      highlighterSettings: PropTypes.shape(GridTypes.highlighterSettings)
        .isRequired,
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

  constructor(props) {
    super(props);
    this.containerRef = createRef();

    this.scrollToTop = this.scrollToTop.bind(this);
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
    return LAYOUT_L10N.getFormatStr(
      "flexbox.flexItemOf",
      getSelectorFromGrip(grip)
    );
  }

  /**
   * Scrolls to top of the layout container.
   */
  scrollToTop() {
    this.containerRef.current.scrollTop = 0;
  }

  render() {
    const { flexContainer, flexItemContainer } = this.props.flexbox;

    const items = [
      {
        className: `flex-accordion ${
          flexContainer.flexItemShown ? "item" : "container"
        }`,
        component: Flexbox,
        componentProps: {
          ...this.props,
          flexContainer,
          scrollToTop: this.scrollToTop,
        },
        header: this.getFlexboxHeader(flexContainer),
        opened: Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF),
        onToggled: () => {
          Services.prefs.setBoolPref(
            FLEXBOX_OPENED_PREF,
            !Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF)
          );
        },
      },
      {
        component: Grid,
        componentProps: this.props,
        header: LAYOUT_L10N.getStr("layout.header"),
        opened: Services.prefs.getBoolPref(GRID_OPENED_PREF),
        onToggled: () => {
          const opened = Services.prefs.getBoolPref(GRID_OPENED_PREF);
          Services.prefs.setBoolPref(GRID_OPENED_PREF, !opened);
        },
      },
      {
        component: BoxModel,
        componentProps: this.props,
        header: BOXMODEL_L10N.getStr("boxmodel.title"),
        opened: Services.prefs.getBoolPref(BOXMODEL_OPENED_PREF),
        onToggled: () => {
          const opened = Services.prefs.getBoolPref(BOXMODEL_OPENED_PREF);
          Services.prefs.setBoolPref(BOXMODEL_OPENED_PREF, !opened);
        },
      },
    ];

    // If the current selected node is both a flex container and flex item. Render
    // an extra accordion with another Flexbox component where the node is shown as an
    // item of its parent flex container.
    // If the node was selected from the markup-view, then show this accordion after the
    // container accordion. Otherwise show it first.
    // The reason is that if the user selects an item-container in the markup view, it
    // is assumed that they want to primarily see that element as a container, so the
    // container info should be at the top.
    if (flexItemContainer && flexItemContainer.actorID) {
      items.splice(
        this.props.flexbox.initiatedByMarkupViewSelection ? 1 : 0,
        0,
        {
          className: "flex-accordion item",
          component: Flexbox,
          componentProps: {
            ...this.props,
            flexContainer: flexItemContainer,
            scrollToTop: this.scrollToTop,
          },
          header: this.getFlexboxHeader(flexItemContainer),
          opened: Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF),
          onToggled: () => {
            Services.prefs.setBoolPref(
              FLEXBOX_OPENED_PREF,
              !Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF)
            );
          },
        }
      );
    }

    return dom.div(
      { className: "layout-container", ref: this.containerRef },
      Accordion({ items })
    );
  }
}

module.exports = connect(state => state)(LayoutApp);
