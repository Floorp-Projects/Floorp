/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const ThisFirefox = require("../runtimes/this-firefox");

const Runtime = require("../runtimes/runtime");
const RuntimeItem = createFactory(require("./runtime/RuntimeItem"));

class RuntimesPane extends PureComponent {
  static get propTypes() {
    return {
      selectedRuntime: PropTypes.instanceOf(Runtime),
      thisFirefox: PropTypes.instanceOf(ThisFirefox).isRequired,
    };
  }

  render() {
    const { selectedRuntime, thisFirefox } = this.props;

    return dom.section(
      {
        className: "runtimes-pane",
      },
      dom.ul(
        {},
        RuntimeItem({
          icon: thisFirefox.getIcon(),
          isSelected: thisFirefox === selectedRuntime,
          name: thisFirefox.getName(),
        })
      )
    );
  }
}

module.exports = RuntimesPane;
