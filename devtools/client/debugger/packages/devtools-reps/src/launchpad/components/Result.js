/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const React = require("react");
const { Component } = React;
const PropTypes = require("prop-types");
const dom = require("react-dom-factories");
const { MODE } = require("../../reps/constants");
const { ObjectInspector } = require("../../index").objectInspector;
const { Rep } = require("../../reps/rep");

class Result extends Component {
  static get propTypes() {
    return {
      expression: PropTypes.object.isRequired,
      showResultPacket: PropTypes.func.isRequired,
      hideResultPacket: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.copyPacketToClipboard = this.copyPacketToClipboard.bind(this);
    this.onHeaderClick = this.onHeaderClick.bind(this);
    this.renderRepInAllModes = this.renderRepInAllModes.bind(this);
    this.renderRep = this.renderRep.bind(this);
    this.renderPacket = this.renderPacket.bind(this);
  }

  copyPacketToClipboard(e, packet) {
    e.stopPropagation();

    const textField = document.createElement("textarea");
    // eslint-disable-next-line no-unsanitized/property
    textField.innerHTML = JSON.stringify(packet, null, "  ");
    document.body.appendChild(textField);
    textField.select();
    document.execCommand("copy");
    textField.remove();
  }

  onHeaderClick() {
    const { expression } = this.props;
    if (expression.showPacket === true) {
      this.props.hideResultPacket();
    } else {
      this.props.showResultPacket();
    }
  }

  renderRepInAllModes({ object }) {
    return Object.keys(MODE).map(modeKey =>
      this.renderRep({ object, modeKey })
    );
  }

  renderRep({ object, modeKey }) {
    const path = Symbol(modeKey + object.actor);

    return dom.div(
      {
        className: "rep-element",
        key: path.toString(),
        "data-mode": modeKey,
      },
      ObjectInspector({
        roots: [
          {
            path,
            contents: {
              value: object,
            },
          },
        ],
        autoExpandDepth: 0,
        mode: MODE[modeKey],
        // The following properties are optional function props called by the
        // objectInspector on some occasions. Here we pass dull functions that
        // only logs the parameters with which the callback was called.
        onCmdCtrlClick: (node, { depth, event, focused, expanded }) =>
          console.log("CmdCtrlClick", {
            node,
            depth,
            event,
            focused,
            expanded,
          }),
        onInspectIconClick: nodeFront =>
          console.log("inspectIcon click", { nodeFront }),
        onViewSourceInDebugger: location =>
          console.log("onViewSourceInDebugger", { location }),
        recordTelemetryEvent: (eventName, extra = {}) => {
          console.log("📊", eventName, "📊", extra);
        },
      })
    );
  }

  renderPacket(expression) {
    const { packet, showPacket } = expression;
    const headerClassName = showPacket ? "packet-expanded" : "packet-collapsed";
    const headerLabel = showPacket
      ? "Hide expression packet"
      : "Show expression packet";

    return dom.div(
      { className: "packet" },
      dom.header(
        {
          className: headerClassName,
          onClick: this.onHeaderClick,
        },
        headerLabel,
        dom.span({ className: "copy-label" }, "Copy"),
        dom.button(
          {
            className: "copy-packet-button",
            onClick: e => this.copyPacketToClipboard(e, packet.result),
          },
          "grip"
        ),
        dom.button(
          {
            className: "copy-packet-button",
            onClick: e => this.copyPacketToClipboard(e, packet),
          },
          "packet"
        )
      ),
      ...(showPacket
        ? Object.keys(packet).map(k =>
            dom.div(
              { className: "packet-rep" },
              `${k}: `,
              Rep({ object: packet[k], noGrip: true, mode: MODE.LONG })
            )
          )
        : [])
    );
  }

  render() {
    const { expression } = this.props;
    const { input, packet } = expression;
    return dom.div(
      { className: "rep-row" },
      dom.div({ className: "rep-input" }, input),
      dom.div(
        { className: "reps" },
        this.renderRepInAllModes({
          object: packet.exception || packet.result,
        })
      ),
      this.renderPacket(expression)
    );
  }
}

module.exports = Result;
