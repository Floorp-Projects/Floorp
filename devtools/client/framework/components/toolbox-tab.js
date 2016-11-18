/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {DOM, createClass} = require("devtools/client/shared/vendor/react");
const {img, button} = DOM;

module.exports = createClass({
  displayName: "ToolboxTab",

  renderIcon(definition, isHighlighted) {
    const {icon, highlightedicon} = definition;
    if (!icon) {
      return [];
    }
    return [
      img({
        className: "default-icon",
        src: icon
      }),
      img({
        className: "highlighted-icon",
        src: highlightedicon || icon
      })
    ];
  },

  render() {
    const {panelDefinition, currentToolId, highlightedTool, selectTool,
           focusedButton, focusButton} = this.props;
    const {id, tooltip, label, iconOnly} = panelDefinition;
    const isHighlighted = id === currentToolId;

    const className = [
      "devtools-tab",
      panelDefinition.invertIconForLightTheme || panelDefinition.invertIconForDarkTheme
        ? "icon-invertable"
        : "",
      panelDefinition.invertIconForLightTheme ? "icon-invertable-light-theme" : "",
      panelDefinition.invertIconForDarkTheme ? "icon-invertable-dark-theme" : "",
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
        onClick: () => selectTool(id),
      },
      ...this.renderIcon(panelDefinition, isHighlighted),
      iconOnly ? null : label
    );
  }
});
