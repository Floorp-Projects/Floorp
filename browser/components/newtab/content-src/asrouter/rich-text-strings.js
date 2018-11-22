import {MessageContext} from "fluent";

/**
 * Properties that allow rich text MUST be added to this list.
 *   key: the localization_id that should be used
 *   value: a property or array of properties on the message.content object
 */
const RICH_TEXT_CONFIG = {
  "text": ["text", "scene1_text"],
  "scene2_text": "scene2_text",
  "privacy_html": "scene2_privacy_html",
  "disclaimer_html": "scene2_disclaimer_html",
};

export const RICH_TEXT_KEYS = Object.keys(RICH_TEXT_CONFIG);

/**
 * Generates an array of messages suitable for fluent's localization provider
 * including all needed strings for rich text.
 * @param {object} content A .content object from an ASR message (i.e. message.content)
 * @returns {MessageContext[]} A array containing the fluent message context
 */
export function generateMessages(content) {
  const cx = new MessageContext("en-US");

  RICH_TEXT_KEYS.forEach(key => {
    const attrs = RICH_TEXT_CONFIG[key];
    const attrsToTry = Array.isArray(attrs) ? [...attrs] : [attrs];
    let string = "";
    while (!string && attrsToTry.length) {
      const attr = attrsToTry.pop();
      string = content[attr];
    }
    cx.addMessages(`${key} = ${string}`);
  });
  return [cx];
}
