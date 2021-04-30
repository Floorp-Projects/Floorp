/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
const MS_STRING_PROP = "string_id";

/**
 * Based on the .text prop, localizes an inner element if a string_id
 * is provided, OR renders plain text, OR hides it if nothing is provided.
 *
 * Examples:
 *
 * Localized text
 * ftl:
 *  title = Welcome
 * jsx:
 *   <Localized text={{string_id: "title"}}><h1 /></Localized>
 * output:
 *   <h1 data-l10n-id="title">Welcome</h1>
 *
 * Unlocalized text
 * jsx:
 *   <Localized text="Welcome"><h1 /></Localized>
 * output:
 *   <h1>Welcome</h1>
 */

export const Localized = ({ text, children }) => {
  if (!text) {
    return null;
  }

  let props = children ? children.props : {};
  let textNode;

  if (typeof text === "object" && text[MS_STRING_PROP]) {
    props = { ...props };
    props["data-l10n-id"] = text[MS_STRING_PROP];
    if (text.args) props["data-l10n-args"] = JSON.stringify(text.args);
  } else if (typeof text === "string") {
    textNode = text;
  }

  if (!children) {
    return React.createElement("span", props, textNode);
  } else if (textNode) {
    return React.cloneElement(children, props, textNode);
  }
  return React.cloneElement(children, props);
};
