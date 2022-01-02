/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import PropTypes from "prop-types";
import React, { Component } from "react";

import { connect } from "../../utils/connect";
import classnames from "classnames";
import { features, prefs } from "../../utils/prefs";
import {
  getIsWaitingOnBreak,
  getSkipPausing,
  getCurrentThread,
  isTopFrameSelected,
  getThreadContext,
  getIsCurrentThreadPaused,
} from "../../selectors";
import { formatKeyShortcut } from "../../utils/text";
import actions from "../../actions";
import { debugBtn } from "../shared/Button/CommandBarButton";
import AccessibleImage from "../shared/AccessibleImage";
import "./CommandBar.css";

import { appinfo } from "devtools-services";

const MenuButton = require("devtools/client/shared/components/menu/MenuButton");
const MenuItem = require("devtools/client/shared/components/menu/MenuItem");
const MenuList = require("devtools/client/shared/components/menu/MenuList");

const isMacOS = appinfo.OS === "Darwin";

// NOTE: the "resume" command will call either the resume or breakOnNext action
// depending on whether or not the debugger is paused or running
const COMMANDS = ["resume", "stepOver", "stepIn", "stepOut"];

const KEYS = {
  WINNT: {
    resume: "F8",
    stepOver: "F10",
    stepIn: "F11",
    stepOut: "Shift+F11",
  },
  Darwin: {
    resume: "Cmd+\\",
    stepOver: "Cmd+'",
    stepIn: "Cmd+;",
    stepOut: "Cmd+Shift+:",
    stepOutDisplay: "Cmd+Shift+;",
  },
  Linux: {
    resume: "F8",
    stepOver: "F10",
    stepIn: "F11",
    stepOut: "Shift+F11",
  },
};

function getKey(action) {
  return getKeyForOS(appinfo.OS, action);
}

function getKeyForOS(os, action) {
  const osActions = KEYS[os] || KEYS.Linux;
  return osActions[action];
}

function formatKey(action) {
  const key = getKey(`${action}Display`) || getKey(action);
  if (isMacOS) {
    const winKey =
      getKeyForOS("WINNT", `${action}Display`) || getKeyForOS("WINNT", action);
    // display both Windows type and Mac specific keys
    return formatKeyShortcut([key, winKey].join(" "));
  }
  return formatKeyShortcut(key);
}

class CommandBar extends Component {
  componentWillUnmount() {
    const { shortcuts } = this.context;

    COMMANDS.forEach(action => shortcuts.off(getKey(action)));

    if (isMacOS) {
      COMMANDS.forEach(action => shortcuts.off(getKeyForOS("WINNT", action)));
    }
  }

  componentDidMount() {
    const { shortcuts } = this.context;

    COMMANDS.forEach(action =>
      shortcuts.on(getKey(action), e => this.handleEvent(e, action))
    );

    if (isMacOS) {
      // The Mac supports both the Windows Function keys
      // as well as the Mac non-Function keys
      COMMANDS.forEach(action =>
        shortcuts.on(getKeyForOS("WINNT", action), e =>
          this.handleEvent(e, action)
        )
      );
    }
  }

  handleEvent(e, action) {
    const { cx } = this.props;
    e.preventDefault();
    e.stopPropagation();
    if (action === "resume") {
      this.props.isPaused ? this.props.resume() : this.props.breakOnNext(cx);
    } else {
      this.props[action](cx);
    }
  }

  renderStepButtons() {
    const { isPaused, topFrameSelected } = this.props;
    const className = isPaused ? "active" : "disabled";
    const isDisabled = !isPaused;

    return [
      this.renderPauseButton(),
      debugBtn(
        () => this.props.stepOver(),
        "stepOver",
        className,
        L10N.getFormatStr("stepOverTooltip", formatKey("stepOver")),
        isDisabled
      ),
      debugBtn(
        () => this.props.stepIn(),
        "stepIn",
        className,
        L10N.getFormatStr("stepInTooltip", formatKey("stepIn")),
        isDisabled || (features.frameStep && !topFrameSelected)
      ),
      debugBtn(
        () => this.props.stepOut(),
        "stepOut",
        className,
        L10N.getFormatStr("stepOutTooltip", formatKey("stepOut")),
        isDisabled
      ),
    ];
  }

  resume() {
    this.props.resume();
  }

  renderPauseButton() {
    const { cx, breakOnNext, isWaitingOnBreak } = this.props;

    if (this.props.isPaused) {
      return debugBtn(
        () => this.resume(),
        "resume",
        "active",
        L10N.getFormatStr("resumeButtonTooltip", formatKey("resume"))
      );
    }

    if (isWaitingOnBreak) {
      return debugBtn(
        null,
        "pause",
        "disabled",
        L10N.getStr("pausePendingButtonTooltip"),
        true
      );
    }

    return debugBtn(
      () => breakOnNext(cx),
      "pause",
      "active",
      L10N.getFormatStr("pauseButtonTooltip", formatKey("resume"))
    );
  }

  renderSkipPausingButton() {
    const { skipPausing, toggleSkipPausing } = this.props;

    if (!features.skipPausing) {
      return null;
    }

    return (
      <button
        className={classnames(
          "command-bar-button",
          "command-bar-skip-pausing",
          {
            active: skipPausing,
          }
        )}
        title={
          skipPausing
            ? L10N.getStr("undoSkipPausingTooltip.label")
            : L10N.getStr("skipPausingTooltip.label")
        }
        onClick={toggleSkipPausing}
      >
        <AccessibleImage
          className={skipPausing ? "enable-pausing" : "disable-pausing"}
        />
      </button>
    );
  }

  renderSettingsButton() {
    const { toolboxDoc } = this.context;

    return (
      <MenuButton
        menuId="debugger-settings-menu-button"
        toolboxDoc={toolboxDoc}
        className="devtools-button command-bar-button debugger-settings-menu-button"
        title={L10N.getStr("settings.button.label")}
      >
        {() => this.renderSettingsMenuItems()}
      </MenuButton>
    );
  }

  renderSettingsMenuItems() {
    return (
      <MenuList id="debugger-settings-menu-list">
        <MenuItem
          key="debugger-settings-menu-item-disable-javascript"
          className="menu-item debugger-settings-menu-item-disable-javascript"
          checked={!this.props.javascriptEnabled}
          label={L10N.getStr("settings.disableJavaScript.label")}
          tooltip={L10N.getStr("settings.disableJavaScript.tooltip")}
          onClick={() => {
            this.props.toggleJavaScriptEnabled(!this.props.javascriptEnabled);
          }}
        />
        <MenuItem
          key="debugger-settings-menu-item-disable-inline-previews"
          checked={features.inlinePreview}
          label={L10N.getStr("inlinePreview.toggle.label")}
          tooltip={L10N.getStr("inlinePreview.toggle.tooltip")}
          onClick={() =>
            this.props.toggleInlinePreview(!features.inlinePreview)
          }
        />
        <MenuItem
          key="debugger-settings-menu-item-disable-wrap-lines"
          checked={prefs.editorWrapping}
          label={L10N.getStr("editorWrapping.toggle.label")}
          tooltip={L10N.getStr("editorWrapping.toggle.tooltip")}
          onClick={() => this.props.toggleEditorWrapping(!prefs.editorWrapping)}
        />
        <MenuItem
          key="debugger-settings-menu-item-disable-sourcemaps"
          checked={prefs.clientSourceMapsEnabled}
          label={L10N.getStr("settings.toggleSourceMaps.label")}
          tooltip={L10N.getStr("settings.toggleSourceMaps.tooltip")}
          onClick={() =>
            this.props.toggleSourceMapsEnabled(!prefs.clientSourceMapsEnabled)
          }
        />
      </MenuList>
    );
  }

  render() {
    return (
      <div
        className={classnames("command-bar", {
          vertical: !this.props.horizontal,
        })}
      >
        {this.renderStepButtons()}
        <div className="filler" />
        {this.renderSkipPausingButton()}
        <div className="devtools-separator" />
        {this.renderSettingsButton()}
      </div>
    );
  }
}

CommandBar.contextTypes = {
  shortcuts: PropTypes.object,
  toolboxDoc: PropTypes.object,
};

const mapStateToProps = state => ({
  cx: getThreadContext(state),
  isWaitingOnBreak: getIsWaitingOnBreak(state, getCurrentThread(state)),
  skipPausing: getSkipPausing(state),
  topFrameSelected: isTopFrameSelected(state, getCurrentThread(state)),
  javascriptEnabled: state.ui.javascriptEnabled,
  isPaused: getIsCurrentThreadPaused(state),
});

export default connect(mapStateToProps, {
  resume: actions.resume,
  stepIn: actions.stepIn,
  stepOut: actions.stepOut,
  stepOver: actions.stepOver,
  breakOnNext: actions.breakOnNext,
  pauseOnExceptions: actions.pauseOnExceptions,
  toggleSkipPausing: actions.toggleSkipPausing,
  toggleInlinePreview: actions.toggleInlinePreview,
  toggleEditorWrapping: actions.toggleEditorWrapping,
  toggleSourceMapsEnabled: actions.toggleSourceMapsEnabled,
  toggleJavaScriptEnabled: actions.toggleJavaScriptEnabled,
})(CommandBar);
