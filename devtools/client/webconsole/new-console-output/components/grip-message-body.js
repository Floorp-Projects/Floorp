/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// If this is being run from Mocha, then the browser loader hasn't set up
// define. We need to do that before loading Rep.
if (typeof define === "undefined") {
  require("amd-loader");
}

// React
const {
  createFactory,
  PropTypes
} = require("devtools/client/shared/vendor/react");
const { createFactories } = require("devtools/client/shared/components/reps/rep-utils");
const { Rep } = createFactories(require("devtools/client/shared/components/reps/rep"));
const StringRep = createFactories(require("devtools/client/shared/components/reps/string").StringRep).rep;
const VariablesViewLink = createFactory(require("devtools/client/webconsole/new-console-output/components/variables-view-link").VariablesViewLink);
const { Grip } = require("devtools/client/shared/components/reps/grip");

GripMessageBody.displayName = "GripMessageBody";

GripMessageBody.propTypes = {
  grip: PropTypes.oneOfType([
    PropTypes.string,
    PropTypes.number,
    PropTypes.object,
  ]).isRequired,
};

function GripMessageBody(props) {
  const { grip } = props;

  return (
    // @TODO once there is a longString rep, also turn off quotes for those.
    typeof grip === "string"
      ? StringRep({
        object: grip,
        useQuotes: false,
        mode: props.mode,
      })
      : Rep({
        object: grip,
        objectLink: VariablesViewLink,
        defaultRep: Grip,
        mode: props.mode,
      })
  );
}

module.exports.GripMessageBody = GripMessageBody;
