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

type Props = {
  categories: EventListenerCategoryList,
  expandedCategories: EventListenerExpandedList,
  activeEventListeners: EventListenerActiveList,
  addEventListeners: typeof actions.addEventListenerBreakpoints,
  removeEventListeners: typeof actions.removeEventListenerBreakpoints,
  addEventListenerExpanded: typeof actions.addEventListenerExpanded,
  removeEventListenerExpanded: typeof actions.removeEventListenerExpanded,
};

class EventListeners extends Component<Props> {
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
            onChange={e => this.onCategoryClick(category, e.target.checked)}
            checked={checked}
            ref={el => el && (el.indeterminate = indeterminate)}
          />
          <span className="event-listener-category">{category.name}</span>
        </label>
      </div>
    );
  }

  renderCategoryListing(category) {
    const { activeEventListeners, expandedCategories } = this.props;

    const expanded = expandedCategories.includes(category.name);
    if (!expanded) {
      return null;
    }

    return (
      <ul>
        {category.events.map(event => {
          return (
            <li className="event-listener-event" key={event.id}>
              <label className="event-listener-label">
                <input
                  type="checkbox"
                  value={event.id}
                  onChange={e =>
                    this.onEventTypeClick(event.id, e.target.checked)
                  }
                  checked={activeEventListeners.includes(event.id)}
                />
                <span className="event-listener-name">{event.name}</span>
              </label>
            </li>
          );
        })}
      </ul>
    );
  }

  render() {
    const { categories } = this.props;

    return (
      <div className="event-listeners-content">
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
      </div>
    );
  }
}

const mapStateToProps = state => {
  return {
    activeEventListeners: getActiveEventListeners(state),
    categories: getEventListenerBreakpointTypes(state),
    expandedCategories: getEventListenerExpanded(state),
  };
};

export default connect(
  mapStateToProps,
  {
    addEventListeners: actions.addEventListenerBreakpoints,
    removeEventListeners: actions.removeEventListenerBreakpoints,
    addEventListenerExpanded: actions.addEventListenerExpanded,
    removeEventListenerExpanded: actions.removeEventListenerExpanded,
  }
)(EventListeners);
