"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _lodash = require("devtools/client/shared/vendor/lodash");

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _selectors = require("../../selectors/index");

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class HighlightLines extends _react.Component {
  constructor() {
    super();

    this.highlightLineRange = () => {
      const {
        highlightedLineRange,
        editor
      } = this.props;
      const {
        codeMirror
      } = editor;

      if ((0, _lodash.isEmpty)(highlightedLineRange) || !codeMirror) {
        return;
      }

      const {
        start,
        end
      } = highlightedLineRange;
      codeMirror.operation(() => {
        editor.alignLine(start);
        (0, _lodash.range)(start - 1, end).forEach(line => {
          codeMirror.addLineClass(line, "line", "highlight-lines");
        });
      });
    };
  }

  componentDidMount() {
    this.highlightLineRange();
  }

  componentWillUpdate() {
    this.clearHighlightRange();
  }

  componentDidUpdate() {
    this.highlightLineRange();
  }

  componentWillUnmount() {
    this.clearHighlightRange();
  }

  clearHighlightRange() {
    const {
      highlightedLineRange,
      editor
    } = this.props;
    const {
      codeMirror
    } = editor;

    if ((0, _lodash.isEmpty)(highlightedLineRange) || !codeMirror) {
      return;
    }

    const {
      start,
      end
    } = highlightedLineRange;
    codeMirror.operation(() => {
      (0, _lodash.range)(start - 1, end).forEach(line => {
        codeMirror.removeLineClass(line, "line", "highlight-lines");
      });
    });
  }

  render() {
    return null;
  }

}

exports.default = (0, _reactRedux.connect)(state => ({
  highlightedLineRange: (0, _selectors.getHighlightedLineRange)(state)
}))(HighlightLines);