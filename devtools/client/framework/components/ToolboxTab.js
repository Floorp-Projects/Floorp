/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {img, button, span} = dom;

class ToolboxTab extends Component {
  // See toolbox-toolbar propTypes for details on the props used here.
  static get propTypes() {
    return {
      currentToolId: PropTypes.string,
      focusButton: PropTypes.func,
      focusedButton: PropTypes.string,
      highlightedTools: PropTypes.object.isRequired,
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
    const {panelDefinition, currentToolId, highlightedTools, selectTool,
           focusedButton, focusButton} = this.props;
    const {id, extensionId, tooltip, label, iconOnly} = panelDefinition;
    const isHighlighted = id === currentToolId;

    const className = [
      "devtools-tab",
      currentToolId === id ? "selected" : "",
      highlightedTools.has(id) ? "highlighted" : "",
      iconOnly ? "devtools-tab-icon-only" : ""
    ].join(" ");

    return button(
      {
        className,
        id: `toolbox-tab-${id}`,
        "data-id": id,
        "data-extension-id": extensionId,
        title: tooltip,
        type: "button",
        "aria-pressed": currentToolId === id ? "true" : "false",
        tabIndex: focusedButton === id ? "0" : "-1",
        onFocus: () => focusButton(id),
        onMouseDown: () => selectTool(id, "tab_switch"),
        onKeyDown: (evt) => {
          if (evt.key === "Enter" || evt.key === " ") {
            selectTool(id, "tab_switch");
          }
        },
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
