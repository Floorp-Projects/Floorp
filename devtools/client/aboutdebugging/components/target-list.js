/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global React */

"use strict";

loader.lazyRequireGetter(this, "React",
  "devtools/client/shared/vendor/react");
loader.lazyRequireGetter(this, "Target",
  "devtools/client/aboutdebugging/components/target", true);
loader.lazyRequireGetter(this, "Services");

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");
const LocaleCompare = (a, b) => {
  return a.name.toLowerCase().localeCompare(b.name.toLowerCase());
};

exports.TargetList = React.createClass({
  displayName: "TargetList",

  render() {
    let { client, debugDisabled } = this.props;
    let targets = this.props.targets.sort(LocaleCompare).map(target => {
      return React.createElement(Target, { client, target, debugDisabled });
    });
    return (
      React.createElement("div", { id: this.props.id, className: "targets" },
        React.createElement("h4", null, this.props.name),
        targets.length > 0 ? targets :
          React.createElement("p", null, Strings.GetStringFromName("nothing"))
      )
    );
  },
});
