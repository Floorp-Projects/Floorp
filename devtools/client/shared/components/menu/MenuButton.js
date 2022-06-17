/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */
"use strict";

// A button that toggles a doorhanger menu.

const Services = require("Services");
const flags = require("devtools/shared/flags");
const {
  createRef,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { button } = dom;

const isMacOS = Services.appinfo.OS === "Darwin";

loader.lazyRequireGetter(
  this,
  "HTMLTooltip",
  "devtools/client/shared/widgets/tooltip/HTMLTooltip",
  true
);

loader.lazyRequireGetter(
  this,
  "focusableSelector",
  "devtools/client/shared/focus",
  true
);

loader.lazyRequireGetter(
  this,
  "createPortal",
  "devtools/client/shared/vendor/react-dom",
  true
);

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
      // The toolbox document that will be used for rendering the menu popup.
      toolboxDoc: PropTypes.object.isRequired,

      // A text content for the button.
      label: PropTypes.string,

      // URL of the icon to associate with the MenuButton. (Optional)
      // e.g. chrome://devtools/skin/image/foo.svg
      icon: PropTypes.string,

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

      // Callback function to be invoked when the child panel is closed.
      onCloseButton: PropTypes.func,
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
    this.onTouchStart = this.onTouchStart.bind(this);

    this.buttonRef = createRef();

    this.state = {
      expanded: false,
      // In tests, initialize the menu immediately.
      isMenuInitialized: flags.testing || false,
      win: props.toolboxDoc.defaultView.top,
    };
    this.ignoreNextClick = false;

    this.initializeTooltip();
  }

  componentDidMount() {
    if (!this.state.isMenuInitialized) {
      // Initialize the menu when the button is focused or moused over.
      for (const event of ["focus", "mousemove"]) {
        this.buttonRef.current.addEventListener(
          event,
          () => {
            if (!this.state.isMenuInitialized) {
              this.setState({ isMenuInitialized: true });
            }
          },
          { once: true }
        );
      }
    }
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    // If the window changes, we need to regenerate the HTMLTooltip or else the
    // XUL wrapper element will appear above (in terms of z-index) the old
    // window, and not the new.
    const win = nextProps.toolboxDoc.defaultView.top;
    if (
      nextProps.toolboxDoc !== this.props.toolboxDoc ||
      this.state.win !== win ||
      nextProps.menuId !== this.props.menuId
    ) {
      this.setState({ win });
      this.resetTooltip();
      this.initializeTooltip();
    }
  }

  componentDidUpdate() {
    // The MenuButton creates the child panel when initializing the MenuButton.
    // If the children function is called during the rendering process,
    // this child list size might change. So we need to adjust content size here.
    if (typeof this.props.children === "function") {
      this.resizeContent();
    }
  }

  componentWillUnmount() {
    this.resetTooltip();
  }

  initializeTooltip() {
    const tooltipProps = {
      type: "doorhanger",
      useXulWrapper: true,
      isMenuTooltip: true,
    };

    if (this.props.menuId) {
      tooltipProps.id = this.props.menuId;
    }

    this.tooltip = new HTMLTooltip(this.props.toolboxDoc, tooltipProps);
    this.tooltip.on("hidden", this.onHidden);
  }

  async resetTooltip() {
    if (!this.tooltip) {
      return;
    }

    // Mark the menu as closed since the onHidden callback may not be called in
    // this case.
    this.setState({ expanded: false });
    this.tooltip.off("hidden", this.onHidden);
    this.tooltip.destroy();
    this.tooltip = null;
  }

  async showMenu(anchor) {
    this.setState({
      expanded: true,
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
      expanded: false,
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
    if (!this.state.expanded || !this.tooltip || !this.buttonRef.current) {
      return;
    }

    this.tooltip.show(this.buttonRef.current, {
      position: this.props.menuPosition,
      y: this.props.menuOffset,
    });
  }

  // When we are closing the menu we will get a 'hidden' event before we get
  // a 'click' event. We want to re-enable the pointer-events: auto setting we
  // use on the button while the menu is visible, but we don't want to do it
  // until after the subsequent click event since otherwise we will end up
  // re-opening the menu.
  //
  // For mouse events, we achieve this by using setTimeout(..., 0) to schedule
  // a separate task to run after the click event, but in the case of touch
  // events the event order differs and the setTimeout callback will run before
  // the click event.
  //
  // In order to prevent that we detect touch events and set a flag to ignore
  // the next click event. However, we need to differentiate between touch drag
  // events and long press events (which don't generate a 'click') and "taps"
  // (which do). We do that by looking for a 'touchmove' event and clearing the
  // flag if we get one.
  onTouchStart(evt) {
    const touchend = () => {
      const anchorRect = this.buttonRef.current.getClientRects()[0];
      const { clientX, clientY } = evt.changedTouches[0];
      // We need to check that the click is inside the bounds since when the
      // menu is being closed the button will currently have
      // pointer-events: none (and if we don't check the bounds we will end up
      // ignoring unrelated clicks).
      if (
        anchorRect.x <= clientX &&
        clientX <= anchorRect.x + anchorRect.width &&
        anchorRect.y <= clientY &&
        clientY <= anchorRect.y + anchorRect.height
      ) {
        this.ignoreNextClick = true;
      }
    };

    const touchmove = () => {
      this.state.win.removeEventListener("touchend", touchend);
    };

    this.state.win.addEventListener("touchend", touchend, { once: true });
    this.state.win.addEventListener("touchmove", touchmove, { once: true });
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
      if (this.buttonRef.current) {
        this.buttonRef.current.style.pointerEvents = "auto";
      }
      this.state.win.removeEventListener("touchstart", this.onTouchStart, true);
    }, 0);

    this.state.win.addEventListener("touchstart", this.onTouchStart, true);

    if (this.props.onCloseButton) {
      this.props.onCloseButton();
    }
  }

  async onClick(e) {
    if (this.ignoreNextClick) {
      this.ignoreNextClick = false;
      return;
    }

    if (e.target === this.buttonRef.current) {
      // On Mac, even after clicking the button it doesn't get focus.
      // Force focus to the button so that our keydown handlers get called.
      this.buttonRef.current.focus();

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
        if (
          !this.state.expanded &&
          !Services.prefs.getBoolPref("ui.popup.disable_autohide", false)
        ) {
          this.buttonRef.current.style.pointerEvents = "none";
        }
        await this.toggleMenu(e.target);
        // If the menu was activated by keyboard, focus the first item.
        if (wasKeyboardEvent && this.tooltip) {
          this.tooltip.focus();
        }

        // MenuButton creates the children dynamically when clicking the button,
        // so execute the goggle menu after updating the children panel.
        if (typeof this.props.children === "function") {
          this.forceUpdate();
        }
      }
      // If we clicked one of the menu items, then, by default, we should
      // auto-collapse the menu.
      //
      // We check for the defaultPrevented state, however, so that menu items can
      // turn this behavior off (e.g. a menu item with an embedded button).
    } else if (
      this.state.expanded &&
      !e.defaultPrevented &&
      e.target.matches(focusableSelector)
    ) {
      this.hideMenu();
    }
  }

  onKeyDown(e) {
    if (!this.state.expanded) {
      return;
    }

    const isButtonFocussed =
      this.props.toolboxDoc &&
      this.props.toolboxDoc.activeElement === this.buttonRef.current;

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
      case "t":
        if ((isMacOS && e.metaKey) || (!isMacOS && e.ctrlKey)) {
          // Close the menu if the user opens a new tab while it is still open.
          //
          // Bug 1499271: Once toolbox has been converted to XUL we should watch
          // for the 'visibilitychange' event instead of explicitly looking for
          // Ctrl+T.
          this.hideMenu();
        }
        break;
    }
  }

  render() {
    const buttonProps = {
      // Pass through any props set on the button, except the ones we handle
      // here.
      ...omit(this.props, Object.keys(MenuButton.propTypes)),
      onClick: this.onClick,
      "aria-expanded": this.state.expanded,
      "aria-haspopup": "menu",
      ref: this.buttonRef,
    };

    if (this.state.expanded) {
      buttonProps.onKeyDown = this.onKeyDown;
    }

    if (this.props.menuId) {
      buttonProps["aria-controls"] = this.props.menuId;
    }

    if (this.props.icon) {
      const iconClass = "menu-button--iconic";
      buttonProps.className = buttonProps.className
        ? `${buttonProps.className} ${iconClass}`
        : iconClass;
      buttonProps.style = {
        "--menuitem-icon-image": "url(" + this.props.icon + ")",
      };
    }

    if (this.state.isMenuInitialized) {
      const menu = createPortal(
        typeof this.props.children === "function"
          ? this.props.children()
          : this.props.children,
        this.tooltip.panel
      );

      return button(buttonProps, this.props.label, menu);
    }

    return button(buttonProps, this.props.label);
  }
}

module.exports = MenuButton;
