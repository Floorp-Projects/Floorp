/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
"use strict";

// A button that toggles a doorhanger menu.

const { PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const Services = require("Services");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { button } = dom;
const {
  HTMLTooltip,
} = require("devtools/client/shared/widgets/tooltip/HTMLTooltip");

// Return a copy of |obj| minus |fields|.
const omit = (obj, fields) => {
  const objCopy = { ...obj };
  for (const field of fields) {
    delete objCopy[field];
  }
  return objCopy;
};

class MenuButton extends PureComponent {
  static get propTypes() {
    return {
      // The document to be used for rendering the menu popup.
      doc: PropTypes.object.isRequired,

      // An optional ID to assign to the menu's container tooltip object.
      menuId: PropTypes.string,

      // The preferred side of the anchor element to display the menu.
      // Defaults to "bottom".
      menuPosition: PropTypes.string.isRequired,

      // The offset of the menu from the anchor element.
      // Defaults to -5.
      menuOffset: PropTypes.number.isRequired,

      // The menu content.
      children: PropTypes.any,

      // Callback function to be invoked when the button is clicked.
      onClick: PropTypes.func,
    };
  }

  static get defaultProps() {
    return {
      menuPosition: "bottom",
      menuOffset: -5,
    };
  }

  constructor(props) {
    super(props);

    this.showMenu = this.showMenu.bind(this);
    this.hideMenu = this.hideMenu.bind(this);
    this.toggleMenu = this.toggleMenu.bind(this);
    this.onHidden = this.onHidden.bind(this);
    this.onClick = this.onClick.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);

    this.tooltip = null;
    this.buttonRef = null;
    this.setButtonRef = element => {
      this.buttonRef = element;
    };

    this.state = {
      expanded: false,
      win: props.doc.defaultView.top,
    };
  }

  componentWillMount() {
    this.initializeTooltip();
  }

  componentWillReceiveProps(nextProps) {
    // If the window changes, we need to regenerate the HTMLTooltip or else the
    // XUL wrapper element will appear above (in terms of z-index) the old
    // window, and not the new.
    const win = nextProps.doc.defaultView.top;
    if (
      nextProps.doc !== this.props.doc ||
      this.state.win !== win ||
      nextProps.menuId !== this.props.menuId
    ) {
      this.setState({ win });
      this.resetTooltip();
      this.initializeTooltip();
    }
  }

  componentWillUnmount() {
    this.resetTooltip();
  }

  initializeTooltip() {
    const tooltipProps = {
      type: "doorhanger",
      useXulWrapper: true,
    };

    if (this.props.menuId) {
      tooltipProps.id = this.props.menuId;
    }

    this.tooltip = new HTMLTooltip(this.props.doc, tooltipProps);
    this.tooltip.on("hidden", this.onHidden);
  }

  async resetTooltip() {
    if (!this.tooltip) {
      return;
    }

    // Mark the menu as closed since the onHidden callback may not be called in
    // this case.
    this.setState({ expanded: false });
    this.tooltip.destroy();
    this.tooltip.off("hidden", this.onHidden);
    this.tooltip = null;
  }

  async showMenu(anchor) {
    this.setState({
      expanded: true
    });

    if (!this.tooltip) {
      return;
    }

    await this.tooltip.show(anchor, {
      position: this.props.menuPosition,
      y: this.props.menuOffset,
    });
  }

  async hideMenu() {
    this.setState({
      expanded: false
    });

    if (!this.tooltip) {
      return;
    }

    await this.tooltip.hide();
  }

  async toggleMenu(anchor) {
    return this.state.expanded ? this.hideMenu() : this.showMenu(anchor);
  }

  // Used by the call site to indicate that the menu content has changed so
  // its container should be updated.
  resizeContent() {
    if (!this.state.expanded || !this.tooltip || !this.buttonRef) {
      return;
    }

    this.tooltip.updateContainerBounds(this.buttonRef, {
      position: this.props.menuPosition,
      y: this.props.menuOffset,
    });
  }

  onHidden() {
    this.setState({ expanded: false });
    // While the menu is open, if we click _anywhere_ outside the menu, it will
    // automatically close. This is performed by the XUL wrapper before we get
    // any chance to see any event. To avoid immediately re-opening the menu
    // when we process the subsequent click event on this button, we set
    // 'pointer-events: none' on the button while the menu is open.
    //
    // After the menu is closed we need to remove the pointer-events style (so
    // the button works again) but we don't want to do it immediately since the
    // "popuphidden" event which triggers this callback might be dispatched
    // before the "click" event that we want to ignore.  As a result, we queue
    // up a task using setTimeout() to run after the "click" event.
    this.state.win.setTimeout(() => {
      if (this.buttonRef) {
        this.buttonRef.style.pointerEvents = "auto";
      }
    }, 0);
  }

  async onClick(e) {
    if (e.target === this.buttonRef) {
      // On Mac, even after clicking the button it doesn't get focus.
      // Force focus to the button so that our keydown handlers get called.
      this.buttonRef.focus();

      if (this.props.onClick) {
        this.props.onClick(e);
      }

      if (!e.defaultPrevented) {
        const wasKeyboardEvent = e.screenX === 0 && e.screenY === 0;
        // If the popup menu will be shown, disable this button in order to
        // prevent reopening the popup menu. See extended comment in onHidden().
        // above.
        //
        // Also, we should _not_ set 'pointer-events: none' if
        // ui.popup.disable_autohide pref is in effect since, in that case,
        // there's no redundant hiding behavior and we actually want clicking
        // the button to close the menu.
        if (!this.state.expanded &&
            !Services.prefs.getBoolPref("ui.popup.disable_autohide", false)) {
          this.buttonRef.style.pointerEvents = "none";
        }
        await this.toggleMenu(e.target);
        // If the menu was activated by keyboard, focus the first item.
        if (wasKeyboardEvent && this.tooltip) {
          this.tooltip.focus();
        }
      }
    // If we clicked one of the menu items, then, by default, we should
    // auto-collapse the menu.
    //
    // We check for the defaultPrevented state, however, so that menu items can
    // turn this behavior off (e.g. a menu item with an embedded button).
    } else if (this.state.expanded && !e.defaultPrevented) {
      this.hideMenu();
    }
  }

  onKeyDown(e) {
    if (!this.state.expanded) {
      return;
    }

    const isButtonFocussed =
      this.props.doc && this.props.doc.activeElement === this.buttonRef;

    switch (e.key) {
      case "Escape":
        this.hideMenu();
        e.preventDefault();
        break;

      case "Tab":
      case "ArrowDown":
        if (isButtonFocussed && this.tooltip) {
          if (this.tooltip.focus()) {
            e.preventDefault();
          }
        }
        break;

      case "ArrowUp":
        if (isButtonFocussed && this.tooltip) {
          if (this.tooltip.focusEnd()) {
            e.preventDefault();
          }
        }
        break;
    }
  }

  render() {
    // We bypass the call to HTMLTooltip. setContent and set the panel contents
    // directly here.
    //
    // Bug 1472942: Do this for all users of HTMLTooltip.
    const menu = ReactDOM.createPortal(
      this.props.children,
      this.tooltip.panel
    );

    const buttonProps = {
      // Pass through any props set on the button, except the ones we handle
      // here.
      ...omit(this.props, Object.keys(MenuButton.propTypes)),
      onClick: this.onClick,
      "aria-expanded": this.state.expanded,
      "aria-haspopup": "menu",
      ref: this.setButtonRef,
    };

    if (this.state.expanded) {
      buttonProps.onKeyDown = this.onKeyDown;
    }

    if (this.props.menuId) {
      buttonProps["aria-controls"] = this.props.menuId;
    }

    return button(buttonProps, menu);
  }
}

module.exports = MenuButton;
