/**
 *  https://github.com/ryanjduffy/parse-script-tags
 *
 * Copyright (c) by Ryan Duff
 * Licensed under the MIT License.
 */

import * as types from "@babel/types";

import parseFragment from "./parseScriptFragment.js";

const startScript = /<script[^>]*>/im;
const endScript = /<\/script\s*>/im;
// https://stackoverflow.com/questions/5034781/js-regex-to-split-by-line#comment5633979_5035005
const newLines = /\r\n|[\n\v\f\r\x85\u2028\u2029]/;

function getType(tag) {
  const fragment = parseFragment(tag);

  if (fragment) {
    const type = fragment.attributes.type;

    return type ? type.toLowerCase() : null;
  }

  return null;
}

function getCandidateScriptLocations(source, index) {
  const i = index || 0;
  const str = source.substring(i);

  const startMatch = startScript.exec(str);
  if (startMatch) {
    const startsAt = startMatch.index + startMatch[0].length;
    const afterStart = str.substring(startsAt);
    const endMatch = endScript.exec(afterStart);
    if (endMatch) {
      const locLength = endMatch.index;
      const locIndex = i + startsAt;
      const endIndex = locIndex + locLength + endMatch[0].length;

      // extract the complete tag (incl start and end tags and content). if the
      // type is invalid (= not JS), skip this tag and continue
      const tag = source.substring(i + startMatch.index, endIndex);
      const type = getType(tag);
      if (
        type &&
        type !== "javascript" &&
        type !== "text/javascript" &&
        type !== "module"
      ) {
        return getCandidateScriptLocations(source, endIndex);
      }

      return [
        adjustForLineAndColumn(source, {
          index: locIndex,
          length: locLength,
          source: source.substring(locIndex, locIndex + locLength),
        }),
        ...getCandidateScriptLocations(source, endIndex),
      ];
    }
  }

  return [];
}

function parseScripts(locations, parser) {
  return locations.map(parser);
}

function calcLineAndColumn(source, index) {
  const lines = source.substring(0, index).split(newLines);
  const line = lines.length;
  const column = lines.pop().length + 1;

  return {
    column,
    line,
  };
}

function adjustForLineAndColumn(fullSource, location) {
  const { column, line } = calcLineAndColumn(fullSource, location.index);
  return Object.assign({}, location, {
    line,
    column,
    // prepend whitespace for scripts that do not start on the first column
    // NOTE: `column` is 1-based
    source: " ".repeat(column - 1) + location.source,
  });
}

function parseScriptTags(source, parser) {
  const scripts = parseScripts(getCandidateScriptLocations(source), parser)
    .filter(types.isFile)
    .reduce(
      (main, script) => {
        return {
          statements: main.statements.concat(script.program.body),
          comments: main.comments.concat(script.comments),
          tokens: main.tokens.concat(script.tokens),
        };
      },
      {
        statements: [],
        comments: [],
        tokens: [],
      }
    );

  const program = types.program(scripts.statements);
  const file = types.file(program, scripts.comments, scripts.tokens);

  const end = calcLineAndColumn(source, source.length);
  file.start = program.start = 0;
  file.end = program.end = source.length;
  file.loc = program.loc = {
    start: {
      line: 1,
      column: 0,
    },
    end,
  };

  return file;
}

export default parseScriptTags;
export { getCandidateScriptLocations, parseScripts, parseScriptTags };
