/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, createFactory, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const Accordion = createFactory(require("./Accordion"));
const Grid = createFactory(require("./Grid"));

const Types = require("../types");
const { getStr } = require("../utils/l10n");

const App = createClass({

  displayName: "App",

  propTypes: {
    grids: PropTypes.arrayOf(PropTypes.shape(Types.grid)).isRequired,
    onToggleGridHighlighter: PropTypes.func.isRequired,
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
            header: getStr("layout.header"),
            component: Grid,
            componentProps: this.props,
            opened: true
          }
        ]
      })
    );
  },

});

module.exports = connect(state => state)(App);
