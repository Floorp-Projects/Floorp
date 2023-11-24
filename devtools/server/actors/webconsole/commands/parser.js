/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

loader.lazyRequireGetter(
  this,
  ["WebConsoleCommandsManager"],
  "resource://devtools/server/actors/webconsole/commands/manager.js",
  true
);

const COMMAND = "command";
const KEY = "key";
const ARG = "arg";

const COMMAND_PREFIX = /^:/;
const KEY_PREFIX = /^--/;

// default value for flags
const DEFAULT_VALUE = true;
const COMMAND_DEFAULT_FLAG = {
  block: "url",
  screenshot: "filename",
  unblock: "url",
};

/**
 * When given a string that begins with `:` and a unix style string,
 * returns the command name and the arguments.
 * Throws if the command doesn't exist.
 * This is intended to be used by the WebConsole actor only.
 *
 * @param String string
 *        A string to format that begins with `:`.
 *
 * @returns Object The command name and the arguments
 *                 { command: String, args: Object }
 */
function getCommandAndArgs(string) {
  if (!isCommand(string)) {
    throw Error("getCommandAndArgs was called without `:`");
  }
  string = string.trim();
  if (string === ":") {
    throw Error("Missing a command name after ':'");
  }
  const tokens = string.split(/\s+/).map(createToken);
  return parseCommand(tokens);
}

/**
 * creates a token object depending on a string which as a prefix,
 * either `:` for a command or `--` for a key, or nothing for an argument
 *
 * @param String string
 *               A string to use as the basis for the token
 *
 * @returns Object Token Object, with the following shape
 *                { type: String, value: String }
 */
function createToken(string) {
  if (isCommand(string)) {
    const value = string.replace(COMMAND_PREFIX, "");
    if (!value) {
      throw Error("Missing a command name after ':'");
    }
    if (!WebConsoleCommandsManager.getAllColonCommandNames().includes(value)) {
      throw Error(`'${value}' is not a valid command`);
    }
    return { type: COMMAND, value };
  }
  if (isKey(string)) {
    const value = string.replace(KEY_PREFIX, "");
    if (!value) {
      throw Error("invalid flag");
    }
    return { type: KEY, value };
  }
  return { type: ARG, value: string };
}

/**
 * returns a command Tree object for a set of tokens
 *
 *
 * @param Array Tokens tokens
 *                     An array of Token objects
 *
 * @returns Object Tree Object, with the following shape
 *                 { command: String, args: Object }
 */
function parseCommand(tokens) {
  let command = null;
  const args = {};

  for (let i = 0; i < tokens.length; i++) {
    const token = tokens[i];
    if (token.type === COMMAND) {
      if (command) {
        // we are throwing here because two commands have been passed and it is unclear
        // what the user's intention was
        throw Error(
          "Executing multiple commands in one evaluation is not supported"
        );
      }
      command = token.value;
    }

    if (token.type === KEY) {
      const nextTokenIndex = i + 1;
      const nextToken = tokens[nextTokenIndex];
      let values = args[token.value] || DEFAULT_VALUE;
      if (nextToken && nextToken.type === ARG) {
        const { value, offset } = collectString(
          nextToken,
          tokens,
          nextTokenIndex
        );
        // in order for JSON.stringify to correctly output values, they must be correctly
        // typed
        // As per the old GCLI documentation, we can only have one value associated with a
        // flag but multiple flags with the same name can exist and should be combined
        // into and array.  Here we are associating only the value on the right hand
        // side if it is of type `arg` as a single value; the second case initializes
        // an array, and the final case pushes a value to an existing array
        const typedValue = getTypedValue(value);
        if (values === DEFAULT_VALUE) {
          values = typedValue;
        } else if (!Array.isArray(values)) {
          values = [values, typedValue];
        } else {
          values.push(typedValue);
        }
        // skip the next token since we have already consumed it
        i = nextTokenIndex + offset;
      }
      args[token.value] = values;
    }

    // Since this has only been implemented for screenshot, we can only have one default
    // value. Eventually we may have more default values. For now, ignore multiple
    // unflagged args
    const defaultFlag = COMMAND_DEFAULT_FLAG[command];
    if (token.type === ARG && !args[defaultFlag]) {
      const { value, offset } = collectString(token, tokens, i);
      // Throw if the command isn't registered in COMMAND_DEFAULT_FLAG
      // as this command may not expect any argument without an explicit argument name like "-name arg"
      if (!defaultFlag) {
        throw new Error(
          `:${command} command doesn't support unnamed '${value}' argument.`
        );
      }
      args[defaultFlag] = getTypedValue(value);
      i = i + offset;
    }
  }
  return { command, args };
}

const stringChars = ['"', "'", "`"];
function isStringChar(testChar) {
  return stringChars.includes(testChar);
}

function checkLastChar(string, testChar) {
  const lastChar = string[string.length - 1];
  return lastChar === testChar;
}

function hasUnescapedChar(value, char, rightOffset, leftOffset) {
  const lastPos = value.length - 1;
  const string = value.slice(rightOffset, lastPos - leftOffset);
  const index = string.indexOf(char);
  if (index === -1) {
    return false;
  }
  const prevChar = index > 0 ? string[index - 1] : null;
  // return false if the unexpected character is escaped, true if it is not
  return prevChar !== "\\";
}

function collectString(token, tokens, index) {
  const firstChar = token.value[0];
  const isString = isStringChar(firstChar);
  const UNESCAPED_CHAR_ERROR = segment =>
    `String has unescaped \`${firstChar}\` in [${segment}...],` +
    " may miss a space between arguments";
  let value = token.value;

  // the test value is not a string, or it is a string but a complete one
  // i.e. `"test"`, as opposed to `"foo`. In either case, this we can return early
  if (!isString || checkLastChar(value, firstChar)) {
    return { value, offset: 0 };
  }

  if (hasUnescapedChar(value, firstChar, 1, 0)) {
    throw Error(UNESCAPED_CHAR_ERROR(value));
  }

  let offset = null;
  for (let i = index + 1; i <= tokens.length; i++) {
    if (i === tokens.length) {
      throw Error("String does not terminate");
    }

    const nextToken = tokens[i];
    if (nextToken.type !== ARG) {
      throw Error(`String does not terminate before flag "${nextToken.value}"`);
    }

    value = `${value} ${nextToken.value}`;

    if (hasUnescapedChar(nextToken.value, firstChar, 0, 1)) {
      throw Error(UNESCAPED_CHAR_ERROR(value));
    }

    if (checkLastChar(nextToken.value, firstChar)) {
      offset = i - index;
      break;
    }
  }
  return { value, offset };
}

function isCommand(string) {
  return COMMAND_PREFIX.test(string);
}

function isKey(string) {
  return KEY_PREFIX.test(string);
}

function getTypedValue(value) {
  if (!isNaN(value)) {
    return Number(value);
  }
  if (value === "true" || value === "false") {
    return Boolean(value);
  }
  if (isStringChar(value[0])) {
    return value.slice(1, value.length - 1);
  }
  return value;
}

exports.getCommandAndArgs = getCommandAndArgs;
exports.isCommand = isCommand;
