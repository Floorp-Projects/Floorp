/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { getStr } = require("../utils/l10n");
const Types = require("../types");

loader.lazyRequireGetter(this, "showMenu", "devtools/client/shared/components/menu/utils", true);

class SettingsMenu extends PureComponent {
  static get propTypes() {
    return {
      leftAlignmentEnabled: PropTypes.bool.isRequired,
      onChangeReloadCondition: PropTypes.func.isRequired,
      onToggleLeftAlignment: PropTypes.func.isRequired,
      reloadConditions: PropTypes.shape(Types.reloadConditions).isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onToggleSettingMenu = this.onToggleSettingMenu.bind(this);
  }

  onToggleSettingMenu(event) {
    const {
      leftAlignmentEnabled,
      onChangeReloadCondition,
      onToggleLeftAlignment,
      reloadConditions,
    } = this.props;

    const menuItems = [
      {
        id: "toggleLeftAlignment",
        checked: leftAlignmentEnabled,
        label: getStr("responsive.leftAlignViewport"),
        type: "checkbox",
        click: () => {
          onToggleLeftAlignment();
        },
      },
      "-",
      {
        id: "touchSimulation",
        checked: reloadConditions.touchSimulation,
        label: getStr("responsive.reloadConditions.touchSimulation"),
        type: "checkbox",
        click: () => {
          onChangeReloadCondition("touchSimulation", !reloadConditions.touchSimulation);
        },
      },
      {
        id: "userAgent",
        checked: reloadConditions.userAgent,
        label: getStr("responsive.reloadConditions.userAgent"),
        type: "checkbox",
        click: () => {
          onChangeReloadCondition("userAgent", !reloadConditions.userAgent);
        },
      },
    ];

    showMenu(menuItems, {
      button: event.target,
      useTopLevelWindow: true,
    });
  }

  render() {
    return (
      dom.button({
        id: "settings-button",
        className: "devtools-button",
        onClick: this.onToggleSettingMenu,
      })
    );
  }
}

const mapStateToProps = state => {
  return {
    leftAlignmentEnabled: state.ui.leftAlignmentEnabled,
  };
};

module.exports = connect(mapStateToProps)(SettingsMenu);
