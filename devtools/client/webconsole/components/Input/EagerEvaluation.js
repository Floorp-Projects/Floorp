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

const actions = require("devtools/client/webconsole/actions/index");

loader.lazyGetter(this, "REPS", function() {
  return require("devtools/client/shared/components/reps/index").REPS;
});
loader.lazyGetter(this, "MODE", function() {
  return require("devtools/client/shared/components/reps/index").MODE;
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
      serviceContainer: PropTypes.object.isRequired,
      highlightDomElement: PropTypes.func.isRequired,
      unHighlightDomElement: PropTypes.func.isRequired,
    };
  }

  static getDerivedStateFromError(error) {
    return { hasError: true };
  }

  componentDidUpdate(prevProps) {
    const {
      highlightDomElement,
      unHighlightDomElement,
      terminalEagerResult,
    } = this.props;

    if (canHighlightObject(prevProps.terminalEagerResult)) {
      unHighlightDomElement(prevProps.terminalEagerResult.getGrip());
    }

    if (canHighlightObject(terminalEagerResult)) {
      highlightDomElement(terminalEagerResult.getGrip());
    }

    if (this.state?.hasError) {
      // If the render function threw at some point, clear the error after 1s so the
      // component has a chance to render again.
      // This way, we don't block instant evaluation for the whole session, in case the
      // input changed in the meantime. If the input didn't change, we'll hit
      // getDerivatedStateFromError again (and this won't render anything), so it's safe.
      setTimeout(() => {
        this.setState({ hasError: false });
      }, 1000);
    }
  }

  componentWillUnmount() {
    const { unHighlightDomElement, terminalEagerResult } = this.props;

    if (canHighlightObject(terminalEagerResult)) {
      unHighlightDomElement(terminalEagerResult.getGrip());
    }
  }

  renderRepsResult() {
    const { terminalEagerResult } = this.props;

    const result = terminalEagerResult.getGrip
      ? terminalEagerResult.getGrip()
      : terminalEagerResult;
    const { isError } = result || {};

    return REPS.Rep({
      key: "rep",
      object: result,
      mode: isError ? MODE.SHORT : MODE.LONG,
    });
  }

  render() {
    const hasResult =
      this.props.terminalEagerResult !== null &&
      this.props.terminalEagerResult !== undefined &&
      !this.state?.hasError;

    return dom.div(
      { className: "eager-evaluation-result", key: "eager-evaluation-result" },
      hasResult
        ? dom.span(
            { className: "eager-evaluation-result__row" },
            dom.span({
              className: "eager-evaluation-result__icon",
              key: "icon",
            }),
            dom.span(
              { className: "eager-evaluation-result__text", key: "text" },
              this.renderRepsResult()
            )
          )
        : null
    );
  }
}

function canHighlightObject(obj) {
  const grip = obj?.getGrip && obj.getGrip();
  return (
    grip &&
    (REPS.ElementNode.supportsObject(grip) ||
      REPS.TextNode.supportsObject(grip)) &&
    grip.preview.isConnected
  );
}

function mapStateToProps(state) {
  return {
    terminalEagerResult: getTerminalEagerResult(state),
  };
}

function mapDispatchToProps(dispatch) {
  return {
    highlightDomElement: grip => dispatch(actions.highlightDomElement(grip)),
    unHighlightDomElement: grip =>
      dispatch(actions.unHighlightDomElement(grip)),
  };
}
module.exports = connect(mapStateToProps, mapDispatchToProps)(EagerEvaluation);
