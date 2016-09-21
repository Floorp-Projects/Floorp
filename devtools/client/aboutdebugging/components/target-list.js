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
    let { client, debugDisabled, error, targetClass, targets, sort } = this.props;
    if (sort) {
      targets = targets.sort(LocaleCompare);
    }
    targets = targets.map(target => {
      return targetClass({ client, target, debugDisabled });
    });

    let content = "";
    if (error) {
      content = error;
    } else if (targets.length > 0) {
      content = dom.ul({ className: "target-list" }, targets);
    } else {
      content = dom.p(null, Strings.GetStringFromName("nothing"));
    }

    return dom.div({ id: this.props.id, className: "targets" },
      dom.h2(null, this.props.name), content);
  },
});
