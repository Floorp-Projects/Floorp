/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const ToolboxToolbar = createFactory(
  require("devtools/client/framework/components/ToolboxToolbar")
);
const ELEMENT_PICKER_ID = "command-button-pick";

/**
 * This component serves as a state controller for the toolbox React component. It's a
 * thin layer for translating events and state of the outside world into the React update
 * cycle. This solution was used to keep the amount of code changes to a minimimum while
 * adapting the existing codebase to start using React.
 */
class ToolboxController extends Component {
  constructor(props, context) {
    super(props, context);

    // See the ToolboxToolbar propTypes for documentation on each of these items in
    // state, and for the definitions of the props that are expected to be passed in.
    this.state = {
      focusedButton: ELEMENT_PICKER_ID,
      toolboxButtons: [],
      visibleToolboxButtonCount: 0,
      currentToolId: null,
      highlightedTools: new Set(),
      panelDefinitions: [],
      hostTypes: [],
      currentHostType: undefined,
      areDockOptionsEnabled: true,
      canCloseToolbox: true,
      isSplitConsoleActive: false,
      disableAutohide: undefined,
      canRender: false,
      buttonIds: [],
      checkedButtonsUpdated: () => {
        this.forceUpdate();
      },
    };

    this.setFocusedButton = this.setFocusedButton.bind(this);
    this.setToolboxButtons = this.setToolboxButtons.bind(this);
    this.setCurrentToolId = this.setCurrentToolId.bind(this);
    this.highlightTool = this.highlightTool.bind(this);
    this.unhighlightTool = this.unhighlightTool.bind(this);
    this.setHostTypes = this.setHostTypes.bind(this);
    this.setCurrentHostType = this.setCurrentHostType.bind(this);
    this.setDockOptionsEnabled = this.setDockOptionsEnabled.bind(this);
    this.setCanCloseToolbox = this.setCanCloseToolbox.bind(this);
    this.setIsSplitConsoleActive = this.setIsSplitConsoleActive.bind(this);
    this.setDisableAutohide = this.setDisableAutohide.bind(this);
    this.setCanRender = this.setCanRender.bind(this);
    this.setPanelDefinitions = this.setPanelDefinitions.bind(this);
    this.updateButtonIds = this.updateButtonIds.bind(this);
    this.updateFocusedButton = this.updateFocusedButton.bind(this);
    this.setDebugTargetData = this.setDebugTargetData.bind(this);
  }

  shouldComponentUpdate() {
    return this.state.canRender;
  }

  componentWillUnmount() {
    this.state.toolboxButtons.forEach(button => {
      button.off("updatechecked", this.state.checkedButtonsUpdated);
    });
  }

  /**
   * The button and tab ids must be known in order to be able to focus left and right
   * using the arrow keys.
   */
  updateButtonIds() {
    const { toolboxButtons, panelDefinitions, canCloseToolbox } = this.state;

    // This is a little gnarly, but go through all of the state and extract the IDs.
    this.setState({
      buttonIds: [
        ...toolboxButtons
          .filter(btn => btn.isInStartContainer)
          .map(({ id }) => id),
        ...panelDefinitions.map(({ id }) => id),
        ...toolboxButtons
          .filter(btn => !btn.isInStartContainer)
          .map(({ id }) => id),
        canCloseToolbox ? "toolbox-close" : null,
      ].filter(id => id),
    });

    this.updateFocusedButton();
  }

  updateFocusedButton() {
    this.setFocusedButton(this.state.focusedButton);
  }

  setFocusedButton(focusedButton) {
    const { buttonIds } = this.state;

    focusedButton =
      focusedButton && buttonIds.includes(focusedButton)
        ? focusedButton
        : buttonIds[0];
    if (this.state.focusedButton !== focusedButton) {
      this.setState({
        focusedButton,
      });
    }
  }

  setCurrentToolId(currentToolId) {
    this.setState({ currentToolId }, () => {
      // Also set the currently focused button to this tool.
      this.setFocusedButton(currentToolId);
    });
  }

  setCanRender() {
    this.setState({ canRender: true }, this.updateButtonIds);
  }

  isToolHighlighted(toolID) {
    return this.state.highlightedTools.has(toolID);
  }

  highlightTool(highlightedTool) {
    const { highlightedTools } = this.state;
    highlightedTools.add(highlightedTool);
    this.setState({ highlightedTools });
  }

  unhighlightTool(id) {
    const { highlightedTools } = this.state;
    if (highlightedTools.has(id)) {
      highlightedTools.delete(id);
      this.setState({ highlightedTools });
    }
  }

  setDockOptionsEnabled(areDockOptionsEnabled) {
    this.setState({ areDockOptionsEnabled });
  }

  setHostTypes(hostTypes) {
    this.setState({ hostTypes });
  }

  setCurrentHostType(currentHostType) {
    this.setState({ currentHostType });
  }

  setCanCloseToolbox(canCloseToolbox) {
    this.setState({ canCloseToolbox }, this.updateButtonIds);
  }

  setIsSplitConsoleActive(isSplitConsoleActive) {
    this.setState({ isSplitConsoleActive });
  }

  setDisableAutohide(disableAutohide) {
    this.setState({ disableAutohide });
  }

  setPanelDefinitions(panelDefinitions) {
    this.setState({ panelDefinitions }, this.updateButtonIds);
  }

  get panelDefinitions() {
    return this.state.panelDefinitions;
  }

  setToolboxButtons(toolboxButtons) {
    // Listen for updates of the checked attribute.
    this.state.toolboxButtons.forEach(button => {
      button.off("updatechecked", this.state.checkedButtonsUpdated);
    });
    toolboxButtons.forEach(button => {
      button.on("updatechecked", this.state.checkedButtonsUpdated);
    });

    const visibleToolboxButtonCount = toolboxButtons.filter(
      button => button.isVisible
    ).length;

    this.setState(
      { toolboxButtons, visibleToolboxButtonCount },
      this.updateButtonIds
    );
  }

  setDebugTargetData(data) {
    this.setState({ debugTargetData: data });
  }

  render() {
    return ToolboxToolbar(Object.assign({}, this.props, this.state));
  }
}

module.exports = ToolboxController;
