/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
/* global classnames FxButton InfoBox PropTypes r React remoteValues sendPageEvent */

window.ShieldStudies = class ShieldStudies extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      learnMoreHref: null,
    };
  }

  componentDidMount() {
    remoteValues.ShieldLearnMoreHref.subscribe(this);
  }

  componentWillUnmount() {
    remoteValues.ShieldLearnMoreHref.unsubscribe(this);
  }

  receiveRemoteValue(name, value) {
    this.setState({
      learnMoreHref: value,
    });
  }

  render() {
    return (
      r("div", {},
        r(InfoBox, {},
          r("span", {}, "What's this? Firefox may install and run studies from time to time."),
          r("a", {id: "shield-studies-learn-more", href: this.state.learnMoreHref}, "Learn more"),
          r(UpdatePreferencesButton, {}, "Update Preferences"),
        ),
        r(StudyList),
      )
    );
  }
};

class UpdatePreferencesButton extends React.Component {
  constructor(props) {
    super(props);
    this.handleClick = this.handleClick.bind(this);
  }

  handleClick() {
    sendPageEvent("NavigateToDataPreferences");
  }

  render() {
    return r(
      FxButton,
      Object.assign({
        id: "shield-studies-update-preferences",
        onClick: this.handleClick,
      }, this.props),
    );
  }
}

class StudyList extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      studies: [],
    };
  }

  componentDidMount() {
    remoteValues.StudyList.subscribe(this);
  }

  componentWillUnmount() {
    remoteValues.StudyList.unsubscribe(this);
  }

  receiveRemoteValue(name, value) {
    const studies = value.slice();

    // Sort by active status, then by start date descending.
    studies.sort((a, b) => {
      if (a.active !== b.active) {
        return a.active ? -1 : 1;
      }
      return b.studyStartDate - a.studyStartDate;
    });

    this.setState({studies});
  }

  render() {
    return (
      r("ul", {className: "study-list"},
        this.state.studies.map(study => (
          r(StudyListItem, {key: study.name, study})
        ))
      )
    );
  }
}

class StudyListItem extends React.Component {
  constructor(props) {
    super(props);
    this.handleClickRemove = this.handleClickRemove.bind(this);
  }

  handleClickRemove() {
    sendPageEvent("RemoveStudy", this.props.study.recipeId);
  }

  render() {
    const study = this.props.study;
    return (
      r("li", {
        className: classnames("study", {disabled: !study.active}),
        "data-study-name": study.name,
      },
        r("div", {className: "study-icon"},
          study.name.slice(0, 1)
        ),
        r("div", {className: "study-details"},
          r("div", {className: "study-name"}, study.name),
          r("div", {className: "study-description", title: study.description},
            r("span", {className: "study-status"}, study.active ? "Active" : "Complete"),
            r("span", {}, "\u2022"), // &bullet;
            r("span", {}, study.description),
          ),
        ),
        r("div", {className: "study-actions"},
          study.active &&
            r(FxButton, {className: "remove-button", onClick: this.handleClickRemove}, "Remove"),
        ),
      )
    );
  }
}
StudyListItem.propTypes = {
  study: PropTypes.shape({
    recipeId: PropTypes.number.isRequired,
    name: PropTypes.string.isRequired,
    active: PropTypes.boolean,
    description: PropTypes.string.isRequired,
  }).isRequired,
};
