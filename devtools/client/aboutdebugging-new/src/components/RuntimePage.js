/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const RuntimeInfo = createFactory(require("./RuntimeInfo"));

const Services = require("Services");

class RuntimePage extends PureComponent {
  render() {
    return dom.article(
      {
        className: "page",
      },
      RuntimeInfo({
        icon: "chrome://branding/content/icon64.png",
        name: Services.appinfo.name,
        version: Services.appinfo.version,
      }),
    );
  }
}

module.exports = RuntimePage;
