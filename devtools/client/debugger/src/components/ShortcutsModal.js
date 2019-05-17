/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import Modal from "./shared/Modal";
import classnames from "classnames";
import { formatKeyShortcut } from "../utils/text";

import "./ShortcutsModal.css";

type Props = {
  enabled: boolean,
  additionalClass: string,
  handleClose: () => void,
};

export class ShortcutsModal extends Component<Props> {
  renderPrettyCombos(combo: string) {
    return combo
      .split(" ")
      .map(c => (
        <span key={c} className="keystroke">
          {c}
        </span>
      ))
      .reduce((prev, curr) => [prev, " + ", curr]);
  }

  renderShorcutItem(title: string, combo: string) {
    return (
      <li>
        <span>{title}</span>
        <span>{this.renderPrettyCombos(combo)}</span>
      </li>
    );
  }

  renderEditorShortcuts() {
    return (
      <ul className="shortcuts-list">
        {this.renderShorcutItem(
          L10N.getStr("shortcuts.toggleBreakpoint"),
          formatKeyShortcut(L10N.getStr("toggleBreakpoint.key"))
        )}
        {this.renderShorcutItem(
          L10N.getStr("shortcuts.toggleCondPanel.breakpoint"),
          formatKeyShortcut(L10N.getStr("toggleCondPanel.breakpoint.key"))
        )}
        {this.renderShorcutItem(
          L10N.getStr("shortcuts.toggleCondPanel.logPoint"),
          formatKeyShortcut(L10N.getStr("toggleCondPanel.logPoint.key"))
        )}
      </ul>
    );
  }

  renderSteppingShortcuts() {
    return (
      <ul className="shortcuts-list">
        {this.renderShorcutItem(L10N.getStr("shortcuts.pauseOrResume"), "F8")}
        {this.renderShorcutItem(L10N.getStr("shortcuts.stepOver"), "F10")}
        {this.renderShorcutItem(L10N.getStr("shortcuts.stepIn"), "F11")}
        {this.renderShorcutItem(
          L10N.getStr("shortcuts.stepOut"),
          formatKeyShortcut(L10N.getStr("stepOut.key"))
        )}
      </ul>
    );
  }

  renderSearchShortcuts() {
    return (
      <ul className="shortcuts-list">
        {this.renderShorcutItem(
          L10N.getStr("shortcuts.fileSearch2"),
          formatKeyShortcut(L10N.getStr("sources.search.key2"))
        )}
        {this.renderShorcutItem(
          L10N.getStr("shortcuts.searchAgain2"),
          formatKeyShortcut(L10N.getStr("sourceSearch.search.again.key2"))
        )}
        {this.renderShorcutItem(
          L10N.getStr("shortcuts.projectSearch2"),
          formatKeyShortcut(L10N.getStr("projectTextSearch.key"))
        )}
        {this.renderShorcutItem(
          L10N.getStr("shortcuts.functionSearch2"),
          formatKeyShortcut(L10N.getStr("functionSearch.key"))
        )}
        {this.renderShorcutItem(
          L10N.getStr("shortcuts.gotoLine"),
          formatKeyShortcut(L10N.getStr("gotoLineModal.key2"))
        )}
      </ul>
    );
  }

  renderShortcutsContent() {
    return (
      <div
        className={classnames("shortcuts-content", this.props.additionalClass)}
      >
        <div className="shortcuts-section">
          <h2>{L10N.getStr("shortcuts.header.editor")}</h2>
          {this.renderEditorShortcuts()}
        </div>
        <div className="shortcuts-section">
          <h2>{L10N.getStr("shortcuts.header.stepping")}</h2>
          {this.renderSteppingShortcuts()}
        </div>
        <div className="shortcuts-section">
          <h2>{L10N.getStr("shortcuts.header.search")}</h2>
          {this.renderSearchShortcuts()}
        </div>
      </div>
    );
  }

  render() {
    const { enabled } = this.props;

    if (!enabled) {
      return null;
    }

    return (
      <Modal
        in={enabled}
        additionalClass="shortcuts-modal"
        handleClose={this.props.handleClose}
      >
        {this.renderShortcutsContent()}
      </Modal>
    );
  }
}
