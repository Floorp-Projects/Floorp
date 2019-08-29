/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const {
  Component,
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const ReactDOM = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const { openDocLink } = require("devtools/client/shared/link");

const {
  accessibility: {
    SCORES: { BEST_PRACTICES, FAIL, WARNING },
  },
} = require("devtools/shared/constants");

/**
 * A map of accessibility scores to the text descriptions of check icons.
 */
const SCORE_TO_ICON_MAP = {
  [BEST_PRACTICES]: {
    l10nId: "accessibility-best-practices",
    src: "chrome://devtools/skin/images/info.svg",
  },
  [FAIL]: {
    l10nId: "accessibility-fail",
    src: "chrome://devtools/skin/images/error.svg",
  },
  [WARNING]: {
    l10nId: "accessibility-warning",
    src: "chrome://devtools/skin/images/alert.svg",
  },
};

/**
 * Localized "Learn more" link that opens a new tab with relevant documentation.
 */
class LearnMoreClass extends PureComponent {
  static get propTypes() {
    return {
      href: PropTypes.string,
      l10nId: PropTypes.string.isRequired,
      onClick: PropTypes.func,
    };
  }

  static get defaultProps() {
    return {
      href: "#",
      l10nId: null,
      onClick: LearnMoreClass.openDocOnClick,
    };
  }

  static openDocOnClick(event) {
    event.preventDefault();
    openDocLink(event.target.href);
  }

  render() {
    const { href, l10nId, onClick } = this.props;
    const className = "link";

    return Localized({ id: l10nId }, ReactDOM.a({ className, href, onClick }));
  }
}

const LearnMore = createFactory(LearnMoreClass);

/**
 * Renders icon with text description for the accessibility check.
 *
 * @param {Object}
 *        Options:
 *          - score: value from SCORES from "devtools/shared/constants"
 */
function Icon({ score }) {
  const { l10nId, src } = SCORE_TO_ICON_MAP[score];

  return Localized(
    { id: l10nId, attrs: { alt: true } },
    ReactDOM.img({ src, className: `icon ${score}` })
  );
}

/**
 * Renders text description of the accessibility check.
 *
 * @param {Object}
 *        Options:
 *          - args:   arguments for fluent localized string
 *          - href:   url for the learn more link pointing to MDN
 *          - l10nId: fluent localization id
 */
function Annotation({ args, href, l10nId }) {
  return Localized(
    {
      id: l10nId,
      a: LearnMore({ l10nId: "accessibility-learn-more", href }),
      ...args,
    },
    ReactDOM.p({ className: "accessibility-check-annotation" })
  );
}

/**
 * Component for rendering a check for accessibliity checks section,
 * warnings and best practices suggestions association with a given
 * accessibility object in the accessibility tree.
 */
class Check extends Component {
  static get propTypes() {
    return {
      getAnnotation: PropTypes.func.isRequired,
      id: PropTypes.string.isRequired,
      issue: PropTypes.string.isRequired,
      score: PropTypes.string.isRequired,
    };
  }

  render() {
    const { getAnnotation, id, issue, score } = this.props;

    return ReactDOM.div(
      {
        role: "presentation",
        className: "accessibility-check",
      },
      Localized(
        {
          id,
        },
        ReactDOM.h3({ className: "accessibility-check-header" })
      ),
      ReactDOM.div(
        {
          role: "presentation",
        },
        Icon({ score }),
        Annotation({ ...getAnnotation(issue) })
      )
    );
  }
}

module.exports = Check;
