/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

const ExtensionPage = createFactory(
  require("resource://devtools/client/inspector/extensions/components/ExtensionPage.js")
);
const ObjectTreeView = createFactory(
  require("resource://devtools/client/inspector/extensions/components/ObjectTreeView.js")
);
const ExpressionResultView = createFactory(
  require("resource://devtools/client/inspector/extensions/components/ExpressionResultView.js")
);
const Types = require("resource://devtools/client/inspector/extensions/types.js");

/**
 * The ExtensionSidebar is a React component with 2 supported viewMode:
 * - an ObjectTreeView UI, used to show the JS objects
 *   (used by the sidebar.setObject WebExtensions APIs)
 * - an ExpressionResultView UI, used to show the result for an expression
 *   (used by sidebar.setExpression WebExtensions APIs)
 * - an ExtensionPage UI used to show an extension page
 *   (used by the sidebar.setPage WebExtensions APIs).
 *
 * TODO: implement the ExtensionPage viewMode.
 */
class ExtensionSidebar extends PureComponent {
  static get propTypes() {
    return {
      id: PropTypes.string.isRequired,
      extensionsSidebar: PropTypes.object.isRequired,
      onExtensionPageMount: PropTypes.func.isRequired,
      onExtensionPageUnmount: PropTypes.func.isRequired,
      // Helpers injected as props by extension-sidebar.js.
      serviceContainer: PropTypes.shape(Types.serviceContainer).isRequired,
    };
  }

  render() {
    const {
      id,
      extensionsSidebar,
      onExtensionPageMount,
      onExtensionPageUnmount,
      serviceContainer,
    } = this.props;

    const {
      iframeURL,
      object,
      expressionResult,
      rootTitle,
      viewMode = "empty-sidebar",
    } = extensionsSidebar[id] || {};

    let sidebarContentEl;

    switch (viewMode) {
      case "object-treeview":
        sidebarContentEl = ObjectTreeView({ object });
        break;
      case "object-value-grip-view":
        sidebarContentEl = ExpressionResultView({
          expressionResult,
          rootTitle,
          serviceContainer,
        });
        break;
      case "extension-page":
        sidebarContentEl = ExtensionPage({
          iframeURL,
          onExtensionPageMount,
          onExtensionPageUnmount,
        });
        break;
      case "empty-sidebar":
        break;
      default:
        throw new Error(`Unknown ExtensionSidebar viewMode: "${viewMode}"`);
    }

    const className = "devtools-monospace extension-sidebar inspector-tabpanel";

    return dom.div(
      {
        id,
        className,
      },
      sidebarContentEl
    );
  }
}

module.exports = connect(state => state)(ExtensionSidebar);
