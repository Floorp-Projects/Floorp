/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { workerHandler } from "../../../../shared/worker-utils";
import { prettyFast } from "./pretty-fast";

var { SourceMapGenerator } = require("source-map");

const sourceMapGeneratorByTaskId = new Map();

function prettyPrint({ url, indent, sourceText }) {
  const { code, map: sourceMapGenerator } = prettyFast(sourceText, {
    url,
    indent,
  });

  return {
    code,
    sourceMap: sourceMapGenerator.toJSON(),
  };
}

function prettyPrintInlineScript({
  taskId,
  url,
  indent,
  sourceText,
  originalStartLine,
  originalStartColumn,
  generatedStartLine,
}) {
  let taskSourceMapGenerator;
  if (!sourceMapGeneratorByTaskId.has(taskId)) {
    taskSourceMapGenerator = new SourceMapGenerator({ file: url });
    sourceMapGeneratorByTaskId.set(taskId, taskSourceMapGenerator);
  } else {
    taskSourceMapGenerator = sourceMapGeneratorByTaskId.get(taskId);
  }

  const { code } = prettyFast(sourceText, {
    url,
    indent,
    sourceMapGenerator: taskSourceMapGenerator,
    /*
     * By default prettyPrint will trim the text, and we'd have the pretty text displayed
     * just after the script tag, e.g.:
     *
     * ```
     * <script>if (true) {
     *   something()
     * }
     * </script>
     * ```
     *
     * We want the text to start on a new line, so prepend a line break, so we get
     * something like:
     *
     * ```
     * <script>
     * if (true) {
     *   something()
     * }
     * </script>
     * ```
     */
    prefixWithNewLine: true,
    originalStartLine,
    originalStartColumn,
    generatedStartLine,
  });

  // When a taskId was passed, we only return the pretty printed text.
  // The source map should be retrieved with getSourceMapForTask.
  return code;
}

/**
 * Get the source map for a pretty-print task
 *
 * @param {Integer} taskId: The taskId that was used to call prettyPrint
 * @returns {Object} A source map object
 */
function getSourceMapForTask(taskId) {
  if (!sourceMapGeneratorByTaskId.has(taskId)) {
    return null;
  }

  const taskSourceMapGenerator = sourceMapGeneratorByTaskId.get(taskId);
  sourceMapGeneratorByTaskId.delete(taskId);
  return taskSourceMapGenerator.toJSON();
}

self.onmessage = workerHandler({
  prettyPrint,
  prettyPrintInlineScript,
  getSourceMapForTask,
});
