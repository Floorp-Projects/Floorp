/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const React = require("devtools/client/shared/vendor/react");
const { DOM: dom, createClass, createFactory, PropTypes } = React;
const { LocalizationHelper } = require("devtools/shared/l10n");
const Frame = createFactory(require("./frame"));

const l10n = new LocalizationHelper("devtools/client/locales/webconsole.properties");

const AsyncFrame = createFactory(createClass({
  displayName: "AsyncFrame",

  propTypes: {
    asyncCause: PropTypes.string.isRequired
  },

  render() {
    let { asyncCause } = this.props;

    return dom.span(
      { className: "frame-link-async-cause" },
      l10n.getFormatStr("stacktrace.asyncStack", asyncCause)
    );
  }
}));

const StackTrace = createClass({
  displayName: "StackTrace",

  propTypes: {
    stacktrace: PropTypes.array.isRequired,
    onViewSourceInDebugger: PropTypes.func.isRequired,
    onViewSourceInScratchpad: PropTypes.func,
    // Service to enable the source map feature.
    sourceMapService: PropTypes.object,
  },

  render() {
    let {
      stacktrace,
      onViewSourceInDebugger,
      onViewSourceInScratchpad,
      sourceMapService,
    } = this.props;

    let frames = [];
    stacktrace.forEach((s, i) => {
      if (s.asyncCause) {
        frames.push("\t", AsyncFrame({
          key: `${i}-asyncframe`,
          asyncCause: s.asyncCause
        }), "\n");
      }

      let source = s.filename.split(" -> ").pop();
      frames.push("\t", Frame({
        key: `${i}-frame`,
        frame: {
          functionDisplayName: s.functionName,
          source,
          line: s.lineNumber,
          column: s.columnNumber,
        },
        showFunctionName: true,
        showAnonymousFunctionName: true,
        showFullSourceUrl: true,
        onClick: (/^Scratchpad\/\d+$/.test(source))
          ? onViewSourceInScratchpad
          : onViewSourceInDebugger,
        sourceMapService,
      }), "\n");
    });

    return dom.div({ className: "stack-trace" }, frames);
  }
});

module.exports = StackTrace;
