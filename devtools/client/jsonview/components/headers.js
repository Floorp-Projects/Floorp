/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

define(function (require, exports, module) {
  const { DOM: dom, createFactory, createClass, PropTypes } = require("devtools/client/shared/vendor/react");

  const { div, span, table, tbody, tr, td, } = dom;

  /**
   * This template is responsible for rendering basic layout
   * of the 'Headers' panel. It displays HTTP headers groups such as
   * received or response headers.
   */
  let Headers = createClass({
    displayName: "Headers",

    propTypes: {
      data: PropTypes.object,
    },

    getInitialState: function () {
      return {};
    },

    render: function () {
      let data = this.props.data;

      return (
        div({className: "netInfoHeadersTable"},
          div({className: "netHeadersGroup"},
            div({className: "netInfoHeadersGroup"},
              JSONView.Locale.$STR("jsonViewer.responseHeaders")
            ),
            table({cellPadding: 0, cellSpacing: 0},
              HeaderList({headers: data.response})
            )
          ),
          div({className: "netHeadersGroup"},
            div({className: "netInfoHeadersGroup"},
              JSONView.Locale.$STR("jsonViewer.requestHeaders")
            ),
            table({cellPadding: 0, cellSpacing: 0},
              HeaderList({headers: data.request})
            )
          )
        )
      );
    }
  });

  /**
   * This template renders headers list,
   * name + value pairs.
   */
  let HeaderList = createFactory(createClass({
    displayName: "HeaderList",

    propTypes: {
      headers: PropTypes.arrayOf(PropTypes.shape({
        name: PropTypes.string,
        value: PropTypes.string
      }))
    },

    getInitialState: function () {
      return {
        headers: []
      };
    },

    render: function () {
      let headers = this.props.headers;

      headers.sort(function (a, b) {
        return a.name > b.name ? 1 : -1;
      });

      let rows = [];
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
  }));

  // Exports from this module
  exports.Headers = Headers;
});
