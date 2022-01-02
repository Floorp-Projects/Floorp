/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

/**
 * Set text on a child element/component depending on if the message is already
 * translated plain text or a fluent id with optional args.
 */
export class FluentOrText extends React.PureComponent {
  render() {
    // Ensure we have a single child to attach attributes
    const { children, message } = this.props;
    const child = children ? React.Children.only(children) : <span />;

    // For a string message, just use it as the child's text
    let grandChildren = message;
    let extraProps;

    // Convert a message object to set desired fluent-dom attributes
    if (typeof message === "object") {
      const args = message.args || message.values;
      extraProps = {
        "data-l10n-args": args && JSON.stringify(args),
        "data-l10n-id": message.id || message.string_id,
      };

      // Use original children potentially with data-l10n-name attributes
      grandChildren = child.props.children;
    }

    // Add the message to the child via fluent attributes or text node
    return React.cloneElement(child, extraProps, grandChildren);
  }
}
