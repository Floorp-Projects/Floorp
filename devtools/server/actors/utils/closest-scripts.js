/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { findSourceOffset } = require("devtools/server/actors/utils/dbg-source");

function findClosestScriptBySource(scripts, generatedLine, generatedColumn) {
  const bySource = new Map();
  for (const script of scripts) {
    const { source } = script;
    if (script.format !== "js" || !source) {
      continue;
    }

    let sourceScripts = bySource.get(source);
    if (!sourceScripts) {
      sourceScripts = [];
      bySource.set(source, sourceScripts);
    }

    sourceScripts.push(script);
  }

  const closestScripts = [];
  for (const sourceScripts of bySource.values()) {
    const closest = findClosestScript(sourceScripts, generatedLine, generatedColumn);
    if (closest) {
      closestScripts.push(closest);
    }
  }
  return closestScripts;
}
exports.findClosestScriptBySource = findClosestScriptBySource;

function findClosestScript(scripts, generatedLine, generatedColumn) {
  if (scripts.length === 0) {
    return null;
  }
  const { source } = scripts[0];

  const offset = findSourceOffset(
    source,
    generatedLine,
    generatedColumn,
  );

  let closest = null;
  for (const script of scripts) {
    if (script.source !== source) {
      throw new Error("All scripts must be from a single source.");
    }

    if (
      offset >= script.sourceStart &&
      offset < script.sourceStart + script.sourceLength &&
      (!closest || script.sourceLength < closest.sourceLength)
    ) {
      closest = script;
    }
  }

  return closest;
}
