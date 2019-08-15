/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const ReactDOM = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const { openDocLink } = require("devtools/client/shared/link");

const { A11Y_TEXT_LABEL_LINKS } = require("../constants");

const {
  accessibility: {
    AUDIT_TYPE: { TEXT_LABEL },
    ISSUE_TYPE: {
      [TEXT_LABEL]: {
        AREA_NO_NAME_FROM_ALT,
        DIALOG_NO_NAME,
        DOCUMENT_NO_TITLE,
        EMBED_NO_NAME,
        FIGURE_NO_NAME,
        FORM_FIELDSET_NO_NAME,
        FORM_FIELDSET_NO_NAME_FROM_LEGEND,
        FORM_NO_NAME,
        FORM_NO_VISIBLE_NAME,
        FORM_OPTGROUP_NO_NAME_FROM_LABEL,
        FRAME_NO_NAME,
        HEADING_NO_CONTENT,
        HEADING_NO_NAME,
        IFRAME_NO_NAME_FROM_TITLE,
        IMAGE_NO_NAME,
        INTERACTIVE_NO_NAME,
        MATHML_GLYPH_NO_NAME,
        TOOLBAR_NO_NAME,
      },
    },
    SCORES: { BEST_PRACTICES, FAIL, WARNING },
  },
} = require("devtools/shared/constants");

/**
 * A map from text label issues to annotation component properties.
 */
const ISSUE_TO_ANNOTATION_MAP = {
  [AREA_NO_NAME_FROM_ALT]: {
    href: A11Y_TEXT_LABEL_LINKS.AREA_NO_NAME_FROM_ALT,
    l10nId: "accessibility-text-label-issue-area",
    args: {
      get code() {
        return ReactDOM.code({}, "alt");
      },
      // Note: there is no way right now to use custom elements in privileged
      // content. We have to use something like <div> since we can't provide
      // three args with the same name.
      get div() {
        return ReactDOM.code({}, "area");
      },
      // Note: there is no way right now to use custom elements in privileged
      // content. We have to use something like <span> since we can't provide
      // three args with the same name.
      get span() {
        return ReactDOM.code({}, "href");
      },
    },
  },
  [DIALOG_NO_NAME]: {
    href: A11Y_TEXT_LABEL_LINKS.DIALOG_NO_NAME,
    l10nId: "accessibility-text-label-issue-dialog",
  },
  [DOCUMENT_NO_TITLE]: {
    href: A11Y_TEXT_LABEL_LINKS.DOCUMENT_NO_TITLE,
    l10nId: "accessibility-text-label-issue-document-title",
    args: {
      get code() {
        return ReactDOM.code({}, "title");
      },
    },
  },
  [EMBED_NO_NAME]: {
    href: A11Y_TEXT_LABEL_LINKS.EMBED_NO_NAME,
    l10nId: "accessibility-text-label-issue-embed",
  },
  [FIGURE_NO_NAME]: {
    href: A11Y_TEXT_LABEL_LINKS.FIGURE_NO_NAME,
    l10nId: "accessibility-text-label-issue-figure",
  },
  [FORM_FIELDSET_NO_NAME]: {
    href: A11Y_TEXT_LABEL_LINKS.FORM_FIELDSET_NO_NAME,
    l10nId: "accessibility-text-label-issue-fieldset",
    args: {
      get code() {
        return ReactDOM.code({}, "fieldset");
      },
    },
  },
  [FORM_FIELDSET_NO_NAME_FROM_LEGEND]: {
    href: A11Y_TEXT_LABEL_LINKS.FORM_FIELDSET_NO_NAME_FROM_LEGEND,
    l10nId: "accessibility-text-label-issue-fieldset-legend2",
    args: {
      get code() {
        return ReactDOM.code({}, "legend");
      },
      // Note: there is no way right now to use custom elements in privileged
      // content. We have to use something like <span> since we can't provide
      // two args with the same name.
      get span() {
        return ReactDOM.code({}, "fieldset");
      },
    },
  },
  [FORM_NO_NAME]: {
    href: A11Y_TEXT_LABEL_LINKS.FORM_NO_NAME,
    l10nId: "accessibility-text-label-issue-form",
  },
  [FORM_NO_VISIBLE_NAME]: {
    href: A11Y_TEXT_LABEL_LINKS.FORM_NO_VISIBLE_NAME,
    l10nId: "accessibility-text-label-issue-form-visible",
  },
  [FORM_OPTGROUP_NO_NAME_FROM_LABEL]: {
    href: A11Y_TEXT_LABEL_LINKS.FORM_OPTGROUP_NO_NAME_FROM_LABEL,
    l10nId: "accessibility-text-label-issue-optgroup-label2",
    args: {
      get code() {
        return ReactDOM.code({}, "label");
      },
      // Note: there is no way right now to use custom elements in privileged
      // content. We have to use something like <span> since we can't provide
      // two args with the same name.
      get span() {
        return ReactDOM.code({}, "optgroup");
      },
    },
  },
  [FRAME_NO_NAME]: {
    href: A11Y_TEXT_LABEL_LINKS.FRAME_NO_NAME,
    l10nId: "accessibility-text-label-issue-frame",
    args: {
      get code() {
        return ReactDOM.code({}, "frame");
      },
    },
  },
  [HEADING_NO_CONTENT]: {
    href: A11Y_TEXT_LABEL_LINKS.HEADING_NO_CONTENT,
    l10nId: "accessibility-text-label-issue-heading-content",
  },
  [HEADING_NO_NAME]: {
    href: A11Y_TEXT_LABEL_LINKS.HEADING_NO_NAME,
    l10nId: "accessibility-text-label-issue-heading",
  },
  [IFRAME_NO_NAME_FROM_TITLE]: {
    href: A11Y_TEXT_LABEL_LINKS.IFRAME_NO_NAME_FROM_TITLE,
    l10nId: "accessibility-text-label-issue-iframe",
    args: {
      get code() {
        return ReactDOM.code({}, "title");
      },
      // Note: there is no way right now to use custom elements in privileged
      // content. We have to use something like <span> since we can't provide
      // two args with the same name.
      get span() {
        return ReactDOM.code({}, "iframe");
      },
    },
  },
  [IMAGE_NO_NAME]: {
    href: A11Y_TEXT_LABEL_LINKS.IMAGE_NO_NAME,
    l10nId: "accessibility-text-label-issue-image",
  },
  [INTERACTIVE_NO_NAME]: {
    href: A11Y_TEXT_LABEL_LINKS.INTERACTIVE_NO_NAME,
    l10nId: "accessibility-text-label-issue-interactive",
  },
  [MATHML_GLYPH_NO_NAME]: {
    href: A11Y_TEXT_LABEL_LINKS.MATHML_GLYPH_NO_NAME,
    l10nId: "accessibility-text-label-issue-glyph",
    args: {
      get code() {
        return ReactDOM.code({}, "alt");
      },
      // Note: there is no way right now to use custom elements in privileged
      // content. We have to use something like <span> since we can't provide
      // two args with the same name.
      get span() {
        return ReactDOM.code({}, "mglyph");
      },
    },
  },
  [TOOLBAR_NO_NAME]: {
    href: A11Y_TEXT_LABEL_LINKS.TOOLBAR_NO_NAME,
    l10nId: "accessibility-text-label-issue-toolbar",
  },
};

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
class LearnMoreClass extends Component {
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
 * Renders icon with text description for the text label accessibility check.
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
 * Renders text description of the text label accessibility check.
 *
 * @param {Object}
 *        Options:
 *          - issue: value from ISSUE_TYPE[AUDIT_TYPE.TEXT_LABEL] from
 *                   "devtools/shared/constants"
 */
function Annotation({ issue }) {
  const { args, href, l10nId } = ISSUE_TO_ANNOTATION_MAP[issue];

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
 * Component for rendering a check for text label accessibliity check failures,
 * warnings and best practices suggestions association with a given
 * accessibility object in the accessibility tree.
 */
class TextLabelCheck extends Component {
  static get propTypes() {
    return {
      issue: PropTypes.string.isRequired,
      score: PropTypes.string.isRequired,
    };
  }

  render() {
    const { issue, score } = this.props;

    return ReactDOM.div(
      {
        role: "presentation",
        className: "accessibility-check",
      },
      Localized(
        {
          id: "accessibility-text-label-header",
        },
        ReactDOM.h3({ className: "accessibility-check-header" })
      ),
      ReactDOM.div(
        {
          role: "presentation",
          className: "accessibility-text-label-check",
        },
        Icon({ score }),
        Annotation({ issue })
      )
    );
  }
}

module.exports = TextLabelCheck;
