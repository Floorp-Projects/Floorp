/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Constants used in various panels, shared between client and the server.
 */

/* Accessibility Panel ====================================================== */

// List of audit types.
const AUDIT_TYPE = {
  CONTRAST: "CONTRAST",
  TEXT_LABEL: "TEXT_LABEL",
};

// Types of issues grouped by audit types.
const ISSUE_TYPE = {
  [AUDIT_TYPE.TEXT_LABEL]: {
    // <AREA> name is provided via "alt" attribute.
    AREA_NO_NAME_FROM_ALT: "AREA_NO_NAME_FROM_ALT",
    // Dialog name is not provided.
    DIALOG_NO_NAME: "DIALOG_NO_NAME",
    // Document title is not provided.
    DOCUMENT_NO_TITLE: "DOCUMENT_NO_TITLE",
    // <EMBED> name is not provided.
    EMBED_NO_NAME: "EMBED_NO_NAME",
    // <FIGURE> name is not provided.
    FIGURE_NO_NAME: "FIGURE_NO_NAME",
    // <FIELDSET> name is not provided.
    FORM_FIELDSET_NO_NAME: "FORM_FIELDSET_NO_NAME",
    // <FIELDSET> name is not provided via <LEGEND> element.
    FORM_FIELDSET_NO_NAME_FROM_LEGEND: "FORM_FIELDSET_NO_NAME_FROM_LEGEND",
    // Form element's name is not provided.
    FORM_NO_NAME: "FORM_NO_NAME",
    // Form element's name is not visible.
    FORM_NO_VISIBLE_NAME: "FORM_NO_VISIBLE_NAME",
    // <OPTGROUP> name is not provided via "label" attribute.
    FORM_OPTGROUP_NO_NAME_FROM_LABEL: "FORM_OPTGROUP_NO_NAME_FROM_LABEL",
    // <FRAME> name is not provided.
    FRAME_NO_NAME: "FRAME_NO_NAME",
    // <H{1, 2, ...}> has no content.
    HEADING_NO_CONTENT: "HEADING_NO_CONTENT",
    // <H{1, 2, ...}> name is not provided.
    HEADING_NO_NAME: "HEADING_NO_NAME",
    // <IFRAME> name is not provided via "title" attribute.
    IFRAME_NO_NAME_FROM_TITLE: "IFRAME_NO_NAME_FROM_TITLE",
    // <IMG> name is not provided (including empty name).
    IMAGE_NO_NAME: "IMAGE_NO_NAME",
    // Interactive element's name is not provided.
    INTERACTIVE_NO_NAME: "INTERACTIVE_NO_NAME",
    // <MGLYPH> name is no provided.
    MATHML_GLYPH_NO_NAME: "MATHML_GLYPH_NO_NAME",
    // Toolbar's name is not provided when more than one toolbar is present.
    TOOLBAR_NO_NAME: "TOOLBAR_NO_NAME",
  },
};

// Constants associated with WCAG guidelines score system.
const SCORES = {
  // Satisfies WCAG AA guidelines.
  AA: "AA",
  // Satisfies WCAG AAA guidelines.
  AAA: "AAA",
  // Elevates accessibility experience.
  BEST_PRACTICES: "BEST_PRACTICES",
  // Does not satisfy the baseline WCAG guidelines.
  FAIL: "fail",
  // Partially satisfies the WCAG AA guidelines.
  WARNING: "WARNING",
};

exports.accessibility = {
  AUDIT_TYPE,
  ISSUE_TYPE,
  SCORES,
};
