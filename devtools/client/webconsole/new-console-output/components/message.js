/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  createClass,
  createFactory,
  DOM: dom,
  PropTypes
} = require("devtools/client/shared/vendor/react");
const { l10n } = require("devtools/client/webconsole/new-console-output/utils/messages");
const actions = require("devtools/client/webconsole/new-console-output/actions/index");
const {MESSAGE_SOURCE} = require("devtools/client/webconsole/new-console-output/constants");
const CollapseButton = createFactory(require("devtools/client/webconsole/new-console-output/components/collapse-button"));
const MessageIndent = createFactory(require("devtools/client/webconsole/new-console-output/components/message-indent").MessageIndent);
const MessageIcon = createFactory(require("devtools/client/webconsole/new-console-output/components/message-icon"));
const MessageRepeat = createFactory(require("devtools/client/webconsole/new-console-output/components/message-repeat"));
const FrameView = createFactory(require("devtools/client/shared/components/frame"));
const StackTrace = createFactory(require("devtools/client/shared/components/stack-trace"));

const Message = createClass({
  displayName: "Message",

  propTypes: {
    open: PropTypes.bool,
    collapsible: PropTypes.bool,
    collapseTitle: PropTypes.string,
    source: PropTypes.string.isRequired,
    type: PropTypes.string.isRequired,
    level: PropTypes.string.isRequired,
    indent: PropTypes.number.isRequired,
    topLevelClasses: PropTypes.array.isRequired,
    messageBody: PropTypes.any.isRequired,
    repeat: PropTypes.any,
    frame: PropTypes.any,
    attachment: PropTypes.any,
    stacktrace: PropTypes.any,
    messageId: PropTypes.string,
    scrollToMessage: PropTypes.bool,
    exceptionDocURL: PropTypes.string,
    parameters: PropTypes.object,
    request: PropTypes.object,
    dispatch: PropTypes.func,
    timeStamp: PropTypes.number,
    serviceContainer: PropTypes.shape({
      emitNewMessage: PropTypes.func.isRequired,
      onViewSourceInDebugger: PropTypes.func.isRequired,
      onViewSourceInScratchpad: PropTypes.func.isRequired,
      onViewSourceInStyleEditor: PropTypes.func.isRequired,
      openContextMenu: PropTypes.func.isRequired,
      openLink: PropTypes.func.isRequired,
      sourceMapService: PropTypes.any,
    }),
  },

  getDefaultProps: function () {
    return {
      indent: 0
    };
  },

  componentDidMount() {
    if (this.messageNode) {
      if (this.props.scrollToMessage) {
        this.messageNode.scrollIntoView();
      }
      // Event used in tests. Some message types don't pass it in because existing tests
      // did not emit for them.
      if (this.props.serviceContainer) {
        this.props.serviceContainer.emitNewMessage(
          this.messageNode, this.props.messageId);
      }
    }
  },

  onLearnMoreClick: function () {
    let {exceptionDocURL} = this.props;
    this.props.serviceContainer.openLink(exceptionDocURL);
  },

  onContextMenu(e) {
    let { serviceContainer, source, request } = this.props;
    let messageInfo = {
      source,
      request,
    };
    serviceContainer.openContextMenu(e, messageInfo);
    e.stopPropagation();
    e.preventDefault();
  },

  render() {
    const {
      messageId,
      open,
      collapsible,
      collapseTitle,
      source,
      type,
      level,
      indent,
      topLevelClasses,
      messageBody,
      frame,
      stacktrace,
      serviceContainer,
      dispatch,
      exceptionDocURL,
      timeStamp = Date.now(),
    } = this.props;

    topLevelClasses.push("message", source, type, level);
    if (open) {
      topLevelClasses.push("open");
    }

    const timestampEl = dom.span({
      className: "timestamp devtools-monospace"
    }, l10n.timestampString(timeStamp));

    const icon = MessageIcon({level});

    // Figure out if there is an expandable part to the message.
    let attachment = null;
    if (this.props.attachment) {
      attachment = this.props.attachment;
    } else if (stacktrace) {
      const child = open ? StackTrace({
        stacktrace: stacktrace,
        onViewSourceInDebugger: serviceContainer.onViewSourceInDebugger,
        onViewSourceInScratchpad: serviceContainer.onViewSourceInScratchpad,
      }) : null;
      attachment = dom.div({ className: "stacktrace devtools-monospace" }, child);
    }

    // If there is an expandable part, make it collapsible.
    let collapse = null;
    if (collapsible) {
      collapse = CollapseButton({
        open,
        title: collapseTitle,
        onClick: function () {
          if (open) {
            dispatch(actions.messageClose(messageId));
          } else {
            dispatch(actions.messageOpen(messageId));
          }
        },
      });
    }

    const repeat = this.props.repeat ? MessageRepeat({repeat: this.props.repeat}) : null;

    let onFrameClick;
    if (serviceContainer && frame) {
      if (source === MESSAGE_SOURCE.CSS) {
        onFrameClick = serviceContainer.onViewSourceInStyleEditor;
      } else if (/^Scratchpad\/\d+$/.test(frame.source)) {
        onFrameClick = serviceContainer.onViewSourceInScratchpad;
      } else {
        // Point everything else to debugger, if source not available,
        // it will fall back to view-source.
        onFrameClick = serviceContainer.onViewSourceInDebugger;
      }
    }

    // Configure the location.
    const location = dom.span({ className: "message-location devtools-monospace" },
      frame ? FrameView({
        frame,
        onClick: onFrameClick,
        showEmptyPathAsHost: true,
        sourceMapService: serviceContainer ? serviceContainer.sourceMapService : undefined
      }) : null
    );

    let learnMore;
    if (exceptionDocURL) {
      learnMore = dom.a({
        className: "learn-more-link webconsole-learn-more-link",
        title: exceptionDocURL.split("?")[0],
        onClick: this.onLearnMoreClick,
      }, `[${l10n.getStr("webConsoleMoreInfoLabel")}]`);
    }

    return dom.div({
      className: topLevelClasses.join(" "),
      onContextMenu: this.onContextMenu,
      ref: node => {
        this.messageNode = node;
      }
    },
      timestampEl,
      MessageIndent({indent}),
      icon,
      collapse,
      dom.span({ className: "message-body-wrapper" },
        dom.span({ className: "message-flex-body" },
          dom.span({ className: "message-body devtools-monospace" },
            messageBody,
            learnMore
          ),
          repeat,
          location
        ),
        attachment
      )
    );
  }
});

module.exports = Message;
