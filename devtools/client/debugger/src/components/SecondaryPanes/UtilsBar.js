/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow
import React, { Component } from "react";
import classnames from "classnames";
import { debugBtn } from "../shared/Button/CommandBarButton";
import "./CommandBar.css";

type Props = {
  horizontal: boolean,
  toggleShortcutsModal: () => void,
};

class UtilsBar extends Component<Props> {
  renderUtilButtons() {
    return [
      debugBtn(
        this.props.toggleShortcutsModal,
        "shortcuts",
        "active",
        L10N.getStr("shortcuts.buttonName"),
        false
      ),
    ];
  }

  render() {
    return (
      <div
        className={classnames("command-bar bottom", {
          vertical: !this.props.horizontal,
        })}
      >
        {this.renderUtilButtons()}
      </div>
    );
  }
}

export default UtilsBar;
