/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {DOM, createClass, createFactory, PropTypes} = require("devtools/client/shared/vendor/react");
const {div, button} = DOM;
const ToolboxTab = createFactory(require("devtools/client/framework/components/toolbox-tab"));

/**
 * This is the overall component for the toolbox toolbar. It is designed to not know how
 * the state is being managed, and attempts to be as pure as possible. The
 * ToolboxController component controls the changing state, and passes in everything as
 * props.
 */
module.exports = createClass({
  displayName: "ToolboxToolbar",

  propTypes: {
    // The currently focused item (for arrow keyboard navigation)
    // This ID determines the tabindex being 0 or -1.
    focusedButton: PropTypes.string,
    // List of command button definitions.
    toolboxButtons: PropTypes.array,
    // The id of the currently selected tool, e.g. "inspector"
    currentToolId: PropTypes.string,
    // An optionally highlighted tool, e.g. "inspector"
    highlightedTool: PropTypes.string,
    // List of tool panel definitions.
    panelDefinitions: PropTypes.array,
    // Function to select a tool based on its id.
    selectTool: PropTypes.func,
    // Keep a record of what button is focused.
    focusButton: PropTypes.func,
    // The options button definition.
    optionsPanel: PropTypes.object,
    // Hold off displaying the toolbar until enough information is ready for it to render
    // nicely.
    canRender: PropTypes.bool,
    // Localization interface.
    L10N: PropTypes.object,
  },

  /**
   * The render function is kept fairly short for maintainability. See the individual
   * render functions for how each of the sections is rendered.
   */
  render() {
    const containerProps = {className: "devtools-tabbar"};
    return this.props.canRender
      ? (
        div(
          containerProps,
          renderToolboxButtonsStart(this.props),
          renderTabs(this.props),
          renderToolboxButtonsEnd(this.props),
          renderOptions(this.props),
          renderSeparator(),
          renderDockButtons(this.props)
        )
      )
      : div(containerProps);
  }
});

/**
 * Render all of the tabs, this takes in the panel definitions and builds out
 * the buttons for each of them.
 *
 * @param {Array}    panelDefinitions - Array of objects that define panels.
 * @param {String}   currentToolId - The currently selected tool's id; e.g. "inspector".
 * @param {String}   highlightedTool - If a tool is highlighted, this is it's id.
 * @param {Function} selectTool - Function to select a tool in the toolbox.
 * @param {String}   focusedButton - The id of the focused button.
 * @param {Function} focusButton - Keep a record of the currently focused button.
 */
function renderTabs({panelDefinitions, currentToolId, highlightedTool, selectTool,
                     focusedButton, focusButton}) {
  // A wrapper is needed to get flex sizing correct in XUL.
  return div({className: "toolbox-tabs-wrapper"},
    div({className: "toolbox-tabs"},
      ...panelDefinitions.map(panelDefinition => ToolboxTab({
        panelDefinition,
        currentToolId,
        highlightedTool,
        selectTool,
        focusedButton,
        focusButton,
      }))
    )
  );
}

/**
 * A little helper function to call renderToolboxButtons for buttons at the start
 * of the toolbox.
 */
function renderToolboxButtonsStart(props) {
  return renderToolboxButtons(props, true);
}

/**
* A little helper function to call renderToolboxButtons for buttons at the end
* of the toolbox.
 */
function renderToolboxButtonsEnd(props) {
  return renderToolboxButtons(props, false);
}

/**
 * Render all of the tabs, this takes in a list of toolbox button states. These are plain
 * objects that have all of the relevant information needed to render the button.
 * See Toolbox.prototype._createButtonState in devtools/client/framework/toolbox.js for
 * documentation on this object.
 *
 * @param {Array} toolboxButtons - Array of objects that define the command buttons.
 * @param {String} focusedButton - The id of the focused button.
 * @param {Function} focusButton - Keep a record of the currently focused button.
 * @param {boolean} isStart - Render either the starting buttons, or ending buttons.
 */
function renderToolboxButtons({toolboxButtons, focusedButton, focusButton}, isStart) {
  const visibleButtons = toolboxButtons.filter(command => {
    const {isVisible, isInStartContainer} = command;
    return isVisible && (isStart ? isInStartContainer : !isInStartContainer);
  });

  if (visibleButtons.length === 0) {
    return null;
  }

  return div({id: `toolbox-buttons-${isStart ? "start" : "end"}`},
    ...visibleButtons.map(command => {
      const {id, description, onClick, isChecked, className: buttonClass} = command;
      return button({
        id,
        title: description,
        className: (
          "command-button command-button-invertable devtools-button "
          + buttonClass + (isChecked ? " checked" : "")
        ),
        onClick: (event) => {
          onClick(event);
          focusButton(id);
        },
        onFocus: () => focusButton(id),
        tabIndex: id === focusedButton ? "0" : "-1"
      });
    })
  );
}

/**
 * The options button is a ToolboxTab just like in the renderTabs() function. However
 * it is separate from the normal tabs, so deal with it separately here.
 *
 * @param {Object}   optionsPanel - A single panel definition for the options panel.
 * @param {String}   currentToolId - The currently selected tool's id; e.g. "inspector".
 * @param {Function} selectTool - Function to select a tool in the toolbox.
 * @param {String}   focusedButton - The id of the focused button.
 * @param {Function} focusButton - Keep a record of the currently focused button.
 */
function renderOptions({optionsPanel, currentToolId, selectTool, focusedButton,
                        focusButton}) {
  return div({id: "toolbox-option-container"}, ToolboxTab({
    panelDefinition: optionsPanel,
    currentToolId,
    selectTool,
    focusedButton,
    focusButton,
  }));
}

/**
 * Render a separator.
 */
function renderSeparator() {
  return div({
    id: "toolbox-controls-separator",
    className: "devtools-separator"
  });
}

/**
 * Render the dock buttons, and handle all the cases for what type of host the toolbox
 * is attached to. The following props are expected.
 *
 * @property {String} focusedButton - The id of the focused button.
 * @property {Function} closeToolbox - Completely close the toolbox.
 * @property {Array} hostTypes - Array of host type objects, containing:
 *                   @property {String} position - Position name
 *                   @property {Function} switchHost - Function to switch the host.
 * @property {Function} focusButton - Keep a record of the currently focused button.
 * @property {Object} L10N - Localization interface.
 * @property {Boolean} areDockButtonsEnabled - They are not enabled in certain situations
 *                                             like when they are in the WebIDE.
 * @property {Boolean} canCloseToolbox - Are the tools in a context where they can be
 *                                       closed? This is not always the case, e.g. in the
 *                                       WebIDE.
 */
function renderDockButtons(props) {
  const {
    focusedButton,
    closeToolbox,
    hostTypes,
    focusButton,
    L10N,
    areDockButtonsEnabled,
    canCloseToolbox,
  } = props;

  let buttons = [];

  if (areDockButtonsEnabled) {
    hostTypes.forEach(hostType => {
      const id = "toolbox-dock-" + hostType.position;
      buttons.push(button({
        id,
        onFocus: () => focusButton(id),
        className: "toolbox-dock-button devtools-button",
        title: L10N.getStr(`toolboxDockButtons.${hostType.position}.tooltip`),
        onClick: e => {
          hostType.switchHost();
          focusButton(id);
        },
        tabIndex: focusedButton === id ? "0" : "-1",
      }));
    });
  }

  const closeButtonId = "toolbox-close";

  const closeButton = canCloseToolbox
    ? button({
      id: closeButtonId,
      onFocus: () => focusButton(closeButtonId),
      className: "devtools-button",
      title: L10N.getStr("toolbox.closebutton.tooltip"),
      onClick: () => {
        closeToolbox();
        focusButton(closeButtonId);
      },
      tabIndex: focusedButton === "toolbox-close" ? "0" : "-1",
    })
    : null;

  return div({id: "toolbox-controls"},
    div({id: "toolbox-dock-buttons"}, ...buttons),
    closeButton
  );
}
