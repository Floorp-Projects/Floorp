/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React, { Component } from "react";
import classnames from "classnames";

import { connect } from "../../utils/connect";
import actions from "../../actions";
import {
  getActiveEventListeners,
  getEventListenerBreakpointTypes,
  getEventListenerExpanded,
} from "../../selectors";

import AccessibleImage from "../shared/AccessibleImage";

import type {
  EventListenerActiveList,
  EventListenerCategoryList,
  EventListenerExpandedList,
} from "../../actions/types";

import "./EventListeners.css";

type State = {
  searchText: string,
  focused: boolean,
};

type Props = {
  categories: EventListenerCategoryList,
  expandedCategories: EventListenerExpandedList,
  activeEventListeners: EventListenerActiveList,
  addEventListeners: typeof actions.addEventListenerBreakpoints,
  removeEventListeners: typeof actions.removeEventListenerBreakpoints,
  addEventListenerExpanded: typeof actions.addEventListenerExpanded,
  removeEventListenerExpanded: typeof actions.removeEventListenerExpanded,
};

class EventListeners extends Component<Props, State> {
  state = {
    searchText: "",
    focused: false,
  };

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
    this.setState({ searchText: event.target.value });
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

    return (
      <form className="event-search-form" onSubmit={e => e.preventDefault()}>
        <input
          className={classnames("event-search-input", { focused })}
          placeholder={placeholder}
          value={searchText}
          onChange={this.onInputChange}
          onKeyDown={this.onKeyDown}
          onFocus={this.onFocus}
          onBlur={this.onBlur}
        />
      </form>
    );
  }

  renderClearSearchButton() {
    const { searchText } = this.state;

    if (!searchText) {
      return null;
    }

    return (
      <button
        onClick={() => this.setState({ searchText: "" })}
        className="devtools-searchinput-clear"
      />
    );
  }

  renderCategoriesList() {
    const { categories } = this.props;

    return (
      <ul className="event-listeners-list">
        {categories.map((category, index) => {
          return (
            <li className="event-listener-group" key={index}>
              {this.renderCategoryHeading(category)}
              {this.renderCategoryListing(category)}
            </li>
          );
        })}
      </ul>
    );
  }

  renderSearchResultsList() {
    const searchResults = this.getSearchResults();

    return (
      <ul className="event-search-results-list">
        {Object.keys(searchResults).map(category => {
          return searchResults[category].map(event => {
            return this.renderListenerEvent(event, category);
          });
        })}
      </ul>
    );
  }

  renderCategoryHeading(category) {
    const { activeEventListeners, expandedCategories } = this.props;
    const { events } = category;

    const expanded = expandedCategories.includes(category.name);
    const checked = events.every(({ id }) => activeEventListeners.includes(id));
    const indeterminate =
      !checked && events.some(({ id }) => activeEventListeners.includes(id));

    return (
      <div className="event-listener-header">
        <button
          className="event-listener-expand"
          onClick={() => this.onCategoryToggle(category.name)}
        >
          <AccessibleImage className={classnames("arrow", { expanded })} />
        </button>
        <label className="event-listener-label">
          <input
            type="checkbox"
            value={category.name}
            onChange={e => {
              this.onCategoryClick(
                category,
                // Clicking an indeterminate checkbox should always have the
                // effect of disabling any selected items.
                indeterminate ? false : e.target.checked
              );
            }}
            checked={checked}
            ref={el => el && (el.indeterminate = indeterminate)}
          />
          <span className="event-listener-category">{category.name}</span>
        </label>
      </div>
    );
  }

  renderCategoryListing(category) {
    const { expandedCategories } = this.props;

    const expanded = expandedCategories.includes(category.name);
    if (!expanded) {
      return null;
    }

    return (
      <ul>
        {category.events.map(event => {
          return this.renderListenerEvent(event, category);
        })}
      </ul>
    );
  }

  renderCategory(category) {
    return <span className="category-label">{category.toString()} ▸ </span>;
  }

  renderListenerEvent(event, category) {
    const { activeEventListeners } = this.props;
    const { searchText } = this.state;

    return (
      <li className="event-listener-event" key={event.id}>
        <label className="event-listener-label">
          <input
            type="checkbox"
            value={event.id}
            onChange={e => this.onEventTypeClick(event.id, e.target.checked)}
            checked={activeEventListeners.includes(event.id)}
          />
          <span className="event-listener-name">
            {searchText ? this.renderCategory(category) : null}
            {event.name}
          </span>
        </label>
      </li>
    );
  }

  render() {
    const { searchText } = this.state;

    return (
      <div className="event-listeners">
        <div className="event-search-container">
          {this.renderSearchInput()}
          {this.renderClearSearchButton()}
        </div>
        <div className="event-listeners-content">
          {searchText
            ? this.renderSearchResultsList()
            : this.renderCategoriesList()}
        </div>
      </div>
    );
  }
}

const mapStateToProps = state => ({
  activeEventListeners: getActiveEventListeners(state),
  categories: getEventListenerBreakpointTypes(state),
  expandedCategories: getEventListenerExpanded(state),
});

export default connect(
  mapStateToProps,
  {
    addEventListeners: actions.addEventListenerBreakpoints,
    removeEventListeners: actions.removeEventListenerBreakpoints,
    addEventListenerExpanded: actions.addEventListenerExpanded,
    removeEventListenerExpanded: actions.removeEventListenerExpanded,
  }
)(EventListeners);
