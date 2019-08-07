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
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n.js");
const {
  getFramePayload,
  isJSON,
} = require("devtools/client/netmonitor/src/utils/request-utils.js");
const {
  getFormattedSize,
} = require("devtools/client/netmonitor/src/utils/format-utils.js");

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
        const { json } = isJSON(payload);
        this.setState({
          payload,
          isFormattedData: !!json,
          formattedData: json,
        });
      }
    );
  }

  render() {
    const items = [
      {
        className: "rawData",
        component: RawData({ payload: this.state.payload }),
        header: L10N.getFormatStrWithNumbers(
          "netmonitor.ws.rawData.header",
          getFormattedSize(this.state.payload.length)
        ),
        labelledby: "ws-frame-rawData-header",
        opened: true,
      },
    ];
    if (this.state.isFormattedData) {
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
        header: `JSON (${getFormattedSize(this.state.payload.length)})`,
        labelledby: "ws-frame-formattedData-header",
        opened: true,
      });
    }

    return div(
      {
        className: "ws-frame-payload",
      },
      Accordion({
        items,
      })
    );
  }
}

module.exports = FramePayload;
