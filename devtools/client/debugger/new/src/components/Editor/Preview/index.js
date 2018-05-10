"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _Popup = require("./Popup");

var _Popup2 = _interopRequireDefault(_Popup);

var _selectors = require("../../../selectors/index");

var _actions = require("../../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _editor = require("../../../utils/editor/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class Preview extends _react.PureComponent {
  constructor(props) {
    super(props);

    this.onMouseOver = e => {
      const {
        target
      } = e;
      this.props.updatePreview(target, this.props.editor);
    };

    this.onMouseUp = () => {
      this.setState({
        selecting: false
      });
      return true;
    };

    this.onMouseDown = () => {
      this.setState({
        selecting: true
      });
      return true;
    };

    this.onMouseLeave = e => {
      const target = e.target;

      if (target.classList.contains("CodeMirror")) {
        return;
      }

      this.props.clearPreview();
    };

    this.onClose = () => {
      this.props.clearPreview();
    };

    this.state = {
      selecting: false
    };
  }

  componentDidMount() {
    const {
      codeMirror
    } = this.props.editor;
    const codeMirrorWrapper = codeMirror.getWrapperElement();
    codeMirrorWrapper.addEventListener("mouseover", this.onMouseOver);
    codeMirrorWrapper.addEventListener("mouseup", this.onMouseUp);
    codeMirrorWrapper.addEventListener("mousedown", this.onMouseDown);
    codeMirrorWrapper.addEventListener("mouseleave", this.onMouseLeave);

    if (document.body) {
      document.body.addEventListener("mouseleave", this.onMouseLeave);
    }
  }

  componentWillUnmount() {
    const codeMirror = this.props.editor.codeMirror;
    const codeMirrorWrapper = codeMirror.getWrapperElement();
    codeMirrorWrapper.removeEventListener("mouseover", this.onMouseOver);
    codeMirrorWrapper.removeEventListener("mouseup", this.onMouseUp);
    codeMirrorWrapper.removeEventListener("mousedown", this.onMouseDown);
    codeMirrorWrapper.removeEventListener("mouseleave", this.onMouseLeave);

    if (document.body) {
      document.body.removeEventListener("mouseleave", this.onMouseLeave);
    }
  }

  render() {
    const {
      selectedSource,
      preview
    } = this.props;

    if (!this.props.editor || !selectedSource || this.state.selecting) {
      return null;
    }

    if (!preview || preview.updating) {
      return null;
    }

    const {
      result,
      expression,
      location,
      cursorPos,
      extra
    } = preview;
    const value = result;

    if (typeof value == "undefined" || value.optimizedOut) {
      return null;
    }

    const editorRange = (0, _editor.toEditorRange)(selectedSource.get("id"), location);
    return _react2.default.createElement(_Popup2.default, {
      value: value,
      editor: this.props.editor,
      editorRef: this.props.editorRef,
      range: editorRange,
      expression: expression,
      popoverPos: cursorPos,
      extra: extra,
      onClose: this.onClose
    });
  }

}

const mapStateToProps = state => ({
  preview: (0, _selectors.getPreview)(state),
  selectedSource: (0, _selectors.getSelectedSource)(state)
});

const {
  addExpression,
  setPopupObjectProperties,
  updatePreview,
  clearPreview
} = _actions2.default;
const mapDispatchToProps = {
  addExpression,
  setPopupObjectProperties,
  updatePreview,
  clearPreview
};
exports.default = (0, _reactRedux.connect)(mapStateToProps, mapDispatchToProps)(Preview);