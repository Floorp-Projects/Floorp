(function (factory) {
    typeof define === 'function' && define.amd ? define(factory) :
    factory();
})((function () { 'use strict';

    (function() {
        const env = {"NODE_ENV":"production"};
        try {
            if (process) {
                process.env = Object.assign({}, process.env);
                Object.assign(process.env, env);
                return;
            }
        } catch (e) {} // avoid ReferenceError: process is not defined
        globalThis.process = { env:env };
    })();

    /* This Source Code Form is subject to the terms of the Mozilla Public
     * License, v. 2.0. If a copy of the MPL was not distributed with this
     * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

    function isNode() {
      try {
        return process.release.name == "node";
      } catch (e) {
        return false;
      }
    }

    function isNodeTest() {
      return isNode() && process.env.NODE_ENV != "production";
    }

    let assert;
    // TODO: try to enable these assertions on mochitest by also enabling it on:
    //   import flags from "devtools/shared/flags";
    //   if (flags.testing)
    // Unfortunately it throws a lot on mochitests...

    if (isNodeTest()) {
      assert = function (condition, message) {
        if (!condition) {
          throw new Error(`Assertion failure: ${message}`);
        }
      };
    } else {
      assert = function () {};
    }
    var assert$1 = assert;

    /* This Source Code Form is subject to the terms of the Mozilla Public
     * License, v. 2.0. If a copy of the MPL was not distributed with this
     * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

    function escapeRegExp(str) {
      const reRegExpChar = /[\\^$.*+?()[\]{}|]/g;
      return str.replace(reRegExpChar, "\\$&");
    }

    /**
     * Ignore doing outline matches for less than 3 whitespaces
     *
     * @memberof utils/source-search
     * @static
     */
    function ignoreWhiteSpace(str) {
      return /^\s{0,2}$/.test(str) ? "(?!\\s*.*)" : str;
    }

    function wholeMatch(query, wholeWord) {
      if (query === "" || !wholeWord) {
        return query;
      }

      return `\\b${query}\\b`;
    }

    function buildFlags(caseSensitive, isGlobal) {
      if (caseSensitive && isGlobal) {
        return "g";
      }

      if (!caseSensitive && isGlobal) {
        return "gi";
      }

      if (!caseSensitive && !isGlobal) {
        return "i";
      }

      return null;
    }

    function buildQuery(
      originalQuery,
      modifiers,
      { isGlobal = false, ignoreSpaces = false }
    ) {
      const { caseSensitive, regexMatch, wholeWord } = modifiers;

      if (originalQuery === "") {
        return new RegExp(originalQuery);
      }

      // Remove the backslashes at the end of the query as it
      // breaks the RegExp
      let query = originalQuery.replace(/\\$/, "");

      // If we don't want to do a regexMatch, we need to escape all regex related characters
      // so they would actually match.
      if (!regexMatch) {
        query = escapeRegExp(query);
      }

      // ignoreWhiteSpace might return a negative lookbehind, and in such case, we want it
      // to be consumed as a RegExp part by the callsite, so this needs to be called after
      // the regexp is escaped.
      if (ignoreSpaces) {
        query = ignoreWhiteSpace(query);
      }

      query = wholeMatch(query, wholeWord);
      const flags = buildFlags(caseSensitive, isGlobal);

      if (flags) {
        return new RegExp(query, flags);
      }

      return new RegExp(query);
    }

    function getMatches(query, text, options) {
      if (!query || !text || !options) {
        return [];
      }
      const regexQuery = buildQuery(query, options, {
        isGlobal: true,
      });
      const matchedLocations = [];
      const lines = text.split("\n");
      for (let i = 0; i < lines.length; i++) {
        let singleMatch;
        const line = lines[i];
        while ((singleMatch = regexQuery.exec(line)) !== null) {
          // Flow doesn't understand the test above.
          if (!singleMatch) {
            throw new Error("no singleMatch");
          }

          matchedLocations.push({
            line: i,
            ch: singleMatch.index,
            match: singleMatch[0],
          });

          // When the match is an empty string the regexQuery.lastIndex will not
          // change resulting in an infinite loop so we need to check for this and
          // increment it manually in that case.  See issue #7023
          if (singleMatch[0] === "") {
            assert$1(
              !regexQuery.unicode,
              "lastIndex++ can cause issues in unicode mode"
            );
            regexQuery.lastIndex++;
          }
        }
      }
      return matchedLocations;
    }

    function findSourceMatches(content, queryText, options) {
      if (queryText == "") {
        return [];
      }

      const text = content.value;
      const lines = text.split("\n");

      return getMatches(queryText, text, options).map(({ line, ch, match }) => {
        const { value, matchIndex } = truncateLine(lines[line], ch);
        return {
          line: line + 1,
          column: ch,

          matchIndex,
          match,
          value,
        };
      });
    }

    // This is used to find start of a word, so that cropped string look nice
    const startRegex = /([ !@#$%^&*()_+\-=\[\]{};':"\\|,.<>\/?])/g;
    // Similarly, find
    const endRegex = new RegExp(
      [
        "([ !@#$%^&*()_+-=[]{};':\"\\|,.<>/?])",
        '[^ !@#$%^&*()_+-=[]{};\':"\\|,.<>/?]*$"/',
      ].join("")
    );
    // For texts over 100 characters this truncates the text (for display)
    // around the context of the matched text.
    function truncateLine(text, column) {
      if (text.length < 100) {
        return {
          matchIndex: column,
          value: text,
        };
      }

      // Initially take 40 chars left to the match
      const offset = Math.max(column - 40, 0);
      // 400 characters should be enough to figure out the context of the match
      const truncStr = text.slice(offset, column + 400);
      let start = truncStr.search(startRegex);
      let end = truncStr.search(endRegex);

      if (start > column) {
        // No word separator found before the match, so we take all characters
        // before the match
        start = -1;
      }
      if (end < column) {
        end = truncStr.length;
      }
      const value = truncStr.slice(start + 1, end);

      return {
        matchIndex: column - start - offset - 1,
        value,
      };
    }

    var workerUtils = {exports: {}};

    var hasRequiredWorkerUtils;

    function requireWorkerUtils () {
    	if (hasRequiredWorkerUtils) return workerUtils.exports;
    	hasRequiredWorkerUtils = 1;
    	(function (module) {

    		class WorkerDispatcher {
    		  #msgId = 1;
    		  #worker = null;
    		  // Map of message ids -> promise resolution functions, for dispatching worker responses
    		  #pendingCalls = new Map();
    		  #url = "";

    		  constructor(url) {
    		    this.#url = url;
    		  }

    		  start() {
    		    // When running in debugger jest test, we don't have access to ChromeWorker
    		    if (typeof ChromeWorker == "function") {
    		      this.#worker = new ChromeWorker(this.#url);
    		    } else {
    		      this.#worker = new Worker(this.#url);
    		    }
    		    this.#worker.onerror = err => {
    		      console.error(`Error in worker ${this.#url}`, err.message);
    		    };
    		    this.#worker.addEventListener("message", this.#onMessage);
    		  }

    		  stop() {
    		    if (!this.#worker) {
    		      return;
    		    }

    		    this.#worker.removeEventListener("message", this.#onMessage);
    		    this.#worker.terminate();
    		    this.#worker = null;
    		    this.#pendingCalls.clear();
    		  }

    		  task(method, { queue = false } = {}) {
    		    const calls = [];
    		    const push = args => {
    		      return new Promise((resolve, reject) => {
    		        if (queue && calls.length === 0) {
    		          Promise.resolve().then(flush);
    		        }

    		        calls.push({ args, resolve, reject });

    		        if (!queue) {
    		          flush();
    		        }
    		      });
    		    };

    		    const flush = () => {
    		      const items = calls.slice();
    		      calls.length = 0;

    		      if (!this.#worker) {
    		        this.start();
    		      }

    		      const id = this.#msgId++;
    		      this.#worker.postMessage({
    		        id,
    		        method,
    		        calls: items.map(item => item.args),
    		      });

    		      this.#pendingCalls.set(id, items);
    		    };

    		    return (...args) => push(args);
    		  }

    		  invoke(method, ...args) {
    		    return this.task(method)(...args);
    		  }

    		  #onMessage = ({ data: result }) => {
    		    const items = this.#pendingCalls.get(result.id);
    		    this.#pendingCalls.delete(result.id);
    		    if (!items) {
    		      return;
    		    }

    		    if (!this.#worker) {
    		      return;
    		    }

    		    result.results.forEach((resultData, i) => {
    		      const { resolve, reject } = items[i];

    		      if (resultData.error) {
    		        const err = new Error(resultData.message);
    		        err.metadata = resultData.metadata;
    		        reject(err);
    		      } else {
    		        resolve(resultData.response);
    		      }
    		    });
    		  };
    		}

    		function workerHandler(publicInterface) {
    		  return function (msg) {
    		    const { id, method, calls } = msg.data;

    		    Promise.all(
    		      calls.map(args => {
    		        try {
    		          const response = publicInterface[method].apply(undefined, args);
    		          if (response instanceof Promise) {
    		            return response.then(
    		              val => ({ response: val }),
    		              err => asErrorMessage(err)
    		            );
    		          }
    		          return { response };
    		        } catch (error) {
    		          return asErrorMessage(error);
    		        }
    		      })
    		    ).then(results => {
    		      globalThis.postMessage({ id, results });
    		    });
    		  };
    		}

    		function asErrorMessage(error) {
    		  if (typeof error === "object" && error && "message" in error) {
    		    // Error can't be sent via postMessage, so be sure to convert to
    		    // string.
    		    return {
    		      error: true,
    		      message: error.message,
    		      metadata: error.metadata,
    		    };
    		  }

    		  return {
    		    error: true,
    		    message: error == null ? error : error.toString(),
    		    metadata: undefined,
    		  };
    		}

    		// Might be loaded within a worker thread where `module` isn't available.
    		{
    		  module.exports = {
    		    WorkerDispatcher,
    		    workerHandler,
    		  };
    		} 
    	} (workerUtils));
    	return workerUtils.exports;
    }

    var workerUtilsExports = requireWorkerUtils();

    self.onmessage = workerUtilsExports.workerHandler({ getMatches, findSourceMatches });

}));
