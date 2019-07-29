/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";

export class ContextMenuButton extends React.PureComponent {
  constructor(props) {
    super(props);
    this.state = {
      showContextMenu: false,
      contextMenuKeyboard: false,
    };
    this.onClick = this.onClick.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.onUpdate = this.onUpdate.bind(this);
  }

  openContextMenu(isKeyBoard, event) {
    if (this.props.onUpdate) {
      this.props.onUpdate(true);
    }
    this.setState({
      showContextMenu: true,
      contextMenuKeyboard: isKeyBoard,
    });
  }

  onClick(event) {
    event.preventDefault();
    this.openContextMenu(false, event);
  }

  onKeyDown(event) {
    if (event.key === "Enter" || event.key === " ") {
      event.preventDefault();
      this.openContextMenu(true, event);
    }
  }

  onUpdate(showContextMenu) {
    if (this.props.onUpdate) {
      this.props.onUpdate(showContextMenu);
    }
    this.setState({ showContextMenu });
  }

  render() {
    const { tooltipArgs, tooltip, children, refFunction } = this.props;
    const { showContextMenu, contextMenuKeyboard } = this.state;

    return (
      <React.Fragment>
        <button
          aria-haspopup="true"
          data-l10n-id={tooltip}
          data-l10n-args={tooltipArgs ? JSON.stringify(tooltipArgs) : null}
          className="context-menu-button icon"
          onKeyDown={this.onKeyDown}
          onClick={this.onClick}
          ref={refFunction}
        />
        {showContextMenu
          ? React.cloneElement(children, {
              keyboardAccess: contextMenuKeyboard,
              onUpdate: this.onUpdate,
            })
          : null}
      </React.Fragment>
    );
  }
}
