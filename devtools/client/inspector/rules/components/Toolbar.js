/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

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
    return {};
  }

  constructor(props) {
    super(props);

    this.state = {
      // Whether or not the class panel is expanded.
      isClassPanelExpanded: false,
      // Whether or not the pseudo class panel is expanded.
      isPseudoClassPanelExpanded: false,
    };

    this.onClassPanelToggle = this.onClassPanelToggle.bind(this);
    this.onPseudoClassPanelToggle = this.onPseudoClassPanelToggle.bind(this);
  }

  onClassPanelToggle(event) {
    event.stopPropagation();

    this.setState(prevState => {
      const isClassPanelExpanded = !prevState.isClassPanelExpanded;
      const isPseudoClassPanelExpanded = isClassPanelExpanded ?
        false : prevState.isPseudoClassPanelExpanded;

      return {
        isClassPanelExpanded,
        isPseudoClassPanelExpanded,
      };
    });
  }

  onPseudoClassPanelToggle(event) {
    event.stopPropagation();

    this.setState(prevState => {
      const isPseudoClassPanelExpanded = !prevState.isPseudoClassPanelExpanded;
      const isClassPanelExpanded = isPseudoClassPanelExpanded ?
        false : prevState.isClassPanelExpanded;

      return {
        isClassPanelExpanded,
        isPseudoClassPanelExpanded,
      };
    });
  }

  render() {
    const {
      isClassPanelExpanded,
      isPseudoClassPanelExpanded,
    } = this.state;

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
          ClassListPanel({})
          :
          null,
        isPseudoClassPanelExpanded ?
          PseudoClassPanel({})
          :
          null
      )
    );
  }
}

module.exports = Toolbar;
