/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  addons,
  createClass, createFactory,
  DOM: dom,
  PropTypes,
} = require("devtools/client/shared/vendor/react");

const { connect } = require("devtools/client/shared/vendor/react-redux");

const ObjectTreeView = createFactory(require("./ObjectTreeView"));

/**
 * The ExtensionSidebar is a React component with 2 supported viewMode:
 * - an ObjectTreeView UI, used to show the JS objects (used by the sidebar.setObject
 *   and sidebar.setExpression WebExtensions APIs)
 * - an ExtensionPage UI used to show an extension page (used by the sidebar.setPage
 *   WebExtensions APIs).
 *
 * TODO: implement the ExtensionPage viewMode.
 */
const ExtensionSidebar = createClass({
  displayName: "ExtensionSidebar",

  propTypes: {
    id: PropTypes.string.isRequired,
    extensionsSidebar: PropTypes.object.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  render() {
    const { id, extensionsSidebar } = this.props;

    let {
      viewMode = "empty-sidebar",
      object
    } = extensionsSidebar[id] || {};

    let sidebarContentEl;

    switch (viewMode) {
      case "object-treeview":
        sidebarContentEl = ObjectTreeView({ object });
        break;
      case "empty-sidebar":
        break;
      default:
        throw new Error(`Unknown ExtensionSidebar viewMode: "${viewMode}"`);
    }

    const className = "devtools-monospace extension-sidebar inspector-tabpanel";

    return dom.div({ id, className }, sidebarContentEl);
  }
});

module.exports = connect(state => state)(ExtensionSidebar);
