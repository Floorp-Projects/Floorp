/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, createFactory, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const { LocalizationHelper } = require("devtools/shared/l10n");

const Accordion = createFactory(require("./Accordion"));
const Grid = createFactory(require("./Grid"));

const BoxModel = createFactory(require("devtools/client/inspector/boxmodel/components/BoxModel"));

const Types = require("../types");
const { getStr } = require("../utils/l10n");

const BOXMODEL_STRINGS_URI = "devtools/client/locales/boxmodel.properties";
const BOXMODEL_L10N = new LocalizationHelper(BOXMODEL_STRINGS_URI);

const App = createClass({

  displayName: "App",

  propTypes: {
    boxModel: PropTypes.shape(Types.boxModel).isRequired,
    getSwatchColorPickerTooltip: PropTypes.func.isRequired,
    grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
    highlighterSettings: PropTypes.shape(Types.highlighterSettings).isRequired,
    setSelectedNode: PropTypes.func.isRequired,
    showBoxModelProperties: PropTypes.bool.isRequired,
    onHideBoxModelHighlighter: PropTypes.func.isRequired,
    onSetGridOverlayColor: PropTypes.func.isRequired,
    onShowBoxModelEditor: PropTypes.func.isRequired,
    onShowBoxModelHighlighter: PropTypes.func.isRequired,
    onToggleGridHighlighter: PropTypes.func.isRequired,
    onToggleShowGridLineNumbers: PropTypes.func.isRequired,
    onToggleShowInfiniteLines: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  render() {
    return dom.div(
      {
        id: "layout-container",
      },
      Accordion({
        items: [
          {
            header: BOXMODEL_L10N.getStr("boxmodel.title"),
            component: BoxModel,
            componentProps: this.props,
            opened: true,
          },
          {
            header: getStr("layout.header"),
            component: Grid,
            componentProps: this.props,
            opened: true,
          },
        ]
      })
    );
  },

});

module.exports = connect(state => state)(App);
