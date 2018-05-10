"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.ConditionalPanel = undefined;

var _react = require("devtools/client/shared/vendor/react");

var _react2 = _interopRequireDefault(_react);

var _reactDom = require("devtools/client/shared/vendor/react-dom");

var _reactDom2 = _interopRequireDefault(_reactDom);

var _reactRedux = require("devtools/client/shared/vendor/react-redux");

var _editor = require("../../utils/editor/index");

var _actions = require("../../actions/index");

var _actions2 = _interopRequireDefault(_actions);

var _selectors = require("../../selectors/index");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */
class ConditionalPanel extends _react.PureComponent {
  constructor() {
    super();

    this.saveAndClose = () => {
      if (this.input) {
        this.setBreakpoint(this.input.value);
      }

      this.props.closeConditionalPanel();
    };

    this.onKey = e => {
      if (e.key === "Enter") {
        this.saveAndClose();
      } else if (e.key === "Escape") {
        this.props.closeConditionalPanel();
      }
    };

    this.repositionOnScroll = () => {
      if (this.panelNode && this.scrollParent) {
        const {
          scrollLeft
        } = this.scrollParent;
        this.panelNode.style.transform = `translateX(${scrollLeft}px)`;
      }
    };

    this.cbPanel = null;
  }

  keepFocusOnInput() {
    if (this.input) {
      this.input.focus();
    }
  }

  setBreakpoint(condition) {
    const {
      selectedLocation,
      line
    } = this.props;
    const sourceId = selectedLocation ? selectedLocation.sourceId : "";
    const location = {
      sourceId,
      line
    };
    return this.props.setBreakpointCondition(location, {
      condition
    });
  }

  clearConditionalPanel() {
    if (this.cbPanel) {
      this.cbPanel.clear();
      this.cbPanel = null;
    }

    if (this.scrollParent) {
      this.scrollParent.removeEventListener("scroll", this.repositionOnScroll);
    }
  }

  componentWillMount() {
    if (this.props.line) {
      return this.renderToWidget(this.props);
    }
  }

  componentWillUpdate(nextProps) {
    if (nextProps.line) {
      return this.renderToWidget(nextProps);
    }

    return this.clearConditionalPanel();
  }

  componentWillUnmount() {
    // This is called if CodeMirror is re-initializing itself before the
    // user closes the conditional panel. Clear the widget, and re-render it
    // as soon as this component gets remounted
    return this.clearConditionalPanel();
  }

  renderToWidget(props) {
    if (this.cbPanel) {
      if (this.props.line && this.props.line == props.line) {
        return props.closeConditionalPanel();
      }

      this.clearConditionalPanel();
    }

    const {
      selectedLocation,
      line,
      editor
    } = props;
    const sourceId = selectedLocation ? selectedLocation.sourceId : "";
    const editorLine = (0, _editor.toEditorLine)(sourceId, line);
    this.cbPanel = editor.codeMirror.addLineWidget(editorLine, this.renderConditionalPanel(props), {
      coverGutter: true,
      noHScroll: false
    });

    if (this.input) {
      let parent = this.input.parentNode;

      while (parent) {
        if (parent instanceof HTMLElement && parent.classList.contains("CodeMirror-scroll")) {
          this.scrollParent = parent;
          break;
        }

        parent = parent.parentNode;
      }

      if (this.scrollParent) {
        this.scrollParent.addEventListener("scroll", this.repositionOnScroll);
        this.repositionOnScroll();
      }

      this.input.focus();
    }
  }

  renderConditionalPanel(props) {
    const {
      breakpoint
    } = props;
    const condition = breakpoint ? breakpoint.condition : "";
    const panel = document.createElement("div");

    _reactDom2.default.render(_react2.default.createElement("div", {
      className: "conditional-breakpoint-panel",
      onClick: () => this.keepFocusOnInput(),
      onBlur: this.props.closeConditionalPanel,
      ref: node => this.panelNode = node
    }, _react2.default.createElement("div", {
      className: "prompt"
    }, "\xBB"), _react2.default.createElement("input", {
      defaultValue: condition,
      placeholder: L10N.getStr("editor.conditionalPanel.placeholder"),
      onKeyDown: this.onKey,
      ref: input => {
        this.input = input;
        this.keepFocusOnInput();
      }
    })), panel);

    return panel;
  }

  render() {
    return null;
  }

}

exports.ConditionalPanel = ConditionalPanel;

const mapStateToProps = state => {
  const line = (0, _selectors.getConditionalPanelLine)(state);
  const selectedLocation = (0, _selectors.getSelectedLocation)(state);
  return {
    selectedLocation,
    breakpoint: (0, _selectors.getBreakpointForLine)(state, selectedLocation.sourceId, line),
    line
  };
};

const {
  setBreakpointCondition,
  openConditionalPanel,
  closeConditionalPanel
} = _actions2.default;
const mapDispatchToProps = {
  setBreakpointCondition,
  openConditionalPanel,
  closeConditionalPanel
};
exports.default = (0, _reactRedux.connect)(mapStateToProps, mapDispatchToProps)(ConditionalPanel);