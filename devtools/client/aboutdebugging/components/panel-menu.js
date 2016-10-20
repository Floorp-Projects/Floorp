/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createClass, createFactory, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");
const PanelMenuEntry = createFactory(require("./panel-menu-entry"));

module.exports = createClass({
  displayName: "PanelMenu",

  propTypes: {
    panels: PropTypes.arrayOf(PropTypes.shape({
      id: PropTypes.string.isRequired,
      name: PropTypes.string.isRequired,
      icon: PropTypes.string.isRequired,
      component: PropTypes.func.isRequired
    })).isRequired,
    selectPanel: PropTypes.func.isRequired,
    selectedPanelId: PropTypes.string
  },

  render() {
    let { panels, selectedPanelId, selectPanel } = this.props;
    let panelLinks = panels.map(({ id, name, icon }) => {
      let selected = id == selectedPanelId;
      return PanelMenuEntry({
        id,
        name,
        icon,
        selected,
        selectPanel
      });
    });

    // "categories" id used for styling purposes
    return dom.div({ id: "categories", role: "tablist" }, panelLinks);
  },
});
