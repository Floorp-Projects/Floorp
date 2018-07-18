/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/redux/visibility-handler-connect");

const actions = require("devtools/client/webconsole/actions/index");
const ConsoleOutput = createFactory(require("devtools/client/webconsole/components/ConsoleOutput"));
const FilterBar = createFactory(require("devtools/client/webconsole/components/FilterBar"));
const SideBar = createFactory(require("devtools/client/webconsole/components/SideBar"));
const JSTerm = createFactory(require("devtools/client/webconsole/components/JSTerm"));
const NotificationBox = createFactory(require("devtools/client/shared/components/NotificationBox").NotificationBox);

const l10n = require("devtools/client/webconsole/webconsole-l10n");
const { Utils: WebConsoleUtils } = require("devtools/client/webconsole/utils");

const SELF_XSS_OK = l10n.getStr("selfxss.okstring");
const SELF_XSS_MSG = l10n.getFormatStr("selfxss.msg", [SELF_XSS_OK]);

const {
  getNotificationWithValue,
  PriorityLevels,
} = require("devtools/client/shared/components/NotificationBox");

const { getAllNotifications } = require("devtools/client/webconsole/selectors/notifications");

const { div } = dom;

/**
 * Console root Application component.
 */
class App extends Component {
  static get propTypes() {
    return {
      attachRefToHud: PropTypes.func.isRequired,
      dispatch: PropTypes.func.isRequired,
      hud: PropTypes.object.isRequired,
      notifications: PropTypes.object,
      onFirstMeaningfulPaint: PropTypes.func.isRequired,
      serviceContainer: PropTypes.object.isRequired,
      closeSplitConsole: PropTypes.func.isRequired,
      jstermCodeMirror: PropTypes.bool,
    };
  }

  constructor(props) {
    super(props);

    this.onPaste = this.onPaste.bind(this);
  }

  onPaste(event) {
    const {
      dispatch,
      hud,
      notifications,
    } = this.props;

    const {
      usageCount,
      CONSOLE_ENTRY_THRESHOLD
    } = WebConsoleUtils;

    // Bail out if self-xss notification is suppressed.
    if (hud.isBrowserConsole || usageCount >= CONSOLE_ENTRY_THRESHOLD) {
      return;
    }

    // Stop event propagation, so the clipboard content is *not* inserted.
    event.preventDefault();
    event.stopPropagation();

    // Bail out if self-xss notification is already there.
    if (getNotificationWithValue(notifications, "selfxss-notification")) {
      return;
    }

    const input = event.target;

    // Cleanup function if notification is closed by the user.
    const removeCallback = (eventType) => {
      if (eventType == "removed") {
        input.removeEventListener("keyup", pasteKeyUpHandler);
        dispatch(actions.removeNotification("selfxss-notification"));
      }
    };

    // Create self-xss notification
    dispatch(actions.appendNotification(
      SELF_XSS_MSG,
      "selfxss-notification",
      null,
      PriorityLevels.PRIORITY_WARNING_HIGH,
      null,
      removeCallback
    ));

    // Remove notification automatically when the user types "allow pasting".
    const pasteKeyUpHandler = (e) => {
      const value = e.target.value;
      if (value.includes(SELF_XSS_OK)) {
        dispatch(actions.removeNotification("selfxss-notification"));
        input.removeEventListener("keyup", pasteKeyUpHandler);
        WebConsoleUtils.usageCount = WebConsoleUtils.CONSOLE_ENTRY_THRESHOLD;
      }
    };

    input.addEventListener("keyup", pasteKeyUpHandler);
  }

  // Rendering

  render() {
    const {
      attachRefToHud,
      hud,
      notifications,
      onFirstMeaningfulPaint,
      serviceContainer,
      closeSplitConsole,
      jstermCodeMirror,
    } = this.props;

    const classNames = ["webconsole-output-wrapper"];
    if (jstermCodeMirror) {
      classNames.push("jsterm-cm");
    }

    // Render the entire Console panel. The panel consists
    // from the following parts:
    // * FilterBar - Buttons & free text for content filtering
    // * Content - List of logs & messages
    // * SideBar - Object inspector
    // * NotificationBox - Notifications for JSTerm (self-xss warning at the moment)
    // * JSTerm - Input command line.
    return (
      div({
        className: classNames.join(" "),
        ref: node => {
          this.node = node;
        }},
        FilterBar({
          hidePersistLogsCheckbox: hud.isBrowserConsole,
          serviceContainer: {
            attachRefToHud
          },
          closeSplitConsole
        }),
        ConsoleOutput({
          serviceContainer,
          onFirstMeaningfulPaint,
        }),
        SideBar({
          serviceContainer,
        }),
        NotificationBox({
          id: "webconsole-notificationbox",
          notifications,
        }),
        JSTerm({
          hud,
          serviceContainer,
          onPaste: this.onPaste,
          codeMirrorEnabled: jstermCodeMirror,
        }),
      )
    );
  }
}

const mapStateToProps = state => ({
  notifications: getAllNotifications(state),
});

const mapDispatchToProps = dispatch => ({
  dispatch,
});

module.exports = connect(mapStateToProps, mapDispatchToProps)(App);
