/** @jsx React.DOM */

var loop = loop || {};
loop.fxOSMarketplaceViews = (function() {
  "use strict";

  /**
   * The Firefox Marketplace exposes a web page that contains a postMesssage
   * based API that wraps a small set of functionality from the WebApps API
   * that allow us to request the installation of apps given their manifest
   * URL. We will be embedding the content of this web page within an hidden
   * iframe in case that we need to request the installation of the FxOS Loop
   * client.
   */
  var FxOSHiddenMarketplaceView = React.createClass({
    render: function() {
      return <iframe id="marketplace" src={this.props.marketplaceSrc} hidden/>;
    },

    componentDidUpdate: function() {
      // This happens only once when we change the 'src' property of the iframe.
      if (this.props.onMarketplaceMessage) {
        // The reason for listening on the global window instead of on the
        // iframe content window is because the Marketplace is doing a
        // window.top.postMessage.
        // XXX Bug 1097703: This should be changed to an action when the old
        //     style URLs go away.
        window.addEventListener("message", this.props.onMarketplaceMessage);
      }
    }
  });

  return {
    FxOSHiddenMarketplaceView: FxOSHiddenMarketplaceView
  };

})();
