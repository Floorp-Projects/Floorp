/* -*- indent-tabs-mode: nil; js-indent-level: 2; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file is meant to be loaded as a ChromeWorker. It accepts messages which
 * have data of the form:
 *
 *     { id, url, indent, source }
 *
 * Where `id` is a unique ID to identify this request, `url` is the url of the
 * source being pretty printed, `indent` is the number of spaces to indent the
 * code by, and `source` is the source text.
 *
 * On success, the worker responds with a message of the form:
 *
 *     { id, code, mappings }
 *
 * Where `id` is the same unique ID from the request, `code` is the pretty
 * printed source text, and `mappings` is an array or source mappings from the
 * pretty printed code back to the ugly source text.
 *
 * In the case of an error, the worker responds with a message of the form:
 *
 *     { id, error }
 */

importScripts("resource://devtools/shared/worker/helper.js");
importScripts("resource://devtools/shared/acorn/acorn.js");
importScripts("resource://devtools/shared/sourcemap/source-map.js");
importScripts("resource://devtools/shared/pretty-fast/pretty-fast.js");

workerHelper.createTask(self, "pretty-print", ({ url, indent, source }) => {
  try {
    const prettified = prettyFast(source, {
      url: url,
      indent: " ".repeat(indent)
    });

    return {
      code: prettified.code,
      mappings: prettified.map._mappings
    };
  }
  catch (e) {
    return new Error(e.message + "\n" + e.stack);
  }
});
