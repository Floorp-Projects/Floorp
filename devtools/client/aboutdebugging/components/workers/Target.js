/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { Component, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");
const { debugWorker } = require("../../modules/worker");
const Services = require("Services");

loader.lazyRequireGetter(this, "DebuggerClient",
  "devtools/shared/client/debugger-client", true);

const Strings = Services.strings.createBundle(
  "chrome://devtools/locale/aboutdebugging.properties");

class WorkerTarget extends Component {
  static get propTypes() {
    return {
      client: PropTypes.instanceOf(DebuggerClient).isRequired,
      debugDisabled: PropTypes.bool,
      target: PropTypes.shape({
        icon: PropTypes.string,
        name: PropTypes.string.isRequired,
        workerActor: PropTypes.string
      }).isRequired
    };
  }

  constructor(props) {
    super(props);
    this.debug = this.debug.bind(this);
  }

  debug() {
    let { client, target } = this.props;
    debugWorker(client, target.workerActor);
  }

  render() {
    let { target, debugDisabled } = this.props;

    return dom.li({ className: "target-container" },
      dom.img({
        className: "target-icon",
        role: "presentation",
        src: target.icon
      }),
      dom.div({ className: "target" },
        dom.div({ className: "target-name", title: target.name }, target.name)
      ),
      dom.button({
        className: "debug-button",
        onClick: this.debug,
        disabled: debugDisabled
      }, Strings.GetStringFromName("debug"))
    );
  }
}

module.exports = WorkerTarget;
