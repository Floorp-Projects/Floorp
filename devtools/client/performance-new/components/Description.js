/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @typedef {{}} Props - This is an empty object.
 */

"use strict";

const {
  PureComponent,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const {
  div,
  button,
  p,
} = require("devtools/client/shared/vendor/react-dom-factories");
const Localized = createFactory(
  require("devtools/client/shared/vendor/fluent-react").Localized
);

/**
 * This component provides a helpful description for what is going on in the component
 * and provides some external links.
 * @extends {React.PureComponent<Props>}
 */
class Description extends PureComponent {
  /**
   * @param {Props} props
   */
  constructor(props) {
    super(props);
    this.handleLinkClick = this.handleLinkClick.bind(this);
  }

  /**
   * @param {React.MouseEvent<HTMLButtonElement>} event
   */
  handleLinkClick(event) {
    const { openDocLink } = require("devtools/client/shared/link");

    /** @type HTMLButtonElement */
    const target = /** @type {any} */ (event.target);

    openDocLink(target.value, {});
  }

  render() {
    return div(
      { className: "perf-description" },
      Localized(
        {
          id: "perftools-description-intro",
          a: button({
            className: "perf-external-link",
            onClick: this.handleLinkClick,
            value: "https://profiler.firefox.com",
          }),
        },
        p({})
      )
    );
  }
}

module.exports = Description;
