"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.DebugLine = undefined;

var _react = require("devtools/client/shared/vendor/react");

var _editor = require("../../utils/editor/index");

var _source = require("../../utils/source");

var _pause = require("../../utils/pause/index");

var _indentation = require("../../utils/indentation");

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _selectors = require("../../selectors/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
function isDocumentReady(selectedSource, selectedFrame) {
  return selectedFrame && (0, _source.isLoaded)(selectedSource) && (0, _editor.hasDocument)(selectedFrame.location.sourceId);
}

class DebugLine extends _react.Component {
  componentDidUpdate(prevProps) {
    const {
      why,
      selectedFrame,
      selectedSource
    } = this.props;
    this.setDebugLine(why, selectedFrame, selectedSource);
  }

  componentWillUpdate() {
    const {
      why,
      selectedFrame,
      selectedSource
    } = this.props;
    this.clearDebugLine(selectedFrame, selectedSource, why);
  }

  componentDidMount() {
    const {
      why,
      selectedFrame,
      selectedSource
    } = this.props;
    this.setDebugLine(why, selectedFrame, selectedSource);
  }

  setDebugLine(why, selectedFrame, selectedSource) {
    if (!isDocumentReady(selectedSource, selectedFrame)) {
      return;
    }

    const sourceId = selectedFrame.location.sourceId;
    const doc = (0, _editor.getDocument)(sourceId);
    let {
      line,
      column
    } = (0, _editor.toEditorPosition)(selectedFrame.location);
    const {
      markTextClass,
      lineClass
    } = this.getTextClasses(why);
    doc.addLineClass(line, "line", lineClass);
    const lineText = doc.getLine(line);
    column = Math.max(column, (0, _indentation.getIndentation)(lineText));
    this.debugExpression = doc.markText({
      ch: column,
      line
    }, {
      ch: null,
      line
    }, {
      className: markTextClass
    });
  }

  clearDebugLine(selectedFrame, selectedSource, why) {
    if (!isDocumentReady(selectedSource, selectedFrame)) {
      return;
    }

    if (this.debugExpression) {
      this.debugExpression.clear();
    }

    const sourceId = selectedFrame.location.sourceId;
    const {
      line
    } = (0, _editor.toEditorPosition)(selectedFrame.location);
    const doc = (0, _editor.getDocument)(sourceId);
    const {
      lineClass
    } = this.getTextClasses(why);
    doc.removeLineClass(line, "line", lineClass);
  }

  getTextClasses(why) {
    if ((0, _pause.isException)(why)) {
      return {
        markTextClass: "debug-expression-error",
        lineClass: "new-debug-line-error"
      };
    }

    return {
      markTextClass: "debug-expression",
      lineClass: "new-debug-line"
    };
  }

  render() {
    return null;
  }

}

exports.DebugLine = DebugLine;

const mapStateToProps = state => ({
  selectedFrame: (0, _selectors.getVisibleSelectedFrame)(state),
  selectedSource: (0, _selectors.getSelectedSource)(state),
  why: (0, _selectors.getPauseReason)(state)
});

exports.default = (0, _reactRedux.connect)(mapStateToProps)(DebugLine);