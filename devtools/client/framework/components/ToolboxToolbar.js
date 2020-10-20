/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { div, button } = dom;

const DebugTargetInfo = createFactory(
  require("devtools/client/framework/components/DebugTargetInfo")
);
const MenuButton = createFactory(
  require("devtools/client/shared/components/menu/MenuButton")
);
const ToolboxTabs = createFactory(
  require("devtools/client/framework/components/ToolboxTabs")
);

loader.lazyGetter(this, "MeatballMenu", function() {
  return createFactory(
    require("devtools/client/framework/components/MeatballMenu")
  );
});
loader.lazyGetter(this, "MenuItem", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuItem")
  );
});
loader.lazyGetter(this, "MenuList", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuList")
  );
});

loader.lazyRequireGetter(
  this,
  "getUnicodeUrl",
  "devtools/client/shared/unicode-url",
  true
);

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
      // An optionally highlighted tools, e.g. "inspector" (used by ToolboxTabs
      // component).
      highlightedTools: PropTypes.instanceOf(Set),
      // List of tool panel definitions (used by ToolboxTabs component).
      panelDefinitions: PropTypes.array,
      // List of possible docking options.
      hostTypes: PropTypes.arrayOf(
        PropTypes.shape({
          position: PropTypes.string.isRequired,
          switchHost: PropTypes.func.isRequired,
        })
      ),
      // Current docking type. Typically one of the position values in
      // |hostTypes| but this is not always the case (e.g. for "browsertoolbox").
      currentHostType: PropTypes.string,
      // Are docking options enabled? They are not enabled in certain situations
      // like when the toolbox is opened in a tab.
      areDockOptionsEnabled: PropTypes.bool,
      // Do we need to add UI for closing the toolbox? We don't when the
      // toolbox is undocked, for example.
      canCloseToolbox: PropTypes.bool,
      // Is the split console currently visible?
      isSplitConsoleActive: PropTypes.bool,
      // Are we disabling the behavior where pop-ups are automatically closed
      // when clicking outside them?
      //
      // This is a tri-state value that may be true/false or undefined where
      // undefined means that the option is not relevant in this context
      // (i.e. we're not in a browser toolbox).
      disableAutohide: PropTypes.bool,
      // Function to turn the options panel on / off.
      toggleOptions: PropTypes.func.isRequired,
      // Function to turn the split console on / off.
      toggleSplitConsole: PropTypes.func,
      // Function to turn the disable pop-up autohide behavior on / off.
      toggleNoAutohide: PropTypes.func,
      // Function to completely close the toolbox.
      closeToolbox: PropTypes.func,
      // Keep a record of what button is focused.
      focusButton: PropTypes.func,
      // Hold off displaying the toolbar until enough information is ready for
      // it to render nicely.
      canRender: PropTypes.bool,
      // Localization interface.
      L10N: PropTypes.object.isRequired,
      // The devtools toolbox
      toolbox: PropTypes.object,
      // Call back function to detect tabs order updated.
      onTabsOrderUpdated: PropTypes.func.isRequired,
      // Count of visible toolbox buttons which is used by ToolboxTabs component
      // to recognize that the visibility of toolbox buttons were changed.
      // Because in the component we cannot compare the visibility since the
      // button definition instance in toolboxButtons will be unchanged.
      visibleToolboxButtonCount: PropTypes.number,
      // Data to show debug target info, if needed
      debugTargetData: PropTypes.shape({
        runtimeInfo: PropTypes.object.isRequired,
        targetType: PropTypes.string.isRequired,
      }),
    };
  }

  constructor(props) {
    super(props);

    this.hideMenu = this.hideMenu.bind(this);
    this.createFrameList = this.createFrameList.bind(this);
    this.highlightFrame = this.highlightFrame.bind(this);
    this.clickFrameButton = this.clickFrameButton.bind(this);
  }

  componentDidMount() {
    this.props.toolbox.on("panel-changed", this.hideMenu);
  }

  componentWillUnmount() {
    this.props.toolbox.off("panel-changed", this.hideMenu);
  }

  hideMenu() {
    if (this.refs.meatballMenuButton) {
      this.refs.meatballMenuButton.hideMenu();
    }

    if (this.refs.frameMenuButton) {
      this.refs.frameMenuButton.hideMenu();
    }
  }

  /**
   * A little helper function to call renderToolboxButtons for buttons at the start
   * of the toolbox.
   */
  renderToolboxButtonsStart() {
    return this.renderToolboxButtons(true);
  }

  /**
   * A little helper function to call renderToolboxButtons for buttons at the end
   * of the toolbox.
   */
  renderToolboxButtonsEnd() {
    return this.renderToolboxButtons(false);
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
  renderToolboxButtons(isStart) {
    const { focusedButton, toolboxButtons, focusButton } = this.props;
    const visibleButtons = toolboxButtons.filter(command => {
      const { isVisible, isInStartContainer } = command;
      return isVisible && (isStart ? isInStartContainer : !isInStartContainer);
    });

    if (visibleButtons.length === 0) {
      return null;
    }

    // The RDM button, if present, should always go last
    const rdmIndex = visibleButtons.findIndex(
      button => button.id === "command-button-responsive"
    );
    if (rdmIndex !== -1 && rdmIndex !== visibleButtons.length - 1) {
      const rdm = visibleButtons.splice(rdmIndex, 1)[0];
      visibleButtons.push(rdm);
    }

    const renderedButtons = visibleButtons.map(command => {
      const {
        id,
        description,
        disabled,
        onClick,
        isChecked,
        className: buttonClass,
        onKeyDown,
      } = command;

      // If button is frame button, create menu button in order to
      // use the doorhanger menu.
      if (id === "command-button-frames") {
        return this.renderFrameButton(command);
      }

      return button({
        id,
        title: description,
        disabled,
        className: `devtools-tabbar-button command-button ${buttonClass ||
          ""} ${isChecked ? "checked" : ""}`,
        onClick: event => {
          onClick(event);
          focusButton(id);
        },
        onFocus: () => focusButton(id),
        tabIndex: id === focusedButton ? "0" : "-1",
        onKeyDown: event => {
          onKeyDown(event);
        },
      });
    });

    // Add the appropriate separator, if needed.
    const children = renderedButtons;
    if (renderedButtons.length) {
      if (isStart) {
        children.push(this.renderSeparator());
        // For the end group we add a separator *before* the RDM button if it
        // exists, but only if it is not the only button.
      } else if (rdmIndex !== -1 && visibleButtons.length > 1) {
        children.splice(children.length - 1, 0, this.renderSeparator());
      }
    }

    return div(
      { id: `toolbox-buttons-${isStart ? "start" : "end"}` },
      ...children
    );
  }

  renderFrameButton(command) {
    const { id, isChecked, disabled, description } = command;

    const { toolbox } = this.props;

    return MenuButton(
      {
        id,
        disabled,
        menuId: id + "-panel",
        toolboxDoc: toolbox.doc,
        className: `devtools-tabbar-button command-button ${
          isChecked ? "checked" : ""
        }`,
        ref: "frameMenuButton",
        title: description,
        onCloseButton: () => {
          // Only try to unhighlight if the highlighter has been started
          const inspectorFront = toolbox.target.getCachedFront("inspector");
          if (inspectorFront) {
            inspectorFront.highlighter.unhighlight();
          }
        },
      },
      this.createFrameList
    );
  }

  clickFrameButton(event) {
    const { toolbox } = this.props;
    toolbox.onSelectFrame(event.target.id);
  }

  highlightFrame(id) {
    if (!id) {
      return;
    }

    const { toolbox } = this.props;
    toolbox.onHighlightFrame(id);
  }

  createFrameList() {
    const { toolbox } = this.props;
    if (toolbox.frameMap.size < 1) {
      return null;
    }

    const items = [];
    toolbox.frameMap.forEach((frame, index) => {
      const label = toolbox.target.isWebExtension
        ? toolbox.target.getExtensionPathName(frame.url)
        : getUnicodeUrl(frame.url);
      items.push(
        MenuItem({
          id: frame.id.toString(),
          key: "toolbox-frame-key-" + frame.id,
          label,
          checked: frame.id === toolbox.selectedFrameId,
          onClick: this.clickFrameButton,
        })
      );
    });

    return MenuList(
      {
        id: "toolbox-frame-menu",
        onHighlightedChildChange: this.highlightFrame,
      },
      items
    );
  }

  /**
   * Render a separator.
   */
  renderSeparator() {
    return div({ className: "devtools-separator" });
  }

  /**
   * Render the toolbox control buttons. The following props are expected:
   *
   * @param {string} props.focusedButton
   *        The id of the focused button.
   * @param {string} props.currentToolId
   *        The id of the currently selected tool, e.g. "inspector".
   * @param {Object[]} props.hostTypes
   *        Array of host type objects.
   * @param {string} props.hostTypes[].position
   *        Position name.
   * @param {Function} props.hostTypes[].switchHost
   *        Function to switch the host.
   * @param {string} props.currentHostType
   *        The current docking configuration.
   * @param {boolean} props.areDockOptionsEnabled
   *        They are not enabled in certain situations like when the toolbox is
   *        in a tab.
   * @param {boolean} props.canCloseToolbox
   *        Do we need to add UI for closing the toolbox? We don't when the
   *        toolbox is undocked, for example.
   * @param {boolean} props.isSplitConsoleActive
   *         Is the split console currently visible?
   *        toolbox is undocked, for example.
   * @param {boolean|undefined} props.disableAutohide
   *        Are we disabling the behavior where pop-ups are automatically
   *        closed when clicking outside them?
   *        (Only defined for the browser toolbox.)
   * @param {Function} props.selectTool
   *        Function to select a tool based on its id.
   * @param {Function} props.toggleOptions
   *        Function to turn the options panel on / off.
   * @param {Function} props.toggleSplitConsole
   *        Function to turn the split console on / off.
   * @param {Function} props.toggleNoAutohide
   *        Function to turn the disable pop-up autohide behavior on / off.
   * @param {Function} props.closeToolbox
   *        Completely close the toolbox.
   * @param {Function} props.focusButton
   *        Keep a record of the currently focused button.
   * @param {Object} props.L10N
   *        Localization interface.
   * @param {Object} props.toolbox
   *        The devtools toolbox. Used by the MenuButton component to display
   *        the menu popup.
   * @param {Object} refs
   *        The components refs object. Used to keep a reference to the MenuButton
   *        for the meatball menu so that we can tell it to resize its contents
   *        when they change.
   */
  renderToolboxControls() {
    const {
      focusedButton,
      canCloseToolbox,
      closeToolbox,
      focusButton,
      L10N,
      toolbox,
    } = this.props;

    const meatballMenuButtonId = "toolbox-meatball-menu-button";

    const meatballMenuButton = MenuButton(
      {
        id: meatballMenuButtonId,
        menuId: meatballMenuButtonId + "-panel",
        toolboxDoc: toolbox.doc,
        onFocus: () => focusButton(meatballMenuButtonId),
        className: "devtools-tabbar-button",
        title: L10N.getStr("toolbox.meatballMenu.button.tooltip"),
        tabIndex: focusedButton === meatballMenuButtonId ? "0" : "-1",
        ref: "meatballMenuButton",
      },
      MeatballMenu({
        ...this.props,
        hostTypes: this.props.areDockOptionsEnabled ? this.props.hostTypes : [],
        onResize: () => {
          this.refs.meatballMenuButton.resizeContent();
        },
      })
    );

    const closeButtonId = "toolbox-close";

    const closeButton = canCloseToolbox
      ? button({
          id: closeButtonId,
          onFocus: () => focusButton(closeButtonId),
          className: "devtools-tabbar-button",
          title: L10N.getStr("toolbox.closebutton.tooltip"),
          onClick: () => closeToolbox(),
          tabIndex: focusedButton === "toolbox-close" ? "0" : "-1",
        })
      : null;

    return div({ id: "toolbox-controls" }, meatballMenuButton, closeButton);
  }

  /**
   * The render function is kept fairly short for maintainability. See the individual
   * render functions for how each of the sections is rendered.
   */
  render() {
    const { L10N, debugTargetData, toolbox } = this.props;
    const classnames = ["devtools-tabbar"];
    const startButtons = this.renderToolboxButtonsStart();
    const endButtons = this.renderToolboxButtonsEnd();

    if (!startButtons) {
      classnames.push("devtools-tabbar-has-start");
    }
    if (!endButtons) {
      classnames.push("devtools-tabbar-has-end");
    }

    const toolbar = this.props.canRender
      ? div(
          {
            className: classnames.join(" "),
          },
          startButtons,
          ToolboxTabs(this.props),
          endButtons,
          this.renderToolboxControls()
        )
      : div({ className: classnames.join(" ") });

    const debugTargetInfo = debugTargetData
      ? DebugTargetInfo({ debugTargetData, L10N, toolbox })
      : null;

    return div({}, debugTargetInfo, toolbar);
  }
}

module.exports = ToolboxToolbar;
