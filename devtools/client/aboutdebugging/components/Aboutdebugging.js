/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { createFactory, Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const Services = require("Services");

const PanelMenu = createFactory(require("./PanelMenu"));

loader.lazyGetter(this, "AddonsPanel",
  () => createFactory(require("./addons/Panel")));
loader.lazyGetter(this, "TabsPanel",
  () => createFactory(require("./tabs/Panel")));
loader.lazyGetter(this, "WorkersPanel",
  () => createFactory(require("./workers/Panel")));

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/debugger-client", true);
loader.lazyRequireGetter(this, "Telemetry",
  "devtools/client/shared/telemetry");

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

const panels = [{
  id: "addons",
  name: Strings.GetStringFromName("addons"),
  icon: "chrome://devtools/skin/images/debugging-addons.svg",
  component: AddonsPanel
}, {
  id: "tabs",
  name: Strings.GetStringFromName("tabs"),
  icon: "chrome://devtools/skin/images/debugging-tabs.svg",
  component: TabsPanel
}, {
  id: "workers",
  name: Strings.GetStringFromName("workers"),
  icon: "chrome://devtools/skin/images/debugging-workers.svg",
  component: WorkersPanel
}];

const defaultPanelId = "addons";

class AboutDebuggingApp extends Component {
  static get propTypes() {
    return {
      client: PropTypes.instanceOf(DebuggerClient).isRequired,
      connect: PropTypes.object.isRequired,
      telemetry: PropTypes.instanceOf(Telemetry).isRequired
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      selectedPanelId: defaultPanelId
    };

    this.onHashChange = this.onHashChange.bind(this);
    this.selectPanel = this.selectPanel.bind(this);
  }

  componentDidMount() {
    window.addEventListener("hashchange", this.onHashChange);
    this.onHashChange();
    this.props.telemetry.toolOpened("aboutdebugging");
  }

  componentWillUnmount() {
    window.removeEventListener("hashchange", this.onHashChange);
    this.props.telemetry.toolClosed("aboutdebugging");
  }

  onHashChange() {
    this.setState({
      selectedPanelId: window.location.hash.substr(1) || defaultPanelId
    });
  }

  selectPanel(panelId) {
    window.location.hash = "#" + panelId;
  }

  render() {
    const { client, connect } = this.props;
    const { selectedPanelId } = this.state;
    const selectPanel = this.selectPanel;
    const selectedPanel = panels.find(p => p.id == selectedPanelId);
    let panel;

    if (selectedPanel) {
      panel = selectedPanel.component({ client, connect, id: selectedPanel.id });
    } else {
      panel = (
        dom.div({ className: "error-page" },
          dom.h1({ className: "header-name" },
            Strings.GetStringFromName("pageNotFound")
          ),
          dom.h4({ className: "error-page-details" },
            Strings.formatStringFromName("doesNotExist", [selectedPanelId], 1))
        )
      );
    }

    return dom.div({ className: "app" },
      PanelMenu({ panels, selectedPanelId, selectPanel }),
      dom.div({ className: "main-content" }, panel)
    );
  }
}

module.exports = AboutDebuggingApp;
