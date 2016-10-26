/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const React = require("devtools/client/shared/vendor/react");
const Immutable = require("devtools/client/shared/vendor/immutable");
const { LocalizationHelper } = require("devtools/shared/l10n");
const l10n = new LocalizationHelper("devtools/client/locales/components.properties");

// Shortcuts
const { PropTypes, createClass, DOM } = React;
const { div, span, button } = DOM;

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
 * <xul:notifictionbox> binding.
 *
 * See also MDN for more info about <xul:notificationbox>:
 * https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XUL/notificationbox
 */
var NotificationBox = createClass({
  displayName: "NotificationBox",

  propTypes: {
    // List of notifications appended into the box.
    notifications: PropTypes.arrayOf(PropTypes.shape({
      // label to appear on the notification.
      label: PropTypes.string.isRequired,

      // Value used to identify the notification
      value: PropTypes.string.isRequired,

      // URL of image to appear on the notification. If "" then an icon
      // appropriate for the priority level is used.
      image: PropTypes.string.isRequired,

      // Notification priority; see Priority Levels.
      priority: PropTypes.number.isRequired,

      // Array of button descriptions to appear on the notification.
      buttons: PropTypes.arrayOf(PropTypes.shape({
        // Function to be called when the button is activated.
        // This function is passed three arguments:
        // 1) the NotificationBox component the button is associated with
        // 2) the button description as passed to appendNotification.
        // 3) the element which was the target of the button press event.
        // If the return value from this function is not True, then the
        // notification is closed. The notification is also not closed
        // if an error is thrown.
        callback: PropTypes.func.isRequired,

        // The label to appear on the button.
        label: PropTypes.string.isRequired,

        // The accesskey attribute set on the <button> element.
        accesskey: PropTypes.string,
      })),

      // A function to call to notify you of interesting things that happen
      // with the notification box.
      eventCallback: PropTypes.func,
    })),

    // Message that should be shown when hovering over the close button
    closeButtonTooltip: PropTypes.string
  },

  getDefaultProps() {
    return {
      closeButtonTooltip: l10n.getStr("notificationBox.closeTooltip")
    };
  },

  getInitialState() {
    return {
      notifications: new Immutable.OrderedMap()
    };
  },

  /**
   * Create a new notification and display it. If another notification is
   * already present with a higher priority, the new notification will be
   * added behind it. See `propTypes` for arguments description.
   */
  appendNotification(label, value, image, priority, buttons = [],
    eventCallback) {
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

    let notifications = this.state.notifications.set(value, {
      label: label,
      value: value,
      image: image,
      priority: priority,
      type: type,
      buttons: buttons,
      eventCallback: eventCallback,
    });

    // High priorities must be on top.
    notifications = notifications.sortBy((val, key) => {
      return -val.priority;
    });

    this.setState({
      notifications: notifications
    });
  },

  /**
   * Remove specific notification from the list.
   */
  removeNotification(notification) {
    this.close(this.state.notifications.get(notification.value));
  },

  /**
   * Returns an object that represents a notification. It can be
   * used to close it.
   */
  getNotificationWithValue(value) {
    let notification = this.state.notifications.get(value);
    if (!notification) {
      return null;
    }

    // Return an object that can be used to remove the notification
    // later (using `removeNotification` method) or directly close it.
    return Object.assign({}, notification, {
      close: () => {
        this.close(notification);
      }
    });
  },

  getCurrentNotification() {
    return this.state.notifications.first();
  },

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

    this.setState({
      notifications: this.state.notifications.remove(notification.value)
    });
  },

  /**
   * Render a button. A notification can have a set of custom buttons.
   * These are used to execute custom callback.
   */
  renderButton(props, notification) {
    let onClick = event => {
      if (props.callback) {
        let result = props.callback(this, props, event.target);
        if (!result) {
          this.close(notification);
        }
        event.stopPropagation();
      }
    };

    return (
      button({
        key: props.label,
        className: "notification-button",
        accesskey: props.accesskey,
        onClick: onClick},
        props.label
      )
    );
  },

  /**
   * Render a notification.
   */
  renderNotification(notification) {
    return (
      div({
        key: notification.value,
        className: "notification",
        "data-type": notification.type},
        div({className: "notificationInner"},
          div({className: "details"},
            div({
              className: "messageImage",
              "data-type": notification.type}),
            span({className: "messageText"},
              notification.label
            ),
            notification.buttons.map(props =>
              this.renderButton(props, notification)
            )
          ),
          div({
            className: "messageCloseButton",
            title: this.props.closeButtonTooltip,
            onClick: this.close.bind(this, notification)}
          )
        )
      )
    );
  },

  /**
   * Render the top (highest priority) notification. Only one
   * notification is rendered at a time.
   */
  render() {
    let notification = this.state.notifications.first();
    let content = notification ?
      this.renderNotification(notification) :
      null;

    return div({className: "notificationbox"},
      content
    );
  },
});

module.exports.NotificationBox = NotificationBox;
module.exports.PriorityLevels = PriorityLevels;
