/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createRef, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { KeyCodes } = require("devtools/client/shared/keycodes");

const { getStr } = require("../utils/l10n");
const Types = require("../types");

class ClassListPanel extends PureComponent {
  static get propTypes() {
    return {
      classes: PropTypes.arrayOf(PropTypes.shape(Types.class)).isRequired,
      onAddClass: PropTypes.func.isRequired,
      onSetClassState: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      // Class list panel input value.
      value: "",
    };

    this.inputRef = createRef();

    this.onInputChange = this.onInputChange.bind(this);
    this.onInputKeyUp = this.onInputKeyUp.bind(this);
    this.onToggleChange = this.onToggleChange.bind(this);
  }

  componentDidMount() {
    this.inputRef.current.focus();
  }

  onInputChange({ target }) {
    this.setState({ value: target.value });
  }

  onInputKeyUp({ target, keyCode }) {
    // On Enter, submit the input.
    if (keyCode === KeyCodes.DOM_VK_RETURN) {
      this.props.onAddClass(target.value);
      this.setState({ value: "" });
    }
  }

  onToggleChange({ target }) {
    this.props.onSetClassState(target.value, target.checked);
  }

  render() {
    return (
      dom.div(
        {
          id: "ruleview-class-panel",
          className: "theme-toolbar ruleview-reveal-panel",
        },
        dom.input({
          className: "devtools-textinput add-class",
          placeholder: getStr("rule.classPanel.newClass.placeholder"),
          onChange: this.onInputChange,
          onKeyUp: this.onInputKeyUp,
          ref: this.inputRef,
          value: this.state.value,
        }),
        dom.div({ className: "classes" },
          this.props.classes.length ?
            this.props.classes.map(({ name, isApplied }) => {
              return (
                dom.label(
                  {
                    key: name,
                    title: name,
                  },
                  dom.input({
                    checked: isApplied,
                    onChange: this.onToggleChange,
                    type: "checkbox",
                    value: name,
                  }),
                  dom.span({}, name)
                )
              );
            })
            :
            dom.p({ className: "no-classes" }, getStr("rule.classPanel.noClasses"))
        )
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    classes: state.classList.classes,
  };
};

module.exports = connect(mapStateToProps)(ClassListPanel);
