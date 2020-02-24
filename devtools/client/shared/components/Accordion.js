/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const {
  Component,
  createElement,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  ul,
  li,
  h2,
  div,
  span,
} = require("devtools/client/shared/vendor/react-dom-factories");

class Accordion extends Component {
  static get propTypes() {
    return {
      // A list of all items to be rendered using an Accordion component.
      items: PropTypes.arrayOf(
        PropTypes.shape({
          buttons: PropTypes.arrayOf(PropTypes.object),
          className: PropTypes.string,
          component: PropTypes.oneOfType([PropTypes.object, PropTypes.func]),
          componentProps: PropTypes.object,
          contentClassName: PropTypes.string,
          header: PropTypes.string.isRequired,
          id: PropTypes.string.isRequired,
          onToggle: PropTypes.func,
          opened: PropTypes.bool.isRequired,
        })
      ).isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      opened: {},
      everOpened: {},
    };

    this.onHeaderClick = this.onHeaderClick.bind(this);
    this.onHeaderKeyDown = this.onHeaderKeyDown.bind(this);
    this.setCollapseState = this.setCollapseState.bind(this);
  }

  componentDidMount() {
    this.setCollapseState();
  }

  /**
   * Add initial data to the state.opened map, and inject new data
   * when receiving updated props.
   */
  componentDidUpdate(prevProps) {
    if (prevProps.items !== this.props.items) {
      this.setCollapseState();
    }
  }

  setCollapseState() {
    // Add initial data (when empty), inject new data and update the
    // state.opened map whenever the props updates.
    const changedItems = this.props.items.filter(
      ({ id, opened }) =>
        typeof this.state.opened[id] !== "boolean" ||
        this.state.opened[id] !== opened
    );

    if (changedItems.length) {
      const everOpened = { ...this.state.everOpened };
      const opened = { ...this.state.opened };

      for (const item of changedItems) {
        everOpened[item.id] = item.opened;
        opened[item.id] = item.opened;
      }

      this.setState({ everOpened, opened });
    }
  }

  /**
   * @param {Event} event Click event.
   * @param {Object} item The item to be collapsed/expanded.
   */
  onHeaderClick(event, item) {
    event.preventDefault();
    // In the Browser Toolbox's Inspector/Layout view, handleHeaderClick is
    // called twice unless we call stopPropagation, making the accordion item
    // open-and-close or close-and-open
    event.stopPropagation();
    this.toggleItem(item);
  }

  /**
   * @param {Event} event Keyboard event.
   * @param {Object} item The item to be collapsed/expanded.
   */
  onHeaderKeyDown(event, item) {
    if (event.key === " " || event.key === "Enter") {
      event.preventDefault();
      this.toggleItem(item);
    }
  }

  /**
   * Expand or collapse an accordion list item.
   * @param  {Object} item The item to be collapsed or expanded.
   */
  toggleItem(item) {
    const { id } = item;
    const opened = !this.state.opened[id];
    this.setState({
      everOpened: {
        ...this.state.everOpened,
        [id]: true,
      },
      opened: {
        ...this.state.opened,
        [id]: opened,
      },
    });

    if (typeof item.onToggle === "function") {
      item.onToggle(opened);
    }
  }

  renderItem(item) {
    const {
      buttons,
      className = "",
      component,
      componentProps = {},
      contentClassName = "",
      header,
      id,
    } = item;
    const headerId = `${id}-header`;
    const opened = this.state.opened[id];
    let itemContent;

    // Only render content if the accordion item is open or has been opened once
    // before. This saves us rendering complex components when users are keeping
    // them closed (e.g. in Inspector/Layout) or may not open them at all.
    if (this.state.everOpened[id]) {
      if (typeof component === "function") {
        itemContent = createElement(component, componentProps);
      } else if (typeof component === "object") {
        itemContent = component;
      }
    }

    return li(
      {
        key: id,
        id,
        className: `accordion-item ${className}`.trim(),
        "aria-labelledby": headerId,
      },
      h2(
        {
          id: headerId,
          className: "accordion-header",
          tabIndex: 0,
          "aria-expanded": opened,
          // If the header contains buttons, make sure the heading name only
          // contains the "header" text and not the button text
          "aria-label": header,
          onKeyDown: event => this.onHeaderKeyDown(event, item),
          onClick: event => this.onHeaderClick(event, item),
        },
        span({
          className: `theme-twisty${opened ? " open" : ""}`,
          role: "presentation",
        }),
        span(
          {
            className: "accordion-header-label",
          },
          header
        ),
        buttons &&
          span(
            {
              className: "accordion-header-buttons",
              role: "presentation",
            },
            buttons
          )
      ),
      div(
        {
          className: `accordion-content ${contentClassName}`.trim(),
          hidden: !opened,
          role: "presentation",
        },
        itemContent
      )
    );
  }

  render() {
    return ul(
      {
        className: "accordion",
        tabIndex: -1,
      },
      this.props.items.map(item => this.renderItem(item))
    );
  }
}

module.exports = Accordion;
