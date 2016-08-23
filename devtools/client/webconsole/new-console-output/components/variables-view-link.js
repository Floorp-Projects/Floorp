/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  DOM: dom,
  PropTypes
} = require("devtools/client/shared/vendor/react");
const {openVariablesView} = require("devtools/client/webconsole/new-console-output/utils/variables-view");

VariablesViewLink.displayName = "VariablesViewLink";

VariablesViewLink.propTypes = {
  object: PropTypes.object.required
};

function VariablesViewLink(props) {
  const { object, children } = props;

  return (
    dom.a({
      onClick: openVariablesView.bind(null, object),
      className: "cm-variable",
      draggable: false,
    }, children)
  );
}

module.exports.VariablesViewLink = VariablesViewLink;
