/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const ReactDOM = require("devtools/client/shared/vendor/react-dom-factories");

const Check = createFactory(
  require("devtools/client/accessibility/components/Check")
);

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
 * Component for rendering a check for text label accessibliity check failures,
 * warnings and best practices suggestions association with a given
 * accessibility object in the accessibility tree.
 */
class TextLabelCheck extends PureComponent {
  static get propTypes() {
    return {
      issue: PropTypes.string.isRequired,
      score: PropTypes.string.isRequired,
    };
  }

  render() {
    return Check({
      ...this.props,
      getAnnotation: issue => ISSUE_TO_ANNOTATION_MAP[issue],
      id: "accessibility-text-label-header",
    });
  }
}

module.exports = TextLabelCheck;
