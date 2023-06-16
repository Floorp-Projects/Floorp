/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import PropTypes from "prop-types";
import Modal from "./shared/Modal";
import { formatKeyShortcut } from "../utils/text";
const classnames = require("devtools/client/shared/classnames.js");

import "./ShortcutsModal.css";

const isMacOS = Services.appinfo.OS === "Darwin";

export class ShortcutsModal extends Component {
  static get propTypes() {
    return {
      enabled: PropTypes.bool.isRequired,
      handleClose: PropTypes.func.isRequired,
    };
  }

  renderPrettyCombos(combo) {
    return combo
      .split(" ")
      .map(c => (
        <span key={c} className="keystroke">
          {c}
        </span>
      ))
      .reduce((prev, curr) => [prev, " + ", curr]);
  }

  renderShorcutItem(title, combo) {
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
          L10N.getStr("shortcuts.projectSearch2"),
          formatKeyShortcut(L10N.getStr("projectTextSearch.key"))
        )}
        {this.renderShorcutItem(
          L10N.getStr("shortcuts.functionSearch2"),
          formatKeyShortcut(L10N.getStr("functionSearch.key"))
        )}
        {this.renderShorcutItem(
          L10N.getStr("shortcuts.gotoLine"),
          formatKeyShortcut(L10N.getStr("gotoLineModal.key3"))
        )}
      </ul>
    );
  }

  renderShortcutsContent() {
    return (
      <div className={classnames("shortcuts-content", isMacOS ? "mac" : "")}>
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
