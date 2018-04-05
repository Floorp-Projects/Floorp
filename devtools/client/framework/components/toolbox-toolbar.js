/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {div, button} = dom;

const ToolboxTab = createFactory(require("devtools/client/framework/components/toolbox-tab"));
const ToolboxTabs = createFactory(require("devtools/client/framework/components/toolbox-tabs"));

/**
 * This is the overall component for the toolbox toolbar. It is designed to not know how
 * the state is being managed, and attempts to be as pure as possible. The
 * ToolboxController component controls the changing state, and passes in everything as
 * props.
 */
class ToolboxToolbar extends Component {
  static get propTypes() {
    return {
      // The currently focused item (for arrow keyboard navigation)
      // This ID determines the tabindex being 0 or -1.
      focusedButton: PropTypes.string,
      // List of command button definitions.
      toolboxButtons: PropTypes.array,
      // The id of the currently selected tool, e.g. "inspector"
      currentToolId: PropTypes.string,
      // An optionally highlighted tools, e.g. "inspector".
      highlightedTools: PropTypes.instanceOf(Set),
      // List of tool panel definitions (used by ToolboxTabs component).
      panelDefinitions: PropTypes.array,
      // The options panel definition.
      optionsPanel: PropTypes.object,
      // List of possible docking options.
      hostTypes: PropTypes.arrayOf(PropTypes.shape({
        position: PropTypes.string.isRequired,
        switchHost: PropTypes.func.isRequired,
      })),
      // Should the docking options be enabled? They are disabled in some
      // contexts such as WebIDE.
      areDockButtonsEnabled: PropTypes.bool,
      // Do we need to add UI for closing the toolbox? We don't when the
      // toolbox is undocked, for example.
      canCloseToolbox: PropTypes.bool,
      // Function to select a tool based on its id.
      selectTool: PropTypes.func,
      // Function to completely close the toolbox.
      closeToolbox: PropTypes.func,
      // Keep a record of what button is focused.
      focusButton: PropTypes.func,
      // Hold off displaying the toolbar until enough information is ready for
      // it to render nicely.
      canRender: PropTypes.bool,
      // Localization interface.
      L10N: PropTypes.object,
      // The devtools toolbox
      toolbox: PropTypes.object,
    };
  }

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
          ToolboxTabs(this.props),
          renderToolboxButtonsEnd(this.props),
          renderOptions(this.props),
          renderSeparator(),
          renderDockButtons(this.props)
        )
      )
      : div(containerProps);
  }
}

module.exports = ToolboxToolbar;

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
 * @param {String} focusedButton - The id of the focused button.
 * @param {Array} toolboxButtons - Array of objects that define the command buttons.
 * @param {Function} focusButton - Keep a record of the currently focused button.
 * @param {boolean} isStart - Render either the starting buttons, or ending buttons.
 */
function renderToolboxButtons({focusedButton, toolboxButtons, focusButton}, isStart) {
  const visibleButtons = toolboxButtons.filter(command => {
    const {isVisible, isInStartContainer} = command;
    return isVisible && (isStart ? isInStartContainer : !isInStartContainer);
  });

  if (visibleButtons.length === 0) {
    return null;
  }

  return div({id: `toolbox-buttons-${isStart ? "start" : "end"}`},
    ...visibleButtons.map(command => {
      const {
        id,
        description,
        disabled,
        onClick,
        isChecked,
        className: buttonClass,
        onKeyDown
      } = command;
      return button({
        id,
        title: description,
        disabled,
        className: (
          "command-button devtools-button "
          + buttonClass + (isChecked ? " checked" : "")
        ),
        onClick: (event) => {
          onClick(event);
          focusButton(id);
        },
        onFocus: () => focusButton(id),
        tabIndex: id === focusedButton ? "0" : "-1",
        onKeyDown: (event) => {
          onKeyDown(event);
        }
      });
    }),
    isStart ? div({className: "devtools-separator"}) : null
  );
}

/**
 * The options button is a ToolboxTab just like in the ToolboxTabs component.
 * However it is separate from the normal tabs, so deal with it separately here.
 * The following props are expected.
 *
 * @param {string} focusedButton
 *        The id of the focused button.
 * @param {string} currentToolId
 *        The currently selected tool's id; e.g.  "inspector".
 * @param {Object} highlightedTools
 *        Optionally highlighted tools, e.g. "inspector".
 * @param {Object} optionsPanel
 *        A single panel definition for the options panel.
 * @param {Function} selectTool
 *        Function to select a tool in the toolbox.
 * @param {Function} focusButton
 *        Keep a record of the currently focused button.
 */
function renderOptions({focusedButton, currentToolId, highlightedTools,
                        optionsPanel, selectTool, focusButton}) {
  return div({id: "toolbox-option-container"}, ToolboxTab({
    panelDefinition: optionsPanel,
    currentToolId,
    selectTool,
    highlightedTools,
    focusedButton,
    focusButton,
  }));
}

/**
 * Render a separator.
 */
function renderSeparator() {
  return div({className: "devtools-separator"});
}

/**
 * Render the dock buttons, and handle all the cases for what type of host the toolbox
 * is attached to. The following props are expected.
 *
 * @param {string} focusedButton
 *        The id of the focused button.
 * @param {Object[]} hostTypes
 *        Array of host type objects.
 * @param {string} hostTypes[].position
 *        Position name.
 * @param {Function} hostTypes[].switchHost
 *        Function to switch the host.
 * @param {boolean} areDockButtonsEnabled
 *        They are not enabled in certain situations like when they are in the
 *        WebIDE.
 * @param {boolean} canCloseToolbox
 *        Do we need to add UI for closing the toolbox? We don't when the
 *        toolbox is undocked, for example.
 * @param {Function} closeToolbox
 *        Completely close the toolbox.
 * @param {Function} focusButton
 *        Keep a record of the currently focused button.
 * @param {Object} L10N
 *        Localization interface.
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
      },
      tabIndex: focusedButton === "toolbox-close" ? "0" : "-1",
    })
    : null;

  return div({id: "toolbox-controls"},
    div({id: "toolbox-dock-buttons"}, ...buttons),
    closeButton
  );
}
