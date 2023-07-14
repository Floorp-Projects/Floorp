/**
 *  https://github.com/ryanjduffy/parse-script-tags
 *
 * Copyright (c) by Ryan Duff
 * Licensed under the MIT License.
 */

const alphanum = /[a-z0-9\-]/i;

function parseToken(str, start) {
  let i = start;
  while (i < str.length && alphanum.test(str.charAt(i++))) {
    continue;
  }

  if (i !== start) {
    return {
      token: str.substring(start, i - 1),
      index: i,
    };
  }

  return null;
}

function parseAttributes(str, start) {
  let i = start;
  const attributes = {};
  let attribute = null;

  while (i < str.length) {
    const c = str.charAt(i);

    if (attribute === null && c == ">") {
      break;
    } else if (attribute === null && alphanum.test(c)) {
      attribute = {
        name: null,
        value: true,
        bool: true,
        terminator: null,
      };

      const attributeNameNode = parseToken(str, i);
      if (attributeNameNode) {
        attribute.name = attributeNameNode.token;
        i = attributeNameNode.index - 2;
      }
    } else if (attribute !== null) {
      if (c === "=") {
        // once we've started an attribute, look for = to indicate
        // it's a non-boolean attribute
        attribute.bool = false;
        if (attribute.value === true) {
          attribute.value = "";
        }
      } else if (
        !attribute.bool &&
        attribute.terminator === null &&
        (c === '"' || c === "'")
      ) {
        // once we've determined it's non-boolean, look for a
        // value terminator (", ')
        attribute.terminator = c;
      } else if (attribute.terminator) {
        if (c === attribute.terminator) {
          // if we had a terminator and found another, we've
          // reach the end of the attribute
          attributes[attribute.name] = attribute.value;
          attribute = null;
        } else {
          // otherwise, append the character to the attribute value
          attribute.value += c;

          // check for an escaped terminator and push it as well
          // to avoid terminating prematurely
          if (c === "\\") {
            const next = str.charAt(i + 1);
            if (next === attribute.terminator) {
              attribute.value += next;
              i += 1;
            }
          }
        }
      } else if (!/\s/.test(c)) {
        // if we've hit a non-space character and aren't processing a value,
        // we're starting a new attribute so push the attribute and clear the
        // local variable
        attributes[attribute.name] = attribute.value;
        attribute = null;

        // move the cursor back to re-find the start of the attribute
        i -= 1;
      }
    }

    i++;
  }

  if (i !== start) {
    return {
      attributes,
      index: i,
    };
  }

  return null;
}

function parseFragment(str, start = 0) {
  let tag = null;
  let open = false;
  let attributes = {};

  let i = start;
  while (i < str.length) {
    const c = str.charAt(i++);

    if (!open && !tag && c === "<") {
      // Open Start Tag
      open = true;

      const tagNode = parseToken(str, i);
      if (!tagNode) {
        return null;
      }

      i = tagNode.index - 1;
      tag = tagNode.token;
    } else if (open && c === ">") {
      // Close Start Tag
      break;
    } else if (open) {
      // Attributes
      const attributeNode = parseAttributes(str, i - 1);

      if (attributeNode) {
        i = attributeNode.index;
        attributes = attributeNode.attributes || attributes;
      }
    }
  }

  if (tag) {
    return {
      tag,
      attributes,
    };
  }

  return null;
}

export default parseFragment;
export { parseFragment };
