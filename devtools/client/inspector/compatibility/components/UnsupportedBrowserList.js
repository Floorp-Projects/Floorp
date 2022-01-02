/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const UnsupportedBrowserItem = createFactory(
  require("devtools/client/inspector/compatibility/components/UnsupportedBrowserItem")
);

const Types = require("devtools/client/inspector/compatibility/types");

class UnsupportedBrowserList extends PureComponent {
  static get propTypes() {
    return {
      browsers: PropTypes.arrayOf(PropTypes.shape(Types.browser)).isRequired,
    };
  }

  render() {
    const { browsers } = this.props;

    // Aggregate the browser version and the aliase by the browser id.
    // Convert from
    // [{id, name, status, version}, ...]
    // to
    // {
    //   id: {
    //         name,
    //         versions: [{alias <- status, version}, ...],
    //       },
    //   ...
    // }
    const browsersMap = browsers.reduce(
      (map, { id, name, status: alias, version }) => {
        if (!map.has(id)) {
          map.set(id, { name, versions: [] });
        }
        map.get(id).versions.push({ alias, version });

        return map;
      },
      new Map()
    );

    return dom.ul(
      {
        className: "compatibility-unsupported-browser-list",
      },
      [...browsersMap.entries()].map(([id, { name, versions }]) =>
        UnsupportedBrowserItem({ key: id, id, name, versions })
      )
    );
  }
}

module.exports = UnsupportedBrowserList;
