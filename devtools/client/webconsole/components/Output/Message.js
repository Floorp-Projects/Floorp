/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React & Redux
const {
  Component,
  createFactory,
  createElement,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { l10n } = require("devtools/client/webconsole/utils/messages");
const actions = require("devtools/client/webconsole/actions/index");
const {
  MESSAGE_SOURCE,
  MESSAGE_TYPE,
} = require("devtools/client/webconsole/constants");
const {
  MessageIndent,
} = require("devtools/client/webconsole/components/Output/MessageIndent");
const MessageIcon = require("devtools/client/webconsole/components/Output/MessageIcon");
const FrameView = createFactory(
  require("devtools/client/shared/components/Frame")
);

loader.lazyRequireGetter(
  this,
  "CollapseButton",
  "devtools/client/webconsole/components/Output/CollapseButton"
);
loader.lazyRequireGetter(
  this,
  "MessageRepeat",
  "devtools/client/webconsole/components/Output/MessageRepeat"
);
loader.lazyRequireGetter(
  this,
  "PropTypes",
  "devtools/client/shared/vendor/react-prop-types"
);
loader.lazyRequireGetter(
  this,
  "SmartTrace",
  "devtools/client/shared/components/SmartTrace"
);

class Message extends Component {
  static get propTypes() {
    return {
      open: PropTypes.bool,
      collapsible: PropTypes.bool,
      collapseTitle: PropTypes.string,
      onToggle: PropTypes.func,
      source: PropTypes.string.isRequired,
      type: PropTypes.string.isRequired,
      level: PropTypes.string.isRequired,
      indent: PropTypes.number.isRequired,
      inWarningGroup: PropTypes.bool,
      topLevelClasses: PropTypes.array.isRequired,
      messageBody: PropTypes.any.isRequired,
      repeat: PropTypes.any,
      frame: PropTypes.any,
      attachment: PropTypes.any,
      stacktrace: PropTypes.any,
      messageId: PropTypes.string,
      scrollToMessage: PropTypes.bool,
      exceptionDocURL: PropTypes.string,
      request: PropTypes.object,
      dispatch: PropTypes.func,
      timeStamp: PropTypes.number,
      timestampsVisible: PropTypes.bool.isRequired,
      serviceContainer: PropTypes.shape({
        emitForTests: PropTypes.func.isRequired,
        onViewSource: PropTypes.func.isRequired,
        onViewSourceInDebugger: PropTypes.func,
        onViewSourceInStyleEditor: PropTypes.func,
        openContextMenu: PropTypes.func.isRequired,
        openLink: PropTypes.func.isRequired,
        sourceMapService: PropTypes.any,
      }),
      notes: PropTypes.arrayOf(
        PropTypes.shape({
          messageBody: PropTypes.string.isRequired,
          frame: PropTypes.any,
        })
      ),
      maybeScrollToBottom: PropTypes.func,
      message: PropTypes.object.isRequired,
    };
  }

  static get defaultProps() {
    return {
      indent: 0,
    };
  }

  constructor(props) {
    super(props);
    this.onLearnMoreClick = this.onLearnMoreClick.bind(this);
    this.toggleMessage = this.toggleMessage.bind(this);
    this.onContextMenu = this.onContextMenu.bind(this);
    this.renderIcon = this.renderIcon.bind(this);
  }

  componentDidMount() {
    if (this.messageNode) {
      if (this.props.scrollToMessage) {
        this.messageNode.scrollIntoView();
      }

      this.emitNewMessage(this.messageNode);
    }
  }

  componentDidCatch(e) {
    this.setState({ error: e });
  }

  // Event used in tests. Some message types don't pass it in because existing tests
  // did not emit for them.
  emitNewMessage(node) {
    const { serviceContainer, messageId, timeStamp } = this.props;
    serviceContainer.emitForTests(
      "new-messages",
      new Set([{ node, messageId, timeStamp }])
    );
  }

  onLearnMoreClick(e) {
    const { exceptionDocURL } = this.props;
    this.props.serviceContainer.openLink(exceptionDocURL, e);
    e.preventDefault();
  }

  toggleMessage(e) {
    // Don't bubble up to the main App component, which  redirects focus to input,
    // making difficult for screen reader users to review output
    e.stopPropagation();
    const { open, dispatch, messageId, onToggle } = this.props;

    // Early exit the function to avoid the message to collapse if the user is
    // selecting a range in the toggle message.
    const window = e.target.ownerDocument.defaultView;
    if (window.getSelection && window.getSelection().type === "Range") {
      return;
    }

    // If defined on props, we let the onToggle() method handle the toggling,
    // otherwise we toggle the message open/closed ourselves.
    if (onToggle) {
      onToggle(messageId, e);
    } else if (open) {
      dispatch(actions.messageClose(messageId));
    } else {
      dispatch(actions.messageOpen(messageId));
    }
  }

  onContextMenu(e) {
    const { serviceContainer, source, request, messageId } = this.props;
    const messageInfo = {
      source,
      request,
      messageId,
    };
    serviceContainer.openContextMenu(e, messageInfo);
    e.stopPropagation();
    e.preventDefault();
  }

  renderIcon() {
    const { level, inWarningGroup, type } = this.props;

    if (inWarningGroup) {
      return undefined;
    }

    return MessageIcon({
      level,
      type,
    });
  }

  renderTimestamp() {
    if (!this.props.timestampsVisible) {
      return null;
    }

    return dom.span(
      {
        className: "timestamp devtools-monospace",
      },
      l10n.timestampString(this.props.timeStamp || Date.now())
    );
  }

  renderErrorState() {
    const newBugUrl =
      "https://bugzilla.mozilla.org/enter_bug.cgi?product=DevTools&component=Console";
    const timestampEl = this.renderTimestamp();

    return dom.div(
      {
        className: "message error message-did-catch",
      },
      timestampEl,
      MessageIcon({ level: "error" }),
      dom.span(
        { className: "message-body-wrapper" },
        dom.span(
          {
            className: "message-flex-body",
          },
          // Add whitespaces for formatting when copying to the clipboard.
          timestampEl ? " " : null,
          dom.span(
            { className: "message-body devtools-monospace" },
            l10n.getFormatStr("webconsole.message.componentDidCatch.label", [
              newBugUrl,
            ]),
            dom.button(
              {
                className: "devtools-button",
                onClick: () =>
                  navigator.clipboard.writeText(
                    JSON.stringify(
                      this.props.message,
                      function(key, value) {
                        // The message can hold one or multiple fronts that we need to serialize
                        if (value?.getGrip) {
                          return value.getGrip();
                        }
                        return value;
                      },
                      2
                    )
                  ),
              },
              l10n.getStr(
                "webconsole.message.componentDidCatch.copyButton.label"
              )
            )
          )
        )
      ),
      dom.br()
    );
  }

  // eslint-disable-next-line complexity
  render() {
    if (this.state && this.state.error) {
      return this.renderErrorState();
    }

    const {
      open,
      collapsible,
      collapseTitle,
      source,
      type,
      level,
      indent,
      inWarningGroup,
      topLevelClasses,
      messageBody,
      frame,
      stacktrace,
      serviceContainer,
      exceptionDocURL,
      messageId,
      notes,
    } = this.props;

    topLevelClasses.push("message", source, type, level);
    if (open) {
      topLevelClasses.push("open");
    }

    const timestampEl = this.renderTimestamp();
    const icon = this.renderIcon();

    // Figure out if there is an expandable part to the message.
    let attachment = null;
    if (this.props.attachment) {
      attachment = this.props.attachment;
    } else if (stacktrace && open) {
      attachment = dom.div(
        {
          className: "stacktrace devtools-monospace",
        },
        createElement(SmartTrace, {
          stacktrace,
          onViewSourceInDebugger:
            serviceContainer.onViewSourceInDebugger ||
            serviceContainer.onViewSource,
          onViewSource: serviceContainer.onViewSource,
          onReady: this.props.maybeScrollToBottom,
          sourceMapService: serviceContainer.sourceMapService,
        })
      );
    }

    // If there is an expandable part, make it collapsible.
    let collapse = null;
    if (collapsible) {
      collapse = createElement(CollapseButton, {
        open,
        title: collapseTitle,
        onClick: this.toggleMessage,
      });
    }

    let notesNodes;
    if (notes) {
      notesNodes = notes.map(note =>
        dom.span(
          { className: "message-flex-body error-note" },
          dom.span(
            { className: "message-body devtools-monospace" },
            "note: " + note.messageBody
          ),
          dom.span(
            { className: "message-location devtools-monospace" },
            note.frame
              ? FrameView({
                  frame: note.frame,
                  onClick: serviceContainer
                    ? serviceContainer.onViewSourceInDebugger ||
                      serviceContainer.onViewSource
                    : undefined,
                  showEmptyPathAsHost: true,
                  sourceMapService: serviceContainer
                    ? serviceContainer.sourceMapService
                    : undefined,
                })
              : null
          )
        )
      );
    } else {
      notesNodes = [];
    }

    const repeat =
      this.props.repeat && this.props.repeat > 1
        ? createElement(MessageRepeat, { repeat: this.props.repeat })
        : null;

    let onFrameClick;
    if (serviceContainer && frame) {
      if (source === MESSAGE_SOURCE.CSS) {
        onFrameClick =
          serviceContainer.onViewSourceInStyleEditor ||
          serviceContainer.onViewSource;
      } else {
        // Point everything else to debugger, if source not available,
        // it will fall back to view-source.
        onFrameClick =
          serviceContainer.onViewSourceInDebugger ||
          serviceContainer.onViewSource;
      }
    }

    // Configure the location.
    const location = dom.span(
      { className: "message-location devtools-monospace" },
      frame
        ? FrameView({
            frame,
            onClick: onFrameClick,
            showEmptyPathAsHost: true,
            sourceMapService: serviceContainer
              ? serviceContainer.sourceMapService
              : undefined,
            messageSource: source,
          })
        : null
    );

    let learnMore;
    if (exceptionDocURL) {
      learnMore = dom.a(
        {
          className: "learn-more-link webconsole-learn-more-link",
          href: exceptionDocURL,
          title: exceptionDocURL.split("?")[0],
          onClick: this.onLearnMoreClick,
        },
        `[${l10n.getStr("webConsoleMoreInfoLabel")}]`
      );
    }

    const bodyElements = Array.isArray(messageBody)
      ? messageBody
      : [messageBody];

    return dom.div(
      {
        className: topLevelClasses.join(" "),
        onContextMenu: this.onContextMenu,
        ref: node => {
          this.messageNode = node;
        },
        "data-message-id": messageId,
        "aria-live": type === MESSAGE_TYPE.COMMAND ? "off" : "polite",
      },
      timestampEl,
      MessageIndent({
        indent,
        inWarningGroup,
      }),
      icon,
      collapse,
      dom.span(
        { className: "message-body-wrapper" },
        dom.span(
          {
            className: "message-flex-body",
            onClick: collapsible ? this.toggleMessage : undefined,
          },
          // Add whitespaces for formatting when copying to the clipboard.
          timestampEl ? " " : null,
          dom.span(
            { className: "message-body devtools-monospace" },
            ...bodyElements,
            learnMore
          ),
          repeat ? " " : null,
          repeat,
          " ",
          location
        ),
        attachment,
        ...notesNodes
      ),
      // If an attachment is displayed, the final newline is handled by the attachment.
      attachment ? null : dom.br()
    );
  }
}

module.exports = Message;
