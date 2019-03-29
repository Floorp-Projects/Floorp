/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { LocalizationHelper } = require("devtools/shared/l10n");

const l10n = new LocalizationHelper("devtools/client/locales/components.properties");
const { div, span, button } = dom;

// Priority Levels
const PriorityLevels = {
  PRIORITY_INFO_LOW: 1,
  PRIORITY_INFO_MEDIUM: 2,
  PRIORITY_INFO_HIGH: 3,
  PRIORITY_WARNING_LOW: 4,
  PRIORITY_WARNING_MEDIUM: 5,
  PRIORITY_WARNING_HIGH: 6,
  PRIORITY_CRITICAL_LOW: 7,
  PRIORITY_CRITICAL_MEDIUM: 8,
  PRIORITY_CRITICAL_HIGH: 9,
  PRIORITY_CRITICAL_BLOCK: 10,
};

/**
 * This component represents Notification Box - HTML alternative for
 * <xul:notificationbox> binding.
 *
 * See also MDN for more info about <xul:notificationbox>:
 * https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XUL/notificationbox
 *
 * This component can maintain its own state (list of notifications)
 * as well as consume list of notifications provided as a prop
 * (coming e.g. from Redux store).
 */
class NotificationBox extends Component {
  static get propTypes() {
    return {
      // Optional box ID (used for mounted node ID attribute)
      id: PropTypes.string,
      /**
       * List of notifications appended into the box. Each item of the map is an object
       * of the following shape:
       *   - {String} label: Label to appear on the notification.
       *   - {String} value: Value used to identify the notification.
       *   - {String} image: URL of image to appear on the notification. If "" then an
       *                     appropriate icon for the priority level is used.
       *   - {Number} priority: Notification priority; see Priority Levels.
       *   - {Function} eventCallback: A function to call to notify you of interesting
                                       things that happen with the notification box.
       *   - {Array<Object>} buttons: Array of button descriptions to appear on the
       *                              notification. Should be of the following shape:
       *                     - {Function} callback: This function is passed 3 arguments:
                                                    1) the NotificationBox component
                                                       the button is associated with.
                                                    2) the button description as passed
                                                       to appendNotification.
                                                    3) the element which was the target
                                                       of the button press event.
                                                    If the return value from this function
                                                    is not true, then the notification is
                                                    closed. The notification is also not
                                                    closed if an error is thrown.
                             - {String} label: The label to appear on the button.
                             - {String} accesskey: The accesskey attribute set on the
                                                   <button> element.
      */
      notifications: PropTypes.instanceOf(Map),
      // Message that should be shown when hovering over the close button
      closeButtonTooltip: PropTypes.string,
      // Wraps text when passed from console window as wrapping: true
      wrapping: PropTypes.bool,
    };
  }

  static get defaultProps() {
    return {
      closeButtonTooltip: l10n.getStr("notificationBox.closeTooltip"),
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      notifications: new Map(),
    };

    this.appendNotification = this.appendNotification.bind(this);
    this.removeNotification = this.removeNotification.bind(this);
    this.getNotificationWithValue = this.getNotificationWithValue.bind(this);
    this.getCurrentNotification = this.getCurrentNotification.bind(this);
    this.close = this.close.bind(this);
    this.renderButton = this.renderButton.bind(this);
    this.renderNotification = this.renderNotification.bind(this);
  }

  /**
   * Create a new notification and display it. If another notification is
   * already present with a higher priority, the new notification will be
   * added behind it. See `propTypes` for arguments description.
   */
  appendNotification(label, value, image, priority, buttons = [], eventCallback) {
    const newState = appendNotification(this.state, {
      label,
      value,
      image,
      priority,
      buttons,
      eventCallback,
    });

    this.setState(newState);
  }

  /**
   * Remove specific notification from the list.
   */
  removeNotification(notification) {
    if (notification) {
      this.close(this.state.notifications.get(notification.value));
    }
  }

  /**
   * Returns an object that represents a notification. It can be
   * used to close it.
   */
  getNotificationWithValue(value) {
    const notification = this.state.notifications.get(value);
    if (!notification) {
      return null;
    }

    // Return an object that can be used to remove the notification
    // later (using `removeNotification` method) or directly close it.
    return Object.assign({}, notification, {
      close: () => {
        this.close(notification);
      },
    });
  }

  getCurrentNotification() {
    return getHighestPriorityNotification(this.state.notifications);
  }

  /**
   * Close specified notification.
   */
  close(notification) {
    if (!notification) {
      return;
    }

    if (notification.eventCallback) {
      notification.eventCallback("removed");
    }

    if (!this.state.notifications.get(notification.value)) {
      return;
    }

    const newNotifications = new Map(this.state.notifications);
    newNotifications.delete(notification.value);
    this.setState({
      notifications: newNotifications,
    });
  }

  /**
   * Render a button. A notification can have a set of custom buttons.
   * These are used to execute custom callback.
   */
  renderButton(props, notification) {
    const onClick = event => {
      if (props.callback) {
        const result = props.callback(this, props, event.target);
        if (!result) {
          this.close(notification);
        }
        event.stopPropagation();
      }
    };

    return (
      button({
        key: props.label,
        className: "notificationButton",
        accesskey: props.accesskey,
        onClick: onClick},
        props.label
      )
    );
  }

  /**
   * Render a notification.
   */
  renderNotification(notification) {
    return (
      div({
        key: notification.value,
        className: "notification",
        "data-key": notification.value,
        "data-type": notification.type},
        div(
          {className: "notificationInner"},
          div({
            className: "messageImage",
            "data-type": notification.type,
          }),
          span({
            className: "messageText",
            title: notification.label,
          },
            notification.label
          ),
          notification.buttons.map(props =>
            this.renderButton(props, notification)
          ),
          div({
            className: "messageCloseButton",
            title: this.props.closeButtonTooltip,
            onClick: this.close.bind(this, notification)}
          )
        )
      )
    );
  }

  /**
   * Render the top (highest priority) notification. Only one
   * notification is rendered at a time.
   */
  render() {
    const notifications = this.props.notifications || this.state.notifications;
    const notification = getHighestPriorityNotification(notifications);
    const content = notification
      ? this.renderNotification(notification)
      : null;
    const classNames = ["notificationbox"];
    if (this.props.wrapping) {
      classNames.push("wrapping");
    }
    return div({
      className: classNames.join(" "),
      id: this.props.id,
    }, content);
  }
}

// Helpers

/**
 * Create a new notification. If another notification is already present with
 * a higher priority, the new notification will be added behind it.
 * See `propTypes` for arguments description.
 */
function appendNotification(state, props) {
  const {
    label,
    value,
    image,
    priority,
    buttons,
    eventCallback,
  } = props;

  // Priority level must be within expected interval
  // (see priority levels at the top of this file).
  if (priority < PriorityLevels.PRIORITY_INFO_LOW ||
    priority > PriorityLevels.PRIORITY_CRITICAL_BLOCK) {
    throw new Error("Invalid notification priority " + priority);
  }

  // Custom image URL is not supported yet.
  if (image) {
    throw new Error("Custom image URL is not supported yet");
  }

  let type = "warning";
  if (priority >= PriorityLevels.PRIORITY_CRITICAL_LOW) {
    type = "critical";
  } else if (priority <= PriorityLevels.PRIORITY_INFO_HIGH) {
    type = "info";
  }

  if (!state.notifications) {
    state.notifications = new Map();
  }

  const notifications = new Map(state.notifications);
  notifications.set(value, {
    label,
    value,
    image,
    priority,
    type,
    buttons: Array.isArray(buttons) ? buttons : [],
    eventCallback,
  });

  return {
    notifications,
  };
}

function getNotificationWithValue(notifications, value) {
  return notifications ? notifications.get(value) : null;
}

function removeNotificationWithValue(notifications, value) {
  const newNotifications = new Map(notifications);
  newNotifications.delete(value);

  return {
    notifications: newNotifications,
  };
}

function getHighestPriorityNotification(notifications) {
  if (!notifications) {
    return null;
  }

  let currentNotification = null;
  // High priorities must be on top.
  for (const [, notification] of notifications) {
    if (!currentNotification || notification.priority > currentNotification.priority) {
      currentNotification = notification;
    }
  }

  return currentNotification;
}

module.exports.NotificationBox = NotificationBox;
module.exports.PriorityLevels = PriorityLevels;
module.exports.appendNotification = appendNotification;
module.exports.getNotificationWithValue = getNotificationWithValue;
module.exports.removeNotificationWithValue = removeNotificationWithValue;
