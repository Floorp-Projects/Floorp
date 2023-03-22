/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FluentBundle, FluentResource } from "@fluent/bundle";

/**
 * Properties that allow rich text MUST be added to this list.
 *   key: the localization_id that should be used
 *   value: a property or array of properties on the message.content object
 */
const RICH_TEXT_CONFIG = {
  text: ["text", "scene1_text"],
  success_text: "success_text",
  error_text: "error_text",
  scene2_text: "scene2_text",
  amo_html: "amo_html",
  privacy_html: "scene2_privacy_html",
  disclaimer_html: "scene2_disclaimer_html",
};

export const RICH_TEXT_KEYS = Object.keys(RICH_TEXT_CONFIG);

/**
 * Generates an array of messages suitable for fluent's localization provider
 * including all needed strings for rich text.
 * @param {object} content A .content object from an ASR message (i.e. message.content)
 * @returns {FluentBundle[]} A array containing the fluent message context
 */
export function generateBundles(content) {
  const bundle = new FluentBundle("en-US");

  RICH_TEXT_KEYS.forEach(key => {
    const attrs = RICH_TEXT_CONFIG[key];
    const attrsToTry = Array.isArray(attrs) ? [...attrs] : [attrs];
    let string = "";
    while (!string && attrsToTry.length) {
      const attr = attrsToTry.pop();
      string = content[attr];
    }
    bundle.addResource(new FluentResource(`${key} = ${string}`));
  });
  return [bundle];
}
