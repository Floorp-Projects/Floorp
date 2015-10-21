/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global React, TargetComponent */

"use strict";

loader.lazyRequireGetter(this, "React",
  "resource://devtools/client/shared/vendor/react.js");
loader.lazyRequireGetter(this, "TargetComponent",
  "devtools/client/aboutdebugging/components/target", true);
loader.lazyRequireGetter(this, "Services");

const Strings = Services.strings.createBundle(
  "chrome://browser/locale/devtools/aboutdebugging.properties");
const LocaleCompare = (a, b) => {
  return a.name.toLowerCase().localeCompare(b.name.toLowerCase());
};

exports.TargetListComponent = React.createClass({
  displayName: "TargetListComponent",

  render() {
    let client = this.props.client;
    let targets = this.props.targets.sort(LocaleCompare).map(target => {
      return React.createElement(TargetComponent, { client, target });
    });
    return (
      React.createElement("div", { className: "targets" },
        React.createElement("h4", null, this.props.name),
        targets.length > 0 ? targets :
          React.createElement("p", null, Strings.GetStringFromName("nothing"))
      )
    );
  },
});
