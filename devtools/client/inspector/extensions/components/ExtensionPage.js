/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createRef,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

/**
 * The ExtensionPage React Component is used in the ExtensionSidebar component to provide
 * a UI viewMode which shows an extension page rendered inside the sidebar panel.
 */
class ExtensionPage extends PureComponent {
  static get propTypes() {
    return {
      iframeURL: PropTypes.string.isRequired,
      onExtensionPageMount: PropTypes.func.isRequired,
      onExtensionPageUnmount: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.iframeRef = createRef();
  }

  componentDidMount() {
    this.props.onExtensionPageMount(this.iframeRef.current);
  }

  componentWillUnmount() {
    this.props.onExtensionPageUnmount(this.iframeRef.current);
  }

  render() {
    return dom.iframe({
      className: "inspector-extension-sidebar-page",
      src: this.props.iframeURL,
      style: {
        width: "100%",
        height: "100%",
        margin: 0,
        padding: 0,
      },
      ref: this.iframeRef,
    });
  }
}

module.exports = ExtensionPage;
