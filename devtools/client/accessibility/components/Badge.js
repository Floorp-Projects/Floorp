/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const ToggleButton = createFactory(require("./Button").ToggleButton);

const { audit, filterToggle } = require("../actions/audit");
const { preventDefaultAndStopPropagation } = require("devtools/client/shared/events");

class Badge extends Component {
  static get propTypes() {
    return {
      active: PropTypes.bool.isRequired,
      filterKey: PropTypes.string.isRequired,
      dispatch: PropTypes.func.isRequired,
      label: PropTypes.string.isRequired,
      tooltip: PropTypes.string,
      walker: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.toggleFilter = this.toggleFilter.bind(this);
    this.onClick = this.onClick.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
  }

  shouldComponentUpdate(nextProps) {
    return nextProps.active !== this.props.active;
  }

  toggleFilter() {
    const { dispatch, filterKey, walker, active } = this.props;
    dispatch(filterToggle(filterKey));

    if (!active) {
      dispatch(audit(walker));
    }
  }

  onClick(e) {
    preventDefaultAndStopPropagation(e);
    const { mozInputSource, MOZ_SOURCE_KEYBOARD } = e.nativeEvent;
    if (e.isTrusted && mozInputSource === MOZ_SOURCE_KEYBOARD) {
      // Already handled by key down handler on user input.
      return;
    }

    this.toggleFilter();
  }

  onKeyDown(e) {
    // We explicitely handle "click" and "keydown" events this way here because
    // there seem to be a difference in the sequence of keyboard/click events
    // fired when Space/Enter is pressed. When Space is pressed the sequence of
    // events is keydown->keyup->click but when Enter is pressed the sequence is
    // keydown->click->keyup. This results in an unwanted badge click (when
    // pressing Space) within the accessibility tree row when activating it
    // because it gets focused before the click event is dispatched.
    if (![" ", "Enter"].includes(e.key)) {
      return;
    }

    preventDefaultAndStopPropagation(e);
    this.toggleFilter();
  }

  render() {
    const { active, label, tooltip } = this.props;

    return ToggleButton({
      className: "audit-badge badge",
      label,
      active,
      tooltip,
      onClick: this.onClick,
      onKeyDown: this.onKeyDown,
    });
  }
}

const mapStateToProps = ({ audit: { filters } }, { filterKey }) => ({
  active: filters[filterKey],
});

module.exports = connect(mapStateToProps)(Badge);
