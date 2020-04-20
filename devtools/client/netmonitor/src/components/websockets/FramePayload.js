/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { div } = dom;
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");

const Services = require("Services");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n.js");
const {
  getFramePayload,
  getResponseHeader,
  isJSON,
} = require("devtools/client/netmonitor/src/utils/request-utils.js");
const {
  getFormattedSize,
} = require("devtools/client/netmonitor/src/utils/format-utils.js");
const MESSAGE_DATA_LIMIT = Services.prefs.getIntPref(
  "devtools.netmonitor.ws.messageDataLimit"
);
const MESSAGE_DATA_TRUNCATED = L10N.getStr("messageDataTruncated");
const SocketIODecoder = require("devtools/client/netmonitor/src/components/websockets/parsers/socket-io/index.js");
const {
  JsonHubProtocol,
  HandshakeProtocol,
} = require("devtools/client/netmonitor/src/components/websockets/parsers/signalr/index.js");
const {
  parseSockJS,
} = require("devtools/client/netmonitor/src/components/websockets/parsers/sockjs/index.js");
const {
  wampSerializers,
} = require("devtools/client/netmonitor/src/components/websockets/parsers/wamp/serializers.js");
const {
  getRequestByChannelId,
} = require("devtools/client/netmonitor/src/selectors/index");

// Components
const Accordion = createFactory(
  require("devtools/client/shared/components/Accordion")
);
const RawData = createFactory(
  require("devtools/client/netmonitor/src/components/websockets/RawData")
);
loader.lazyGetter(this, "PropertiesView", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-details/PropertiesView")
  );
});

/**
 * Shows the full payload of a WebSocket frame.
 * The payload is unwrapped from the LongStringActor object.
 */
class FramePayload extends Component {
  static get propTypes() {
    return {
      connector: PropTypes.object.isRequired,
      selectedFrame: PropTypes.object,
      request: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      payload: "",
      isFormattedData: false,
      formattedData: {},
      formattedDataTitle: "",
    };
  }

  componentDidMount() {
    this.updateFramePayload();
  }

  componentDidUpdate(prevProps) {
    if (this.props.selectedFrame !== prevProps.selectedFrame) {
      this.updateFramePayload();
    }
  }

  updateFramePayload() {
    const { selectedFrame, connector } = this.props;

    getFramePayload(selectedFrame.payload, connector.getLongString).then(
      async payload => {
        const { formattedData, formattedDataTitle } = await this.parsePayload(
          payload
        );
        this.setState({
          payload,
          isFormattedData: !!formattedData,
          formattedData,
          formattedDataTitle,
        });
      }
    );
  }

  async parsePayload(payload) {
    const { connector, selectedFrame, request } = this.props;

    // Don't apply formatting to control frames
    // Control frame check can be done using opCode as specified here:
    // https://tools.ietf.org/html/rfc6455
    const controlFrames = [0x8, 0x9, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf];
    const isControlFrame = controlFrames.includes(selectedFrame.opCode);
    if (isControlFrame) {
      return {
        formattedData: null,
        formattedDataTitle: "",
      };
    }

    // Make sure that request headers are fetched from the backend before
    // looking for `Sec-WebSocket-Protocol` header.
    const responseHeaders = await connector.requestData(
      request.id,
      "responseHeaders"
    );

    const wsProtocol = getResponseHeader(
      { responseHeaders },
      "Sec-WebSocket-Protocol"
    );

    const wampSerializer = wampSerializers[wsProtocol];
    if (wampSerializer) {
      const wampPayload = wampSerializer.deserializeMessage(payload);

      return {
        formattedData: wampPayload,
        formattedDataTitle: wampSerializer.description,
      };
    }

    // socket.io payload
    const socketIOPayload = this.parseSocketIOPayload(payload);

    if (socketIOPayload) {
      return {
        formattedData: socketIOPayload,
        formattedDataTitle: "Socket.IO",
      };
    }
    // sockjs payload
    const sockJSPayload = parseSockJS(payload);
    if (sockJSPayload) {
      return {
        formattedData: sockJSPayload,
        formattedDataTitle: "SockJS",
      };
    }
    // signalr payload
    const signalRPayload = this.parseSignalR(payload);
    if (signalRPayload) {
      return {
        formattedData: signalRPayload,
        formattedDataTitle: "SignalR",
      };
    }

    // json payload
    const { json } = isJSON(payload);
    if (json) {
      const actionCablePayload = this.parseActionCable(json);
      if (actionCablePayload) {
        return {
          formattedData: actionCablePayload,
          formattedDataTitle: "Action Cable",
        };
      }
      return {
        formattedData: json,
        formattedDataTitle: "JSON",
      };
    }
    return {
      formattedData: null,
      formattedDataTitle: "",
    };
  }

  parseSocketIOPayload(payload) {
    let result;
    // Try decoding socket.io frames
    try {
      const decoder = new SocketIODecoder();
      decoder.on("decoded", decodedPacket => {
        if (
          decodedPacket &&
          !decodedPacket.data.includes("parser error") &&
          decodedPacket.type
        ) {
          result = decodedPacket;
        }
      });
      decoder.add(payload);
      return result;
    } catch (err) {
      // Ignore errors
    }
    return null;
  }

  parseSignalR(payload) {
    // attempt to parse as HandshakeResponseMessage
    let decoder;
    try {
      decoder = new HandshakeProtocol();
      const [remainingData, responseMessage] = decoder.parseHandshakeResponse(
        payload
      );

      if (responseMessage) {
        return {
          handshakeResponse: responseMessage,
          remainingData: this.parseSignalR(remainingData),
        };
      }
    } catch (err) {
      // ignore errors;
    }

    // attempt to parse as JsonHubProtocolMessage
    try {
      decoder = new JsonHubProtocol();
      const msgs = decoder.parseMessages(payload, null);
      if (msgs?.length) {
        return msgs;
      }
    } catch (err) {
      // ignore errors;
    }

    // MVP Signalr
    if (payload.endsWith("\u001e")) {
      const { json } = isJSON(payload.slice(0, -1));
      if (json) {
        return json;
      }
    }

    return null;
  }

  parseActionCable(payload) {
    const identifier = payload.identifier && isJSON(payload.identifier).json;
    const data = payload.data && isJSON(payload.data).json;
    if (!data && !identifier) {
      return null;
    }
    if (identifier) {
      payload.identifier = identifier;
    }
    if (data) {
      payload.data = data;
    }
    return payload;
  }

  render() {
    let { payload } = this.state;
    let isTruncated = false;
    if (this.state.payload.length >= MESSAGE_DATA_LIMIT) {
      payload = payload.substring(0, MESSAGE_DATA_LIMIT);
      isTruncated = true;
    }

    const items = [
      {
        className: "rawData",
        component: RawData,
        componentProps: { payload },
        header: L10N.getFormatStrWithNumbers(
          "netmonitor.ws.rawData.header",
          getFormattedSize(this.state.payload.length)
        ),
        id: "ws-frame-rawData",
        opened: true,
      },
    ];
    if (!isTruncated && this.state.isFormattedData) {
      /**
       * Push the JSON section (formatted data) at the begging of the array
       * before the raw data section. Note that the JSON section will be
       * auto-expanded while the raw data auto-collapsed.
       */
      items.unshift({
        className: "formattedData",
        component: PropertiesView,
        componentProps: {
          object: this.state.formattedData,
        },
        header: this.state.formattedDataTitle,
        id: "ws-frame-formattedData",
        opened: true,
      });
    }

    return div(
      {
        className: "ws-frame-payload",
      },
      isTruncated &&
        div(
          {
            className: "truncated-data-message",
          },
          MESSAGE_DATA_TRUNCATED
        ),
      Accordion({
        items,
      })
    );
  }
}

module.exports = connect(state => ({
  request: getRequestByChannelId(state, state.webSockets.currentChannelId),
}))(FramePayload);
