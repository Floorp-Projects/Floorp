/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getStr } = require("devtools/client/inspector/layout/utils/l10n");

const ComputedProperty = createFactory(require("devtools/client/inspector/layout/components/ComputedProperty"));

class FlexContainerProperties extends PureComponent {
  static get propTypes() {
    return {
      properties: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      // Whether or not the properties are shown.
      isShown: true,
    };

    this.onToggleExpander = this.onToggleExpander.bind(this);
  }

  onToggleExpander(event) {
    event.stopPropagation();

    this.setState((prevState) => {
      return {
        isShown: !prevState.isShown,
      };
    });
  }

  render() {
    const { properties } =  this.props;

    return (
      dom.div(
        {
          id: "flex-container-properties",
          className: "flexbox-container",
        },
        dom.div(
          {
            className: "layout-properties-header",
            onDoubleClick: this.onToggleExpander,
          },
          dom.span(
            {
              className: "layout-properties-expander theme-twisty",
              open: this.state.isShown,
              onClick: this.onToggleExpander,
            }
          ),
          getStr("flexbox.flexContainerProperties")
        ),
        dom.div(
          {
            className: "layout-properties-wrapper devtools-monospace",
            hidden: !this.state.isShown,
            tabIndex: 0,
          },
          Object.entries(properties).map(([name, value]) => ComputedProperty({
            key: name,
            name,
            value,
          }))
        )
      )
    );
  }
}

module.exports = FlexContainerProperties;
