/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useEffect } from "react";
const CONFIGURABLE_STYLES = [
  "color",
  "fontSize",
  "fontWeight",
  "letterSpacing",
  "lineHeight",
  "marginBlock",
  "marginInline",
  "paddingBlock",
  "paddingInline",
];
const ZAP_SIZE_THRESHOLD = 160;

/**
 * Based on the .text prop, localizes an inner element if a string_id
 * is provided, OR renders plain text, OR hides it if nothing is provided.
 * Allows configuring of some styles including zap underline and color.
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
 *   <Localized text={{raw: "Welcome"}}><h1 /></Localized>
 * output:
 *   <h1>Welcome</h1>
 */

export const Localized = ({ text, children }) => {
  // Dynamically determine the size of the zap style.
  const zapRef = React.createRef();
  useEffect(() => {
    const { current } = zapRef;
    if (current)
      requestAnimationFrame(() =>
        current?.classList.replace(
          "short",
          current.getBoundingClientRect().width > ZAP_SIZE_THRESHOLD
            ? "long"
            : "short"
        )
      );
  });

  // Skip rendering of children with no text.
  if (!text) {
    return null;
  }

  // Allow augmenting existing child container properties.
  const props = { children: [], className: "", style: {}, ...children?.props };
  // Support nested Localized by starting with their children.
  const textNodes = props.children;

  // Pick desired fluent or raw/plain text to render.
  if (text.string_id) {
    // Set the key so React knows not to reuse when switching to plain text.
    props.key = text.string_id;
    props["data-l10n-id"] = text.string_id;
    if (text.args) props["data-l10n-args"] = JSON.stringify(text.args);
  } else if (text.raw) {
    textNodes.push(text.raw);
  } else if (typeof text === "string") {
    textNodes.push(text);
  }

  // Add zap style and content in a way that allows fluent to insert too.
  if (text.zap) {
    props.className += " welcomeZap";
    textNodes.push(
      <span className="short zap" data-l10n-name="zap" ref={zapRef}>
        {text.zap}
      </span>
    );
  }

  // Apply certain configurable styles.
  CONFIGURABLE_STYLES.forEach(style => {
    if (text[style] !== undefined) props.style[style] = text[style];
  });

  return React.cloneElement(
    // Provide a default container for the text if necessary.
    children ?? <span />,
    props,
    // Conditionally pass in as void elements can't accept empty array.
    textNodes.length ? textNodes : null
  );
};
