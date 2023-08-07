/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import React, { Component } from "react";
import {
  div,
  input,
  li,
  ul,
  span,
  button,
  form,
  label,
} from "react-dom-factories";
import PropTypes from "prop-types";

import { connect } from "../../utils/connect";
import actions from "../../actions";
import {
  getActiveEventListeners,
  getEventListenerBreakpointTypes,
  getEventListenerExpanded,
} from "../../selectors";

import AccessibleImage from "../shared/AccessibleImage";

const classnames = require("devtools/client/shared/classnames.js");

import "./EventListeners.css";

class EventListeners extends Component {
  state = {
    searchText: "",
    focused: false,
  };

  static get propTypes() {
    return {
      activeEventListeners: PropTypes.array.isRequired,
      addEventListenerExpanded: PropTypes.func.isRequired,
      addEventListeners: PropTypes.func.isRequired,
      categories: PropTypes.array.isRequired,
      expandedCategories: PropTypes.array.isRequired,
      removeEventListenerExpanded: PropTypes.func.isRequired,
      removeEventListeners: PropTypes.func.isRequired,
    };
  }

  hasMatch(eventOrCategoryName, searchText) {
    const lowercaseEventOrCategoryName = eventOrCategoryName.toLowerCase();
    const lowercaseSearchText = searchText.toLowerCase();

    return lowercaseEventOrCategoryName.includes(lowercaseSearchText);
  }

  getSearchResults() {
    const { searchText } = this.state;
    const { categories } = this.props;
    const searchResults = categories.reduce((results, cat, index) => {
      const category = categories[index];

      if (this.hasMatch(category.name, searchText)) {
        results[category.name] = category.events;
      } else {
        results[category.name] = category.events.filter(event =>
          this.hasMatch(event.name, searchText)
        );
      }

      return results;
    }, {});

    return searchResults;
  }

  onCategoryToggle(category) {
    const {
      expandedCategories,
      removeEventListenerExpanded,
      addEventListenerExpanded,
    } = this.props;

    if (expandedCategories.includes(category)) {
      removeEventListenerExpanded(category);
    } else {
      addEventListenerExpanded(category);
    }
  }

  onCategoryClick(category, isChecked) {
    const { addEventListeners, removeEventListeners } = this.props;
    const eventsIds = category.events.map(event => event.id);

    if (isChecked) {
      addEventListeners(eventsIds);
    } else {
      removeEventListeners(eventsIds);
    }
  }

  onEventTypeClick(eventId, isChecked) {
    const { addEventListeners, removeEventListeners } = this.props;
    if (isChecked) {
      addEventListeners([eventId]);
    } else {
      removeEventListeners([eventId]);
    }
  }

  onInputChange = event => {
    this.setState({ searchText: event.currentTarget.value });
  };

  onKeyDown = event => {
    if (event.key === "Escape") {
      this.setState({ searchText: "" });
    }
  };

  onFocus = event => {
    this.setState({ focused: true });
  };

  onBlur = event => {
    this.setState({ focused: false });
  };

  renderSearchInput() {
    const { focused, searchText } = this.state;
    const placeholder = L10N.getStr("eventListenersHeader1.placeholder");
    return form(
      {
        className: "event-search-form",
        onSubmit: e => e.preventDefault(),
      },
      input({
        className: classnames("event-search-input", {
          focused,
        }),
        placeholder: placeholder,
        value: searchText,
        onChange: this.onInputChange,
        onKeyDown: this.onKeyDown,
        onFocus: this.onFocus,
        onBlur: this.onBlur,
      })
    );
  }

  renderClearSearchButton() {
    const { searchText } = this.state;

    if (!searchText) {
      return null;
    }
    return button({
      onClick: () =>
        this.setState({
          searchText: "",
        }),
      className: "devtools-searchinput-clear",
    });
  }

  renderCategoriesList() {
    const { categories } = this.props;
    return ul(
      {
        className: "event-listeners-list",
      },
      categories.map((category, index) => {
        return li(
          {
            className: "event-listener-group",
            key: index,
          },
          this.renderCategoryHeading(category),
          this.renderCategoryListing(category)
        );
      })
    );
  }

  renderSearchResultsList() {
    const searchResults = this.getSearchResults();
    return ul(
      {
        className: "event-search-results-list",
      },
      Object.keys(searchResults).map(category => {
        return searchResults[category].map(event => {
          return this.renderListenerEvent(event, category);
        });
      })
    );
  }

  renderCategoryHeading(category) {
    const { activeEventListeners, expandedCategories } = this.props;
    const { events } = category;

    const expanded = expandedCategories.includes(category.name);
    const checked = events.every(({ id }) => activeEventListeners.includes(id));
    const indeterminate =
      !checked && events.some(({ id }) => activeEventListeners.includes(id));

    return div(
      {
        className: "event-listener-header",
      },
      button(
        {
          className: "event-listener-expand",
          onClick: () => this.onCategoryToggle(category.name),
        },
        React.createElement(AccessibleImage, {
          className: classnames("arrow", {
            expanded,
          }),
        })
      ),
      label(
        {
          className: "event-listener-label",
        },
        input({
          type: "checkbox",
          value: category.name,
          onChange: e => {
            this.onCategoryClick(
              category,
              // Clicking an indeterminate checkbox should always have the
              // effect of disabling any selected items.
              indeterminate ? false : e.target.checked
            );
          },
          checked: checked,
          ref: el => el && (el.indeterminate = indeterminate),
        }),
        span(
          {
            className: "event-listener-category",
          },
          category.name
        )
      )
    );
  }

  renderCategoryListing(category) {
    const { expandedCategories } = this.props;

    const expanded = expandedCategories.includes(category.name);
    if (!expanded) {
      return null;
    }
    return ul(
      null,
      category.events.map(event => {
        return this.renderListenerEvent(event, category.name);
      })
    );
  }

  renderCategory(category) {
    return span(
      {
        className: "category-label",
      },
      category,
      " \u25B8 "
    );
  }

  renderListenerEvent(event, category) {
    const { activeEventListeners } = this.props;
    const { searchText } = this.state;
    return li(
      {
        className: "event-listener-event",
        key: event.id,
      },
      label(
        {
          className: "event-listener-label",
        },
        input({
          type: "checkbox",
          value: event.id,
          onChange: e => this.onEventTypeClick(event.id, e.target.checked),
          checked: activeEventListeners.includes(event.id),
        }),
        span(
          {
            className: "event-listener-name",
          },
          searchText ? this.renderCategory(category) : null,
          event.name
        )
      )
    );
  }

  render() {
    const { searchText } = this.state;
    return div(
      {
        className: "event-listeners",
      },
      div(
        {
          className: "event-search-container",
        },
        this.renderSearchInput(),
        this.renderClearSearchButton()
      ),
      div(
        {
          className: "event-listeners-content",
        },
        searchText
          ? this.renderSearchResultsList()
          : this.renderCategoriesList()
      )
    );
  }
}

const mapStateToProps = state => ({
  activeEventListeners: getActiveEventListeners(state),
  categories: getEventListenerBreakpointTypes(state),
  expandedCategories: getEventListenerExpanded(state),
});

export default connect(mapStateToProps, {
  addEventListeners: actions.addEventListenerBreakpoints,
  removeEventListeners: actions.removeEventListenerBreakpoints,
  addEventListenerExpanded: actions.addEventListenerExpanded,
  removeEventListenerExpanded: actions.removeEventListenerExpanded,
})(EventListeners);
