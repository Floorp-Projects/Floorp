/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  createFactory,
  createRef,
  Component,
  cloneElement,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  ul,
  li,
  div,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

const {
  scrollIntoView,
} = require("resource://devtools/client/shared/scroll.js");
const {
  preventDefaultAndStopPropagation,
} = require("resource://devtools/client/shared/events.js");

loader.lazyRequireGetter(
  this,
  ["getFocusableElements", "wrapMoveFocus"],
  "resource://devtools/client/shared/focus.js",
  true
);

class ListItemClass extends Component {
  static get propTypes() {
    return {
      active: PropTypes.bool,
      current: PropTypes.bool,
      onClick: PropTypes.func,
      item: PropTypes.shape({
        key: PropTypes.string,
        component: PropTypes.object,
        componentProps: PropTypes.object,
        className: PropTypes.string,
      }).isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.contentRef = createRef();

    this._setTabbableState = this._setTabbableState.bind(this);
    this._onKeyDown = this._onKeyDown.bind(this);
  }

  componentDidMount() {
    this._setTabbableState();
  }

  componentDidUpdate() {
    this._setTabbableState();
  }

  _onKeyDown(event) {
    const { target, key, shiftKey } = event;

    if (key !== "Tab") {
      return;
    }

    const focusMoved = !!wrapMoveFocus(
      getFocusableElements(this.contentRef.current),
      target,
      shiftKey
    );
    if (focusMoved) {
      // Focus was moved to the begining/end of the list, so we need to prevent the
      // default focus change that would happen here.
      event.preventDefault();
    }

    event.stopPropagation();
  }

  /**
   * Makes sure that none of the focusable elements inside the list item container are
   * tabbable if the list item is not active. If the list item is active and focus is
   * outside its container, focus on the first focusable element inside.
   */
  _setTabbableState() {
    const elms = getFocusableElements(this.contentRef.current);
    if (elms.length === 0) {
      return;
    }

    if (!this.props.active) {
      elms.forEach(elm => elm.setAttribute("tabindex", "-1"));
      return;
    }

    if (!elms.includes(document.activeElement)) {
      elms[0].focus();
    }
  }

  render() {
    const { active, item, current, onClick } = this.props;
    const { className, component, componentProps } = item;

    return li(
      {
        className: `${className}${current ? " current" : ""}${
          active ? " active" : ""
        }`,
        id: item.key,
        onClick,
        onKeyDownCapture: active ? this._onKeyDown : null,
      },
      div(
        {
          className: "list-item-content",
          role: "presentation",
          ref: this.contentRef,
        },
        cloneElement(component, componentProps || {})
      )
    );
  }
}

const ListItem = createFactory(ListItemClass);

class List extends Component {
  static get propTypes() {
    return {
      // A list of all items to be rendered using a List component.
      items: PropTypes.arrayOf(
        PropTypes.shape({
          component: PropTypes.object,
          componentProps: PropTypes.object,
          className: PropTypes.string,
          key: PropTypes.string.isRequired,
        })
      ).isRequired,

      // Note: the two properties below are mutually exclusive. Only one of the
      // label properties is necessary.
      // ID of an element whose textual content serves as an accessible label for
      // a list.
      labelledBy: PropTypes.string,

      // Accessibility label for a list widget.
      label: PropTypes.string,
    };
  }

  constructor(props) {
    super(props);

    this.listRef = createRef();

    this.state = {
      active: null,
      current: null,
      mouseDown: false,
    };

    this._setCurrentItem = this._setCurrentItem.bind(this);
    this._preventArrowKeyScrolling = this._preventArrowKeyScrolling.bind(this);
    this._onKeyDown = this._onKeyDown.bind(this);
  }

  shouldComponentUpdate(nextProps, nextState) {
    const { active, current, mouseDown } = this.state;

    return (
      current !== nextState.current ||
      active !== nextState.active ||
      mouseDown === nextState.mouseDown
    );
  }

  _preventArrowKeyScrolling(e) {
    switch (e.key) {
      case "ArrowUp":
      case "ArrowDown":
      case "ArrowLeft":
      case "ArrowRight":
        preventDefaultAndStopPropagation(e);
        break;
    }
  }

  /**
   * Sets the passed in item to be the current item.
   *
   * @param {null|Number} index
   *        The index of the item in to be set as current, or undefined to unset the
   *        current item.
   */
  _setCurrentItem(index = -1, options = {}) {
    const item = this.props.items[index];
    if (item !== undefined && !options.preventAutoScroll) {
      const element = document.getElementById(item.key);
      scrollIntoView(element, {
        ...options,
        container: this.listRef.current,
      });
    }

    const state = {};
    if (this.state.active != undefined) {
      state.active = null;
      if (this.listRef.current !== document.activeElement) {
        this.listRef.current.focus();
      }
    }

    if (this.state.current !== index) {
      this.setState({
        ...state,
        current: index,
      });
    }
  }

  /**
   * Handles key down events in the list's container.
   *
   * @param {Event} e
   */
  _onKeyDown(e) {
    const { active, current } = this.state;
    if (current == null) {
      return;
    }

    if (e.altKey || e.ctrlKey || e.shiftKey || e.metaKey) {
      return;
    }

    this._preventArrowKeyScrolling(e);

    const { length } = this.props.items;
    switch (e.key) {
      case "ArrowUp":
        current > 0 && this._setCurrentItem(current - 1, { alignTo: "top" });
        break;

      case "ArrowDown":
        current < length - 1 &&
          this._setCurrentItem(current + 1, { alignTo: "bottom" });
        break;

      case "Home":
        this._setCurrentItem(0, { alignTo: "top" });
        break;

      case "End":
        this._setCurrentItem(length - 1, { alignTo: "bottom" });
        break;

      case "Enter":
      case " ":
        // On space or enter make current list item active. This means keyboard focus
        // handling is passed on to the component within the list item.
        if (document.activeElement === this.listRef.current) {
          preventDefaultAndStopPropagation(e);
          if (active !== current) {
            this.setState({ active: current });
          }
        }
        break;

      case "Escape":
        // If current list item is active, make it inactive and let keyboard focusing be
        // handled normally.
        preventDefaultAndStopPropagation(e);
        if (active != null) {
          this.setState({ active: null });
        }

        this.listRef.current.focus();
        break;
    }
  }

  render() {
    const { active, current } = this.state;
    const { items } = this.props;

    return ul(
      {
        ref: this.listRef,
        className: "list",
        tabIndex: 0,
        onKeyDown: this._onKeyDown,
        onKeyPress: this._preventArrowKeyScrolling,
        onKeyUp: this._preventArrowKeyScrolling,
        onMouseDown: () => this.setState({ mouseDown: true }),
        onMouseUp: () => this.setState({ mouseDown: false }),
        onFocus: () => {
          if (current != null || this.state.mouseDown) {
            return;
          }

          // Only set default current to the first list item if current item is
          // not yet set and the focus event is not the result of a mouse
          // interarction.
          this._setCurrentItem(0);
        },
        onClick: () => {
          // Focus should always remain on the list container itself.
          this.listRef.current.focus();
        },
        onBlur: e => {
          if (active != null) {
            const { relatedTarget } = e;
            if (!this.listRef.current.contains(relatedTarget)) {
              this.setState({ active: null });
            }
          }
        },
        "aria-label": this.props.label,
        "aria-labelledby": this.props.labelledBy,
        "aria-activedescendant": current != null ? items[current].key : null,
      },
      items.map((item, index) => {
        return ListItem({
          item,
          current: index === current,
          active: index === active,
          // We make a key unique depending on whether the list item is in active or
          // inactive state to make sure that it is actually replaced and the tabbable
          // state is reset.
          key: `${item.key}-${index === active ? "active" : "inactive"}`,
          // Since the user just clicked the item, there's no need to check if it should
          // be scrolled into view.
          onClick: () =>
            this._setCurrentItem(index, { preventAutoScroll: true }),
        });
      })
    );
  }
}

module.exports = {
  ListItem: ListItemClass,
  List,
};
