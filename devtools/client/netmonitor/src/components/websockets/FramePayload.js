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
const Services = require("Services");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n.js");
const {
  getFramePayload,
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

// Components
const Accordion = createFactory(
  require("devtools/client/shared/components/Accordion")
);
const RawData = createFactory(require("./RawData"));
loader.lazyGetter(this, "JSONPreview", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/JSONPreview")
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
      payload => {
        const { formattedData, formattedDataTitle } = this.parsePayload(
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

  parsePayload(payload) {
    // socket.io payload
    const socketIOPayload = this.parseSocketIOPayload(payload);
    if (socketIOPayload) {
      return {
        formattedData: socketIOPayload,
        formattedDataTitle: "Socket.IO",
      };
    }
    // json payload
    const { json } = isJSON(payload);
    if (json) {
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

  render() {
    let payload = this.state.payload;
    let isTruncated = false;
    if (this.state.payload.length >= MESSAGE_DATA_LIMIT) {
      payload = payload.substring(0, MESSAGE_DATA_LIMIT);
      isTruncated = true;
    }

    const items = [
      {
        className: "rawData",
        component: RawData({
          payload,
        }),
        header: L10N.getFormatStrWithNumbers(
          "netmonitor.ws.rawData.header",
          getFormattedSize(this.state.payload.length)
        ),
        labelledby: "ws-frame-rawData-header",
        opened: true,
      },
    ];
    if (!isTruncated && this.state.isFormattedData) {
      items.push({
        className: "formattedData",
        component: JSONPreview({
          object: this.state.formattedData,
          columns: [
            {
              id: "value",
              width: "100%",
            },
          ],
        }),
        header: `${this.state.formattedDataTitle} (${getFormattedSize(
          this.state.payload.length
        )})`,
        labelledby: "ws-frame-formattedData-header",
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

module.exports = FramePayload;
