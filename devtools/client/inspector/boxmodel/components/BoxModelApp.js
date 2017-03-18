/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, createFactory, PropTypes } =
  require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const { LocalizationHelper } = require("devtools/shared/l10n");

const Accordion =
  createFactory(require("devtools/client/inspector/layout/components/Accordion"));
const BoxModel = createFactory(require("./BoxModel"));

const Types = require("../types");

const BOXMODEL_STRINGS_URI = "devtools/client/locales/boxmodel.properties";
const BOXMODEL_L10N = new LocalizationHelper(BOXMODEL_STRINGS_URI);

const BoxModelApp = createClass({

  displayName: "BoxModelApp",

  propTypes: {
    boxModel: PropTypes.shape(Types.boxModel).isRequired,
    setSelectedNode: PropTypes.func.isRequired,
    showBoxModelProperties: PropTypes.bool.isRequired,
    onHideBoxModelHighlighter: PropTypes.func.isRequired,
    onShowBoxModelEditor: PropTypes.func.isRequired,
    onShowBoxModelHighlighter: PropTypes.func.isRequired,
    onShowBoxModelHighlighterForNode: PropTypes.func.isRequired,
    onToggleGeometryEditor: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  render() {
    return Accordion({
      items: [
        {
          header: BOXMODEL_L10N.getStr("boxmodel.title"),
          component: BoxModel,
          componentProps: this.props,
          opened: true,
        }
      ]
    });
  },

});

module.exports = connect(state => state)(BoxModelApp);
