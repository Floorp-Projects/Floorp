/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("../types");
const { getStr } = require("../utils/l10n");
const ToggleMenu = createFactory(require("./ToggleMenu"));

class ReloadConditions extends PureComponent {
  static get propTypes() {
    return {
      reloadConditions: PropTypes.shape(Types.reloadConditions).isRequired,
      onChangeReloadCondition: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      reloadConditions,
      onChangeReloadCondition,
    } = this.props;

    return ToggleMenu({
      id: "global-reload-conditions-menu",
      items: [
        {
          id: "touchSimulation",
          label: getStr("responsive.reloadConditions.touchSimulation"),
          checked: reloadConditions.touchSimulation,
        },
        {
          id: "userAgent",
          label: getStr("responsive.reloadConditions.userAgent"),
          checked: reloadConditions.userAgent,
        },
      ],
      label: getStr("responsive.reloadConditions.label"),
      title: getStr("responsive.reloadConditions.title"),
      onChange: onChangeReloadCondition,
    });
  }
}

module.exports = ReloadConditions;
