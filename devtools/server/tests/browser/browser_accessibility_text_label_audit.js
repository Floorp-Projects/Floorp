/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Checks functionality around text label audit for the AccessibleActor.
 */

const {
  accessibility: {
    AUDIT_TYPE: { TEXT_LABEL },
    SCORES: { BEST_PRACTICES, FAIL, WARNING },
    ISSUE_TYPE: {
      [TEXT_LABEL]: {
        DIALOG_NO_NAME,
        DOCUMENT_NO_TITLE,
        EMBED_NO_NAME,
        FIGURE_NO_NAME,
        FORM_FIELDSET_NO_NAME,
        FORM_FIELDSET_NO_NAME_FROM_LEGEND,
        FORM_NO_NAME,
        FORM_NO_VISIBLE_NAME,
        FORM_OPTGROUP_NO_NAME_FROM_LABEL,
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
} = require("resource://devtools/shared/constants.js");

add_task(async function () {
  const { target, walker, a11yWalker, parentAccessibility } =
    await initAccessibilityFrontsForUrl(
      `${MAIN_DOMAIN}doc_accessibility_text_label_audit.html`
    );

  const tests = [
    ["Button menu with inner content", "#buttonmenu-1", null],
    ["Button menu nested inside a <label>", "#buttonmenu-2", null],
    [
      "Button menu with no name",
      "#buttonmenu-3",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Button menu with aria-label", "#buttonmenu-4", null],
    ["Button menu with <label>", "#buttonmenu-5", null],
    ["Button menu with aria-labelledby", "#buttonmenu-6", null],
    ["Paragraph with inner content", "#p1", null],
    ["Empty paragraph", "#p2", null],
    [
      "<canvas> with no name",
      "#canvas-1",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    ["<canvas> with aria-label", "#canvas-2", null],
    ["<canvas> with aria-labelledby", "#canvas-3", null],
    [
      "<canvas> with inner content",
      "#canvas-4",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    [
      "Checkbox with no name",
      "#checkbox-1",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Checkbox with unrelated label",
      "#checkbox-2",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    ["Checkbox nested inside a <label>", "#checkbox-3", null],
    ["Checkbox with a label", "#checkbox-4", null],
    [
      "Checkbox with aria-label",
      "#checkbox-5",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    ["Checkbox with aria-labelledby visible label", "#checkbox-6", null],
    [
      "Empty aria checkbox",
      "#checkbox-7",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Aria checkbox with aria-label",
      "#checkbox-8",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    ["Aria checkbox with aria-labelledby visible label", "#checkbox-9", null],
    ["Menuitem checkbox with inner content", "#menuitemcheckbox-1", null],
    [
      "Menuitem checkbox with unlabelled inner content",
      "#menuitemcheckbox-2",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Empty menuitem checkbox",
      "#menuitemcheckbox-3",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Menuitem checkbox with no textual inner content",
      "#menuitemcheckbox-4",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Menuitem checkbox with labelled inner content",
      "#menuitemcheckbox-5",
      null,
    ],
    [
      "Menuitem checkbox with white space inner content",
      "#menuitemcheckbox-6",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Column header with inner content", "#columnheader-1", null],
    [
      "Empty column header",
      "#columnheader-2",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Column header with white space inner content",
      "#columnheader-3",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Column header with aria-label", "#columnheader-4", null],
    [
      "Column header with empty aria-label",
      "#columnheader-5",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Column header with white space aria-label",
      "#columnheader-6",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Column header with aria-labelledby", "#columnheader-7", null],
    ["Aria column header with inner content", "#columnheader-8", null],
    [
      "Empty aria column header",
      "#columnheader-9",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria column header with white space inner content",
      "#columnheader-10",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria column header with aria-label", "#columnheader-11", null],
    [
      "Aria column header with empty aria-label",
      "#columnheader-12",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria column header with white space aria-label",
      "#columnheader-13",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria column header with aria-labelledby", "#columnheader-14", null],
    ["Combobox with a <label>", "#combobox-1", null],
    [
      "Combobox with no label",
      "#combobox-2",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Combobox with unrelated label",
      "#combobox-3",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    ["Combobox nested inside a label", "#combobox-4", null],
    [
      "Combobox with aria-label",
      "#combobox-5",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    ["Combobox with aria-labelledby a visible label", "#combobox-6", null],
    ["Combobox option with inner content", "#combobox-option-1", null],
    [
      "Combobox option with no inner content",
      "#combobox-option-2",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Combobox option with white string inner content",
      "#combobox-option-3",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Combobox option with label attribute", "#combobox-option-4", null],
    [
      "Combobox option with empty label attribute",
      "#combobox-option-5",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Combobox option with white string label attribute",
      "#combobox-option-6",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Svg diagram with no name",
      "#diagram-1",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    [
      "Svg diagram with empty aria-label",
      "#diagram-2",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    ["Svg diagram with aria-label", "#diagram-3", null],
    ["Svg diagram with aria-labelledby", "#diagram-4", null],
    [
      "Svg diagram with aria-labelledby an element with empty content",
      "#diagram-5",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    [
      "Dialog with no name",
      "#dialog-1",
      { score: BEST_PRACTICES, issue: DIALOG_NO_NAME },
    ],
    [
      "Dialog with empty aria-label",
      "#dialog-2",
      { score: BEST_PRACTICES, issue: DIALOG_NO_NAME },
    ],
    ["Dialog with aria-label", "#dialog-3", null],
    ["Dialog with aria-labelledby", "#dialog-4", null],
    [
      "Aria dialog with no name",
      "#dialog-5",
      { score: BEST_PRACTICES, issue: DIALOG_NO_NAME },
    ],
    [
      "Aria dialog with empty aria-label",
      "#dialog-6",
      { score: BEST_PRACTICES, issue: DIALOG_NO_NAME },
    ],
    ["Aria dialog with aria-label", "#dialog-7", null],
    ["Aria dialog with aria-labelledby", "#dialog-8", null],
    [
      "Dialog with aria-labelledby an element with empty content",
      "#dialog-9",
      { score: BEST_PRACTICES, issue: DIALOG_NO_NAME },
    ],
    [
      "Aria dialog with aria-labelledby an element with empty content",
      "#dialog-10",
      { score: BEST_PRACTICES, issue: DIALOG_NO_NAME },
    ],
    [
      "Edit combobox with no name",
      "#editcombobox-1",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Edit combobox with aria-label",
      "#editcombobox-2",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    [
      "Edit combobox with aria-labelled a visible label",
      "#editcombobox-3",
      null,
    ],
    ["Input nested inside a <label>", "#entry-1", null],
    ["Input with no name", "#entry-2", { score: FAIL, issue: FORM_NO_NAME }],
    [
      "Input with aria-label",
      "#entry-3",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    [
      "Input with unrelated <label>",
      "#entry-4",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    ["Input with <label>", "#entry-5", null],
    ["Input with aria-labelledby", "#entry-6", null],
    [
      "Aria textbox with no name",
      "#entry-7",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Aria textbox with aria-label",
      "#entry-8",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    ["Aria textbox with aria-labelledby", "#entry-9", null],
    ["Figure with <figcaption>", "#figure-1", null],
    [
      "Figore with no <figcaption>",
      "#figure-2",
      { score: BEST_PRACTICES, issue: FIGURE_NO_NAME },
    ],
    ["Aria figure with aria-labelledby", "#figure-3", null],
    [
      "Aria figure with aria-labelledby an element with empty content",
      "#figure-4",
      { score: BEST_PRACTICES, issue: FIGURE_NO_NAME },
    ],
    [
      "Aria figure with no name",
      "#figure-5",
      { score: BEST_PRACTICES, issue: FIGURE_NO_NAME },
    ],
    ["Image with no alt text", "#img-1", { score: FAIL, issue: IMAGE_NO_NAME }],
    ["Image with aria-label", "#img-2", null],
    ["Image with aria-labelledby", "#img-3", null],
    ["Image with alt text", "#img-4", null],
    [
      "Image with aria-labelledby an element with empty content",
      "#img-5",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    [
      "Aria image with no name",
      "#img-6",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    ["Aria image with aria-label", "#img-7", null],
    ["Aria image with aria-labelledby", "#img-8", null],
    [
      "Aria image with empty aria-label",
      "#img-9",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    [
      "Aria image with aria-labelledby an element with empty content",
      "#img-10",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    ["<optgroup> with label", "#optgroup-1", null],
    [
      "<optgroup> with empty label",
      "#optgroup-2",
      { score: FAIL, issue: FORM_OPTGROUP_NO_NAME_FROM_LABEL },
    ],
    [
      "<optgroup> with no label",
      "#optgroup-3",
      { score: FAIL, issue: FORM_OPTGROUP_NO_NAME_FROM_LABEL },
    ],
    [
      "<optgroup> with aria-label",
      "#optgroup-4",
      { score: FAIL, issue: FORM_OPTGROUP_NO_NAME_FROM_LABEL },
    ],
    [
      "<optgroup> with aria-labelledby",
      "#optgroup-5",
      { score: FAIL, issue: FORM_OPTGROUP_NO_NAME_FROM_LABEL },
    ],
    ["<fieldset> with <legend>", "#fieldset-1", null],
    [
      "<fieldset> with empty <legend>",
      "#fieldset-2",
      { score: FAIL, issue: FORM_FIELDSET_NO_NAME },
    ],
    [
      "<fieldset> with no <legend>",
      "#fieldset-3",
      { score: FAIL, issue: FORM_FIELDSET_NO_NAME },
    ],
    [
      "<fieldset> with aria-label",
      "#fieldset-4",
      { score: WARNING, issue: FORM_FIELDSET_NO_NAME_FROM_LEGEND },
    ],
    [
      "<fieldset> with aria-labelledby",
      "#fieldset-5",
      { score: WARNING, issue: FORM_FIELDSET_NO_NAME_FROM_LEGEND },
    ],
    ["Empty <h1>", "#heading-1", { score: FAIL, issue: HEADING_NO_NAME }],
    ["<h1> with inner content", "#heading-2", null],
    [
      "<h1> with white space inner content",
      "#heading-3",
      { score: FAIL, issue: HEADING_NO_NAME },
    ],
    [
      "<h1> with aria-label",
      "#heading-4",
      { score: WARNING, issue: HEADING_NO_CONTENT },
    ],
    [
      "<h1> with aria-labelledby",
      "#heading-5",
      { score: WARNING, issue: HEADING_NO_CONTENT },
    ],
    ["<h1> with inner content and aria-label", "#heading-6", null],
    ["<h1> with inner content and aria-labelledby", "#heading-7", null],
    [
      "Empty aria heading",
      "#heading-8",
      { score: FAIL, issue: HEADING_NO_NAME },
    ],
    ["Aria heading with content", "#heading-9", null],
    [
      "Aria heading with white space inner content",
      "#heading-10",
      { score: FAIL, issue: HEADING_NO_NAME },
    ],
    [
      "Aria heading with aria-label",
      "#heading-11",
      { score: WARNING, issue: HEADING_NO_CONTENT },
    ],
    [
      "Aria heading with aria-labelledby",
      "#heading-12",
      { score: WARNING, issue: HEADING_NO_CONTENT },
    ],
    ["Aria heading with inner content and aria-label", "#heading-13", null],
    [
      "Aria heading with inner content and aria-labelledby",
      "#heading-14",
      null,
    ],
    [
      "Image map with no name",
      "#imagemap-1",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    ["Image map with aria-label", "#imagemap-2", null],
    ["Image map with aria-labelledby", "#imagemap-3", null],
    ["Image map with alt attribute", "#imagemap-4", null],
    [
      "Image map with aria-labelledby an element with empty content",
      "#imagemap-5",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    ["<iframe> with title", "#iframe-1", null],
    [
      "<iframe> with empty title",
      "#iframe-2",
      { score: FAIL, issue: IFRAME_NO_NAME_FROM_TITLE },
    ],
    [
      "<iframe> with no title",
      "#iframe-3",
      { score: FAIL, issue: IFRAME_NO_NAME_FROM_TITLE },
    ],
    [
      "<iframe> with aria-label",
      "#iframe-4",
      { score: FAIL, issue: IFRAME_NO_NAME_FROM_TITLE },
    ],
    [
      "<iframe> with aria-label and title",
      "#iframe-5",
      { score: FAIL, issue: IFRAME_NO_NAME_FROM_TITLE },
    ],
    [
      "<object> with image data type and no name",
      "#object-1",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    ["<object> with image data type and aria-label", "#object-2", null],
    ["<object> with image data type and aria-labelledby", "#object-3", null],
    ["<object> with non-image data type", "#object-4", null],
    [
      "<embed> with image data type and no name",
      "#embed-1",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    [
      "<embed> with video data type and no name",
      "#embed-2",
      { score: FAIL, issue: EMBED_NO_NAME },
    ],
    ["<embed> with video data type and aria-label", "#embed-3", null],
    ["<embed> with video data type and aria-labelledby", "#embed-4", null],
    ["Link with no inner content", "#link-1", null],
    ["Link with inner content", "#link-2", null],
    [
      "Link with href and no inner content",
      "#link-3",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Link with href and inner content", "#link-4", null],
    [
      "Link with empty href and no inner content",
      "#link-5",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Link with empty href and inner content", "#link-6", null],
    [
      "Link with # href and no inner content",
      "#link-7",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Link with # href and inner content", "#link-8", null],
    [
      "Link with non empty href and no inner content",
      "#link-9",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Link with non empty href and inner content", "#link-10", null],
    ["Link with aria-label", "#link-11", null],
    ["Link with aria-labelledby", "#link-12", null],
    [
      "Aria link with no inner content",
      "#link-13",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria link with inner content", "#link-14", null],
    ["Aria link with aria-label", "#link-15", null],
    ["Aria link with aria-labelledby", "#link-16", null],
    ["<select> with a visible <label>", "#listbox-1", null],
    [
      "<select> with no name",
      "#listbox-2",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "<select> with unrelated <label>",
      "#listbox-3",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    ["<select> nested inside a <label>", "#listbox-4", null],
    [
      "<select> with aria-label",
      "#listbox-5",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    ["<select> with aria-labelledby a visible element", "#listbox-6", null],
    [
      "MathML glyph with no name",
      "#mglyph-1",
      { score: FAIL, issue: MATHML_GLYPH_NO_NAME },
    ],
    ["MathML glyph with aria-label", "#mglyph-2", null],
    ["MathML glyph with aria-labelledby", "#mglyph-3", null],
    ["MathML glyph with alt text", "#mglyph-4", null],
    [
      "MathML glyph with empty alt text",
      "#mglyph-5",
      { score: FAIL, issue: MATHML_GLYPH_NO_NAME },
    ],
    [
      "MathML glyph with aria-labelledby an element with no inner content",
      "#mglyph-6",
      { score: FAIL, issue: MATHML_GLYPH_NO_NAME },
    ],
    [
      "Aria menu item with no name",
      "#menuitem-1",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria menu item with empty aria-label",
      "#menuitem-2",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria menu item with aria-label", "#menuitem-3", null],
    ["Aria menu item with aria-labelledby", "#menuitem-4", null],
    [
      "Aria menu item with aria-labelledby element with empty inner content",
      "#menuitem-5",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria menu item with inner content", "#menuitem-6", null],
    ["Option with inner content", "#option-1", null],
    [
      "Option with no inner content",
      "#option-2",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Option with white space inner ",
      "#option-3",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Option with a label", "#option-4", null],
    [
      "Option with an empty label",
      "#option-5",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Option with a white space label",
      "#option-6",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria option with inner content", "#option-7", null],
    [
      "Aria option with no inner content",
      "#option-8",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria option with white space inner content",
      "#option-9",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria option with aria-label", "#option-10", null],
    [
      "Aria option with empty aria-label",
      "#option-11",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria option with white space aria-label",
      "#option-12",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria option with aria-labelledby", "#option-13", null],
    [
      "Aria option with aria-labelledby an element with empty content",
      "#option-14",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria option with aria-labelledby an element with white space content",
      "#option-15",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Empty aria treeitem",
      "#treeitem-1",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria treeitem with empty aria-label",
      "#treeitem-2",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria treeitem with aria-label", "#treeitem-3", null],
    ["Aria treeitem with aria-labelledby", "#treeitem-4", null],
    [
      "Aria treeitem with aria-labelledby an element with empty content",
      "#treeitem-5",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria treeitem with inner content", "#treeitem-6", null],
    [
      "Aria tab with no content",
      "#tab-1",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria tab with empty aria-label",
      "#tab-2",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria tab with aria-label", "#tab-3", null],
    ["Aria tab with aria-labelledby", "#tab-4", null],
    [
      "Aria tab with aria-labelledby an element with empty content",
      "#tab-5",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria tab with inner content", "#tab-6", null],
    ["Password nested inside a <label>", "#password-1", null],
    ["Password no name", "#password-2", { score: FAIL, issue: FORM_NO_NAME }],
    [
      "Password with aria-label",
      "#password-3",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    [
      "Password with unrelated label",
      "#password-4",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    ["Password with <label>", "#password-5", null],
    ["Password with aria-labelledby a visible element", "#password-6", null],
    ["<progress> nested inside a label", "#progress-1", null],
    [
      "<progress> with no name",
      "#progress-2",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "<progress> with aria-label",
      "#progress-3",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    [
      "<progress> with unrelated <label>",
      "#progress-4",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    ["<progress> with <label>", "#progress-5", null],
    ["<progress> with aria-labelledby a visible element", "#progress-6", null],
    [
      "Aria progressbar nested inside a <label>",
      "#progress-7",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Aria progressbar with aria-labelledby a visible element",
      "#progress-8",
      null,
    ],
    [
      "Aria progressbar no name",
      "#progress-9",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Aria progressbar with aria-label",
      "#progress-10",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    [
      "Aria progressbar with unrelated <label>",
      "#progress-11",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Aria progressbar with <label>",
      "#progress-12",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Aria progressbar with aria-labelledby a visible <label>",
      "#progress-13",
      null,
    ],
    ["Button with inner content", "#button-1", null],
    [
      "Image button with no name",
      "#button-2",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Button with no name",
      "#button-3",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Image button with empty alt text",
      "#button-4",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Image button with alt text", "#button-5", null],
    [
      "Button with white space inner content",
      "#button-6",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Button inside a <label>", "#button-7", null],
    ["Button with aria-label", "#button-8", null],
    [
      "Button with unrelated <label>",
      "#button-9",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Button with <label>", "#button-10", null],
    ["Button with aria-labelledby a visile <label>", "#button-11", null],
    [
      "Aria button inside a label",
      "#button-12",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria button with aria-labelled by a <label>", "#button-13", null],
    [
      "Aria button with no content",
      "#button-14",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria button with aria-label", "#button-15", null],
    [
      "Aria button with unrelated <label>",
      "#button-16",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria button with <label>",
      "#button-17",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria button with aria-labelledby a visible <label>", "#button-18", null],
    ["Radio nested inside a label", "#radiobutton-1", null],
    [
      "Radio with no name",
      "#radiobutton-2",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Radio with aria-label",
      "#radiobutton-3",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    [
      "Radio with unrelated <label>",
      "#radiobutton-4",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    ["Radio with visible label>", "#radiobutton-5", null],
    ["Radio with aria-labelledby a visible <label>", "#radiobutton-6", null],
    [
      "Aria radio with no name",
      "#radiobutton-7",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Aria radio with aria-label",
      "#radiobutton-8",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    [
      "Aria radio with aria-labelledby a visible element",
      "#radiobutton-9",
      null,
    ],
    ["Aria menuitemradio with inner content", "#menuitemradio-1", null],
    [
      "Aria menuitemradio with no inner content",
      "#menuitemradio-2",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria menuitemradio with white space inner content",
      "#menuitemradio-3",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Rowheader with inner content", "#rowheader-1", null],
    [
      "Rowheader with no inner content",
      "#rowheader-2",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Rowheader with white space inner content",
      "#rowheader-3",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Rowheader with aria-label", "#rowheader-4", null],
    [
      "Rowheader with empty aria-label",
      "#rowheader-5",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Rowheader with white space aria-label",
      "#rowheader-6",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Rowheader with aria-labelledby", "#rowheader-7", null],
    ["Aria rowheader with inner content", "#rowheader-8", null],
    [
      "Aria rowheader with no inner content",
      "#rowheader-9",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria rowheader with white space inner content",
      "#rowheader-10",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria rowheader with aria-label", "#rowheader-11", null],
    [
      "Aria rowheader with empty aria-label",
      "#rowheader-12",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria rowheader with white space aria-label",
      "#rowheader-13",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria rowheader with aria-labelledby", "#rowheader-14", null],
    ["Slider nested inside a <label>", "#slider-1", null],
    ["Slider with no name", "#slider-2", { score: FAIL, issue: FORM_NO_NAME }],
    [
      "Slider with aria-label",
      "#slider-3",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    [
      "Slider with unrelated <label>",
      "#slider-4",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    ["Slider with a visible <label>", "#slider-5", null],
    ["Slider with aria-labelled by a visible <label>", "#slider-6", null],
    [
      "Aria slider with no name",
      "#slider-7",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Aria slider with aria-label",
      "#slider-8",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    ["Aria slider with aria-labelledby a visible element", "#slider-9", null],
    ["Number input inside a label", "#spinbutton-1", null],
    [
      "Number input with no label",
      "#spinbutton-2",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Number input with aria-label",
      "#spinbutton-3",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    [
      "Number input with unrelated <label>",
      "#spinbutton-4",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    ["Number input with visible <label>", "#spinbutton-5", null],
    [
      "Number input with aria-labelled by a visible <label>",
      "#spinbutton-6",
      null,
    ],
    [
      "Aria spinbutton with no name",
      "#spinbutton-7",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Aria spinbutton with aria-label",
      "#spinbutton-8",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    [
      "Aria spinbutton with aria-labelledby a visible element",
      "#spinbutton-9",
      null,
    ],
    [
      "Aria switch with no name",
      "#switch-1",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Aria switch wtih aria-label",
      "#switch-2",
      { score: WARNING, issue: FORM_NO_VISIBLE_NAME },
    ],
    ["Aria switch with aria-labelledby a visible element", "#switch-3", null],
    [
      "Aria switch with unrelated <label>",
      "#switch-4",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    [
      "Aria switch nested inside a <label>",
      "#switch-5",
      { score: FAIL, issue: FORM_NO_NAME },
    ],
    // See bug: https://bugzilla.mozilla.org/show_bug.cgi?id=559770
    // ["Meter inside a label", "#meter-1", null],
    // See bug: https://bugzilla.mozilla.org/show_bug.cgi?id=559770
    // ["Meter with no name", "#meter-2", { score: FAIL, issue: FORM_NO_NAME }],
    // See bug: https://bugzilla.mozilla.org/show_bug.cgi?id=559770
    // ["Meter with aria-label", "#meter-3",
    //   { score: WARNING, issue: FORM_NO_VISIBLE_NAME}],
    // See bug: https://bugzilla.mozilla.org/show_bug.cgi?id=559770
    // ["Meter with unrelated <label>", "#meter-4", { score: FAIL, issue: FORM_NO_NAME }],
    ["Meter with visible <label>", "#meter-5", null],
    ["Meter with aria-labelledby a visible <label>", "#meter-6", null],
    // See bug: https://bugzilla.mozilla.org/show_bug.cgi?id=559770
    // ["Aria meter with no name", "#meter-7", { score: FAIL, issue: FORM_NO_NAME }],
    // See bug: https://bugzilla.mozilla.org/show_bug.cgi?id=559770
    // ["Aria meter with aria-label", "#meter-8",
    //   { score: WARNING, issue: FORM_NO_VISIBLE_NAME}],
    ["Aria meter with aria-labelledby a visible element", "#meter-9", null],
    ["Toggle button with inner content", "#togglebutton-1", null],
    [
      "Image toggle button with no name",
      "#togglebutton-2",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Empty toggle button",
      "#togglebutton-3",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Image toggle button with empty alt text",
      "#togglebutton-4",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Image toggle button with alt text", "#togglebutton-5", null],
    [
      "Toggle button with white space inner content",
      "#togglebutton-6",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Toggle button nested inside a label", "#togglebutton-7", null],
    ["Toggle button with aria-label", "#togglebutton-8", null],
    [
      "Toggle button with unrelated <label>",
      "#togglebutton-9",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Toggle button with <label>", "#togglebutton-10", null],
    [
      "Toggle button with aria-labelled by a visible <label>",
      "#togglebutton-11",
      null,
    ],
    [
      "Aria toggle button nested inside a label",
      "#togglebutton-12",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria toggle button with aria-labelled by and nested inside a label",
      "#togglebutton-13",
      null,
    ],
    [
      "Aria toggle button with no name",
      "#togglebutton-14",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    ["Aria toggle button with aria-label", "#togglebutton-15", null],
    [
      "Aria toggle button with unrelated <label>",
      "#togglebutton-16",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria toggle button with <label>",
      "#togglebutton-17",
      { score: FAIL, issue: INTERACTIVE_NO_NAME },
    ],
    [
      "Aria toggle button with aria-labelledby a visible <label>",
      "#togglebutton-18",
      null,
    ],
    ["Non-unique aria toolbar with aria-label", "#toolbar-1", null],
    [
      "Non-unique aria toolbar with no name (",
      "#toolbar-2",
      { score: FAIL, issue: TOOLBAR_NO_NAME },
    ],
    [
      "Non-unique aAria toolbar with aria-labelledby an element with empty content",
      "#toolbar-3",
      { score: FAIL, issue: TOOLBAR_NO_NAME },
    ],
    ["Non-unique aria toolbar with aria-labelledby", "#toolbar-4", null],
    ["SVGElement with role=img that has a title", "#svg-1", null],
    ["SVGElement without role=img that has a title", "#svg-2", null],
    [
      "SVGElement with role=img and no name",
      "#svg-3",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    [
      "SVGElement with no name",
      "#svg-4",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    ["SVGElement with a name", "#svg-5", null],
    [
      "SVGElement with a name and with ownerSVGElement with a name",
      "#svg-6",
      null,
    ],
    ["SVGElement with a title", "#svg-7", null],
    [
      "SVGElement with a name and with ownerSVGElement with a title",
      "#svg-8",
      null,
    ],
    ["SVGElement with role=img that has a title", "#svg-9", null],
    [
      "SVGElement with a name and with ownerSVGElement with role=img that has a title",
      "#svg-10",
      null,
    ],
    [
      "SVGElement with role=img and no title",
      "#svg-11",
      { score: FAIL, issue: IMAGE_NO_NAME },
    ],
    [
      "SVGElement with a name and with ownerSVGElement with role=img and no title",
      "#svg-12",
      null,
    ],
  ];

  for (const [description, selector, expected] of tests) {
    info(description);
    const node = await walker.querySelector(walker.rootNode, selector);
    const front = await a11yWalker.getAccessibleFor(node);
    const audit = await front.audit({ types: [TEXT_LABEL] });
    Assert.deepEqual(
      audit[TEXT_LABEL],
      expected,
      `Audit result for ${selector} is correct.`
    );
  }

  info("Test document rule:");
  const front = await a11yWalker.getAccessibleFor(walker.rootNode);
  let audit = await front.audit({ types: [TEXT_LABEL] });
  info("Document with no title");
  Assert.deepEqual(
    audit[TEXT_LABEL],
    { score: FAIL, issue: DOCUMENT_NO_TITLE },
    "Audit result for document is correct."
  );

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.document.title = "Hello world";
  });
  audit = await front.audit({ types: [TEXT_LABEL] });
  info("Document with title");
  Assert.deepEqual(
    audit[TEXT_LABEL],
    null,
    "Audit result for document is correct."
  );

  await waitForA11yShutdown(parentAccessibility);
  await target.destroy();
  gBrowser.removeCurrentTab();
});
