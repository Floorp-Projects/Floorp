/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {DOM, Component, PropTypes} = require("devtools/client/shared/vendor/react");
const {img, button, span} = DOM;

class ToolboxTab extends Component {
  // See toolbox-toolbar propTypes for details on the props used here.
  static get propTypes() {
    return {
      currentToolId: PropTypes.string,
      focusButton: PropTypes.func,
      focusedButton: PropTypes.string,
      highlightedTool: PropTypes.string,
      panelDefinition: PropTypes.object,
      selectTool: PropTypes.func,
    };
  }

  constructor(props) {
    super(props);
    this.renderIcon = this.renderIcon.bind(this);
  }

  renderIcon(definition, isHighlighted) {
    const {icon} = definition;
    if (!icon) {
      return [];
    }
    return [
      img({
        src: icon
      }),
    ];
  }

  render() {
    const {panelDefinition, currentToolId, highlightedTool, selectTool,
           focusedButton, focusButton} = this.props;
    const {id, tooltip, label, iconOnly} = panelDefinition;
    const isHighlighted = id === currentToolId;

    const className = [
      "devtools-tab",
      currentToolId === id ? "selected" : "",
      highlightedTool === id ? "highlighted" : "",
      iconOnly ? "devtools-tab-icon-only" : ""
    ].join(" ");

    return button(
      {
        className,
        id: `toolbox-tab-${id}`,
        "data-id": id,
        title: tooltip,
        type: "button",
        tabIndex: focusedButton === id ? "0" : "-1",
        onFocus: () => focusButton(id),
        onMouseDown: () => selectTool(id),
      },
      span(
        {
          className: "devtools-tab-line"
        }
      ),
      ...this.renderIcon(panelDefinition, isHighlighted),
      iconOnly ?
        null :
        span(
          {
            className: "devtools-tab-label"
          },
          label
        )
    );
  }
}

module.exports = ToolboxTab;
