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
  object: PropTypes.object.isRequired
};

function VariablesViewLink(props, ...children) {
  const { className, object } = props;
  const classes = ["cm-variable"];
  if (className) {
    classes.push(className);
  }
  return (
    dom.a({
      onClick: openVariablesView.bind(null, object),
      // Context menu can use this actor id information to enable additional menu items.
      "data-link-actor-id": object.actor,
      className: classes.join(" "),
      draggable: false,
    }, ...children)
  );
}

module.exports = VariablesViewLink;
