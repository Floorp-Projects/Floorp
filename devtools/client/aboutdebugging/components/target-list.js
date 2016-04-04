/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createClass, DOM: dom } =
  require("devtools/client/shared/vendor/react");
const Services = require("Services");

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const LocaleCompare = (a, b) => {
  return a.name.toLowerCase().localeCompare(b.name.toLowerCase());
};

module.exports = createClass({
  displayName: "TargetList",

  render() {
    let { client, debugDisabled, targetClass } = this.props;
    let targets = this.props.targets.sort(LocaleCompare).map(target => {
      return targetClass({ client, target, debugDisabled });
    });

    return dom.div({ id: this.props.id, className: "targets" },
      dom.h4(null, this.props.name),
      targets.length > 0 ?
        targets :
        dom.p(null, Strings.GetStringFromName("nothing"))
    );
  },
});
