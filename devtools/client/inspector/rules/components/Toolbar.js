/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const SearchBox = createFactory(require("./SearchBox"));

loader.lazyGetter(this, "ClassListPanel", function() {
  return createFactory(require("./ClassListPanel"));
});
loader.lazyGetter(this, "PseudoClassPanel", function() {
  return createFactory(require("./PseudoClassPanel"));
});

const { getStr } = require("../utils/l10n");

class Toolbar extends PureComponent {
  static get propTypes() {
    return {
      isClassPanelExpanded: PropTypes.bool.isRequired,
      onAddClass: PropTypes.func.isRequired,
      onSetClassState: PropTypes.func.isRequired,
      onToggleClassPanelExpanded: PropTypes.func.isRequired,
      onTogglePseudoClass: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      // Whether or not the pseudo class panel is expanded.
      isPseudoClassPanelExpanded: false,
    };

    this.onClassPanelToggle = this.onClassPanelToggle.bind(this);
    this.onPseudoClassPanelToggle = this.onPseudoClassPanelToggle.bind(this);
  }

  onClassPanelToggle(event) {
    event.stopPropagation();

    const isClassPanelExpanded = !this.props.isClassPanelExpanded;
    this.props.onToggleClassPanelExpanded(isClassPanelExpanded);
    this.setState(prevState => {
      return {
        isPseudoClassPanelExpanded: isClassPanelExpanded ?
                                    false :
                                    prevState.isPseudoClassPanelExpanded,
      };
    });
  }

  onPseudoClassPanelToggle(event) {
    event.stopPropagation();

    const isPseudoClassPanelExpanded = !this.state.isPseudoClassPanelExpanded;

    if (isPseudoClassPanelExpanded) {
      this.props.onToggleClassPanelExpanded(false);
    }

    this.setState({ isPseudoClassPanelExpanded });
  }

  render() {
    const { isClassPanelExpanded } = this.props;
    const { isPseudoClassPanelExpanded } = this.state;

    return (
      dom.div(
        {
          id: "ruleview-toolbar-container",
          className: "devtools-toolbar",
        },
        dom.div({ id: "ruleview-toolbar" },
          SearchBox({}),
          dom.div({ id: "ruleview-command-toolbar" },
            dom.button({
              id: "ruleview-add-rule-button",
              className: "devtools-button",
              title: getStr("rule.addRule.tooltip"),
            }),
            dom.button({
              id: "pseudo-class-panel-toggle",
              className: "devtools-button" +
                          (isPseudoClassPanelExpanded ? " checked" : ""),
              onClick: this.onPseudoClassPanelToggle,
              title: getStr("rule.togglePseudo.tooltip"),
            }),
            dom.button({
              id: "class-panel-toggle",
              className: "devtools-button" +
                          (isClassPanelExpanded ? " checked" : ""),
              onClick: this.onClassPanelToggle,
              title: getStr("rule.classPanel.toggleClass.tooltip"),
            })
          )
        ),
        isClassPanelExpanded ?
          ClassListPanel({
            onAddClass: this.props.onAddClass,
            onSetClassState: this.props.onSetClassState,
          })
          :
          null,
        isPseudoClassPanelExpanded ?
          PseudoClassPanel({
            onTogglePseudoClass: this.props.onTogglePseudoClass,
          })
          :
          null
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    isClassPanelExpanded: state.classList.isClassPanelExpanded,
  };
};

module.exports = connect(mapStateToProps)(Toolbar);
