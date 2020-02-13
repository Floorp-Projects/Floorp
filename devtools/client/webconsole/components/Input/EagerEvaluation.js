/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const {
  getTerminalEagerResult,
} = require("devtools/client/webconsole/selectors/history");

loader.lazyGetter(this, "REPS", function() {
  return require("devtools/client/shared/components/reps/reps").REPS;
});
loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/reps").MODE;
});
loader.lazyRequireGetter(
  this,
  "PropTypes",
  "devtools/client/shared/vendor/react-prop-types"
);

/**
 * Show the results of evaluating the current terminal text, if possible.
 */
class EagerEvaluation extends Component {
  static get propTypes() {
    return {
      terminalEagerResult: PropTypes.any,
    };
  }

  renderRepsResult() {
    const { terminalEagerResult } = this.props;

    const result = terminalEagerResult.getGrip
      ? terminalEagerResult.getGrip()
      : terminalEagerResult;
    const isError = result && result.class && result.class === "Error";

    return REPS.Rep({
      key: "rep",
      object: result,
      mode: isError ? MODE.SHORT : MODE.LONG,
    });
  }

  render() {
    const hasResult = this.props.terminalEagerResult !== null;

    return dom.span(
      {
        className: "devtools-monospace eager-evaluation-result",
        key: "eager-evaluation-result",
      },
      hasResult ? dom.span({ className: "icon", key: "icon" }) : null,
      hasResult ? this.renderRepsResult() : null
    );
  }
}

function mapStateToProps(state) {
  return {
    terminalEagerResult: getTerminalEagerResult(state),
  };
}

module.exports = connect(mapStateToProps)(EagerEvaluation);
