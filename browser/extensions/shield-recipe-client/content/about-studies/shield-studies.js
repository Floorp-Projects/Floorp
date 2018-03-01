/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
/* global classnames FxButton InfoBox PropTypes r React remoteValues sendPageEvent */

window.ShieldStudies = class ShieldStudies extends React.Component {

  render() {
    return (
      r("div", {},
        r(WhatsThisBox),
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
      studies: null,
    };
  }

  componentDidMount() {
    remoteValues.studyList.subscribe(this);
  }

  componentWillUnmount() {
    remoteValues.studyList.unsubscribe(this);
  }

  receiveRemoteValue(name, value) {
    if (value) {
      const studies = value.slice();

      // Sort by active status, then by start date descending.
      studies.sort((a, b) => {
        if (a.active !== b.active) {
          return a.active ? -1 : 1;
        }
        return b.studyStartDate - a.studyStartDate;
      });

      this.setState({studies});
    } else {
      this.setState({studies: value});
    }
  }

  render() {
    const { studies } = this.state;

    if (studies === null) {
      // loading
      return null;
    }

    let info = null;
    if (studies.length === 0) {
      info = r("p", {className: "study-list-info"}, "You have not participated in any studies.");
    }

    return (
      r("div", {},
        info,
        r("ul", {className: "study-list"},
          this.state.studies.map(study => (
            r(StudyListItem, {key: study.name, study})
          ))
        ),
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
    sendPageEvent("RemoveStudy", {recipeId: this.props.study.recipeId, reason: "individual-opt-out"});
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

class WhatsThisBox extends React.Component {
  constructor(props) {
    super(props);
    this.state = {
      learnMoreHref: null,
      studiesEnabled: null,
    };
  }

  componentDidMount() {
    remoteValues.shieldLearnMoreHref.subscribe(this);
    remoteValues.studiesEnabled.subscribe(this);
  }

  componentWillUnmount() {
    remoteValues.shieldLearnMoreHref.unsubscribe(this);
    remoteValues.studiesEnabled.unsubscribe(this);
  }

  receiveRemoteValue(name, value) {
    switch (name) {
      case "ShieldLearnMoreHref": {
        this.setState({ learnMoreHref: value });
        break;
      }
      case "StudiesEnabled": {
        this.setState({ studiesEnabled: value });
        break;
      }
      default: {
        console.error(`Unknown remote value ${name}`);
      }
    }
  }

  render() {
    const { learnMoreHref, studiesEnabled } = this.state;

    let message = null;

    // studiesEnabled can be null, in which case do nothing
    if (studiesEnabled === false) {
      message = r("span", {}, "This is a list of studies that you have participated in. No new studies will run.");
    } else if (studiesEnabled === true) {
      message = r("span", {}, "What's this? Firefox may install and run studies from time to time.");
    }

    return (
      r(InfoBox, {},
        message,
        r("a", {id: "shield-studies-learn-more", href: learnMoreHref}, "Learn more"),
        r(UpdatePreferencesButton, {}, "Update Preferences"),
       )
    );
  }
}
