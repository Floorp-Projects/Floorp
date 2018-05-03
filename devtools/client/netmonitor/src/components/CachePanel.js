/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 "use strict";

 const { Component, createFactory } = require("devtools/client/shared/vendor/react");
 const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
 const dom = require("devtools/client/shared/vendor/react-dom-factories");
 const { L10N } = require("../utils/l10n");
 const { fetchNetworkUpdatePacket } = require("../utils/request-utils");

 // Components
 const TreeViewClass = require("devtools/client/shared/components/tree/TreeView");
 const PropertiesView = createFactory(require("./PropertiesView"));

 const { div, input } = dom;

 const CACHE = L10N.getStr("netmonitor.cache.cache");
 const DATA_SIZE = L10N.getStr("netmonitor.cache.dataSize");
 const EXPIRES = L10N.getStr("netmonitor.cache.expires");
 const FETCH_COUNT = L10N.getStr("netmonitor.cache.fetchCount");
 const LAST_FETCHED = L10N.getStr("netmonitor.cache.lastFetched");
 const LAST_MODIFIED = L10N.getStr("netmonitor.cache.lastModified");
 const DEVICE = L10N.getStr("netmonitor.cache.device");
 const NOT_AVAILABLE = L10N.getStr("netmonitor.cache.notAvailable");
 const EMPTY = L10N.getStr("netmonitor.cache.empty");

 /**
  * Cache panel component
  * This tab lists full details of any cache information of the response.
  */
 class CachePanel extends Component {
   static get propTypes() {
     return {
      connector: PropTypes.object.isRequired,
      openLink: PropTypes.func,
      request: PropTypes.object.isRequired,
     };
   }

   componentDidMount() {
     let { connector, request } = this.props;
     fetchNetworkUpdatePacket(connector.requestData, request, ["responseCache"]);
   }

   componentWillReceiveProps(nextProps) {
     let { connector, request } = nextProps;
     fetchNetworkUpdatePacket(connector.requestData, request, ["responseCache"]);
   }

   renderSummary(label, value) {
     return (
       div({ className: "tabpanel-summary-container cache-summary"},
        div({
          className: "tabpanel-summary-label cache-summary-label",
        }, label + ":"),
        input({
          className: "tabpanel-summary-value textbox-input devtools-monospace",
          readOnly: true,
          value
        }),
      )
     );
   }

   getProperties(responseCache) {
     let responseCacheObj;
     let cacheObj;
     try {
       responseCacheObj = Object.assign({}, responseCache);
       responseCacheObj = responseCacheObj.cache;
     } catch (e) {
       return null;
     }
     try {
       cacheObj = Object.assign({}, responseCacheObj);
     } catch (e) {
       return null;
     }
     return cacheObj;
   }

   getDate(timestamp) {
     if (!timestamp) {
       return null;
     }
     let d = new Date(parseInt(timestamp, 10) * 1000);
     return d.toLocaleDateString() + " " + d.toLocaleTimeString();
   }

   render() {
     let {
      request,
      openLink,
     } = this.props;
     let { responseCache } = request;

     let object;
     let cache = this.getProperties(responseCache);

     if (cache.lastFetched || cache.fetchCount || cache.dataSize
        || cache.lastModified | cache.expires || cache.device) {
       object = {
        [CACHE]: {
          [LAST_FETCHED]: this.getDate(cache.lastFetched) || NOT_AVAILABLE,
          [FETCH_COUNT]: cache.fetchCount || NOT_AVAILABLE,
          [DATA_SIZE]: cache.dataSize || NOT_AVAILABLE,
          [LAST_MODIFIED]: this.getDate(cache.lastModified) || NOT_AVAILABLE,
          [EXPIRES]: this.getDate(cache.expires) || NOT_AVAILABLE,
          [DEVICE]: cache.device || NOT_AVAILABLE
        }
       };
     } else {
       return div({ className: "empty-notice" },
        EMPTY
      );
     }

     return div({ className: "panel-container security-panel" },
      PropertiesView({
        object,
        enableFilter: false,
        expandedNodes: TreeViewClass.getExpandedNodes(object),
        openLink,
      })
    );
   }
 }

 module.exports = CachePanel;
