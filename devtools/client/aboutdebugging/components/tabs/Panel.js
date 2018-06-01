/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const Services = require("Services");

const PanelHeader = createFactory(require("../PanelHeader"));
const TargetList = createFactory(require("../TargetList"));
const TabTarget = createFactory(require("./Target"));

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/debugger-client", true);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

class TabsPanel extends Component {
  static get propTypes() {
    return {
      client: PropTypes.instanceOf(DebuggerClient).isRequired,
      connect: PropTypes.object,
      id: PropTypes.string.isRequired
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      tabs: []
    };

    this.update = this.update.bind(this);
  }

  componentDidMount() {
    const { client } = this.props;
    client.addListener("tabListChanged", this.update);
    this.update();
  }

  componentWillUnmount() {
    const { client } = this.props;
    client.removeListener("tabListChanged", this.update);
  }

  async update() {
    let { tabs } = await this.props.client.mainRoot.listTabs({ favicons: true });

    // Filter out closed tabs (represented as `null`).
    tabs = tabs.filter(tab => !!tab);

    for (const tab of tabs) {
      if (tab.favicon) {
        const base64Favicon = btoa(String.fromCharCode.apply(String, tab.favicon));
        tab.icon = "data:image/png;base64," + base64Favicon;
      } else {
        tab.icon = "chrome://devtools/skin/images/globe.svg";
      }
    }

    this.setState({ tabs });
  }

  render() {
    const { client, connect, id } = this.props;
    const { tabs } = this.state;

    return dom.div({
      id: id + "-panel",
      className: "panel",
      role: "tabpanel",
      "aria-labelledby": id + "-header"
    },
    PanelHeader({
      id: id + "-header",
      name: Strings.GetStringFromName("tabs")
    }),
    dom.div({},
      TargetList({
        client,
        connect,
        id: "tabs",
        name: Strings.GetStringFromName("tabs"),
        sort: false,
        targetClass: TabTarget,
        targets: tabs
      })
    ));
  }
}

module.exports = TabsPanel;
