/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Component, createFactory} = require("devtools/client/shared/vendor/react");
const ToolboxToolbar = createFactory(require("devtools/client/framework/components/toolbox-toolbar"));
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
      currentToolId: null,
      canRender: false,
      highlightedTool: "",
      areDockButtonsEnabled: true,
      panelDefinitions: [],
      hostTypes: [],
      canCloseToolbox: true,
      toolboxButtons: [],
      buttonIds: [],
      checkedButtonsUpdated: () => {
        this.forceUpdate();
      }
    };

    this.updateButtonIds = this.updateButtonIds.bind(this);
    this.updateFocusedButton = this.updateFocusedButton.bind(this);
    this.setFocusedButton = this.setFocusedButton.bind(this);
    this.setCurrentToolId = this.setCurrentToolId.bind(this);
    this.setCanRender = this.setCanRender.bind(this);
    this.setOptionsPanel = this.setOptionsPanel.bind(this);
    this.highlightTool = this.highlightTool.bind(this);
    this.unhighlightTool = this.unhighlightTool.bind(this);
    this.setDockButtonsEnabled = this.setDockButtonsEnabled.bind(this);
    this.setHostTypes = this.setHostTypes.bind(this);
    this.setCanCloseToolbox = this.setCanCloseToolbox.bind(this);
    this.setPanelDefinitions = this.setPanelDefinitions.bind(this);
    this.setToolboxButtons = this.setToolboxButtons.bind(this);
    this.setCanMinimize = this.setCanMinimize.bind(this);
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
    const {panelDefinitions, toolboxButtons, optionsPanel, hostTypes,
           canCloseToolbox} = this.state;

    // This is a little gnarly, but go through all of the state and extract the IDs.
    this.setState({
      buttonIds: [
        ...toolboxButtons.filter(btn => btn.isInStartContainer).map(({id}) => id),
        ...panelDefinitions.map(({id}) => id),
        ...toolboxButtons.filter(btn => !btn.isInStartContainer).map(({id}) => id),
        optionsPanel ? optionsPanel.id : null,
        ...hostTypes.map(({position}) => "toolbox-dock-" + position),
        canCloseToolbox ? "toolbox-close" : null
      ].filter(id => id)
    });

    this.updateFocusedButton();
  }

  updateFocusedButton() {
    this.setFocusedButton(this.state.focusedButton);
  }

  setFocusedButton(focusedButton) {
    const {buttonIds} = this.state;

    focusedButton = focusedButton && buttonIds.includes(focusedButton)
        ? focusedButton
        : buttonIds[0];
    if (this.state.focusedButton !== focusedButton) {
      this.setState({
        focusedButton
      });
    }
  }

  setCurrentToolId(currentToolId) {
    this.setState({currentToolId});
    // Also set the currently focused button to this tool.
    this.setFocusedButton(currentToolId);
  }

  setCanRender() {
    this.setState({ canRender: true });
    this.updateButtonIds();
  }

  setOptionsPanel(optionsPanel) {
    this.setState({ optionsPanel });
    this.updateButtonIds();
  }

  highlightTool(highlightedTool) {
    this.setState({ highlightedTool });
  }

  unhighlightTool(id) {
    if (this.state.highlightedTool === id) {
      this.setState({ highlightedTool: "" });
    }
  }

  setDockButtonsEnabled(areDockButtonsEnabled) {
    this.setState({ areDockButtonsEnabled });
    this.updateButtonIds();
  }

  setHostTypes(hostTypes) {
    this.setState({ hostTypes });
    this.updateButtonIds();
  }

  setCanCloseToolbox(canCloseToolbox) {
    this.setState({ canCloseToolbox });
    this.updateButtonIds();
  }

  setPanelDefinitions(panelDefinitions) {
    this.setState({ panelDefinitions });
    this.updateButtonIds();
  }

  setToolboxButtons(toolboxButtons) {
    // Listen for updates of the checked attribute.
    this.state.toolboxButtons.forEach(button => {
      button.off("updatechecked", this.state.checkedButtonsUpdated);
    });
    toolboxButtons.forEach(button => {
      button.on("updatechecked", this.state.checkedButtonsUpdated);
    });

    this.setState({ toolboxButtons });
    this.updateButtonIds();
  }

  setCanMinimize(canMinimize) {
    /* Bug 1177463 - The minimize button is currently hidden until we agree on
       the UI for it, and until bug 1173849 is fixed too. */

    // this.setState({ canMinimize });
  }

  render() {
    return ToolboxToolbar(Object.assign({}, this.props, this.state));
  }
}

module.exports = ToolboxController;
