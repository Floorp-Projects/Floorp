/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function(require, exports, module) {
  const { createFactory, Component } = require("devtools/client/shared/vendor/react");
  const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
  const dom = require("devtools/client/shared/vendor/react-dom-factories");

  const { div, span, table, tbody, tr, td, } = dom;

  /**
   * This template is responsible for rendering basic layout
   * of the 'Headers' panel. It displays HTTP headers groups such as
   * received or response headers.
   */
  class Headers extends Component {
    static get propTypes() {
      return {
        data: PropTypes.object,
      };
    }

    constructor(props) {
      super(props);
      this.state = {};
    }

    render() {
      const data = this.props.data;

      return (
        div({className: "netInfoHeadersTable"},
          div({className: "netHeadersGroup"},
            div({className: "netInfoHeadersGroup"},
              JSONView.Locale.$STR("jsonViewer.responseHeaders")
            ),
            table({cellPadding: 0, cellSpacing: 0},
              HeaderListFactory({headers: data.response})
            )
          ),
          div({className: "netHeadersGroup"},
            div({className: "netInfoHeadersGroup"},
              JSONView.Locale.$STR("jsonViewer.requestHeaders")
            ),
            table({cellPadding: 0, cellSpacing: 0},
              HeaderListFactory({headers: data.request})
            )
          )
        )
      );
    }
  }

  /**
   * This template renders headers list,
   * name + value pairs.
   */
  class HeaderList extends Component {
    static get propTypes() {
      return {
        headers: PropTypes.arrayOf(PropTypes.shape({
          name: PropTypes.string,
          value: PropTypes.string
        }))
      };
    }

    constructor(props) {
      super(props);

      this.state = {
        headers: []
      };
    }

    render() {
      const headers = this.props.headers;

      headers.sort(function(a, b) {
        return a.name > b.name ? 1 : -1;
      });

      const rows = [];
      headers.forEach(header => {
        rows.push(
          tr({key: header.name},
            td({className: "netInfoParamName"},
              span({title: header.name}, header.name)
            ),
            td({className: "netInfoParamValue"}, header.value)
          )
        );
      });

      return (
        tbody({},
          rows
        )
      );
    }
  }

  const HeaderListFactory = createFactory(HeaderList);

  // Exports from this module
  exports.Headers = Headers;
});
