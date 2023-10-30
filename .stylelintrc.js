/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

"use strict";

const fs = require("fs");
const path = require("path");

function readFile(path) {
  return fs
    .readFileSync(path, { encoding: "utf-8" })
    .split("\n")
    .filter(p => p && !p.startsWith("#"));
}

const ignoreFiles = [
  ...readFile(
    path.join(__dirname, "tools", "rewriting", "ThirdPartyPaths.txt")
  ),
  ...readFile(path.join(__dirname, "tools", "rewriting", "Generated.txt")),
];

module.exports = {
  extends: ["stylelint-config-recommended"],
  ignoreFiles,
  rules: {
    /* Disabled because of `-moz-element(#foo)` which gets misparsed. */
    "color-no-invalid-hex": null,
    "font-family-no-missing-generic-family-keyword": [
      true,
      {
        ignoreFontFamilies: [
          "-moz-button",
          "-moz-field",
          "-moz-fixed",
          "-moz-list",
          "caption",
        ],
      },
    ],

    "function-no-unknown": [
      true,
      {
        ignoreFunctions: [
          "light-dark" /* Used for color-scheme dependent colors */,
          "add" /* Used in mathml.css */,
        ],
      },
    ],

    "max-nesting-depth": [
      3,
      {
        ignore: ["blockless-at-rules"],
      },
    ],

    "no-descending-specificity": null,
    "no-duplicate-selectors": null,

    "property-no-unknown": [
      true,
      {
        ignoreProperties: ["overflow-clip-box"],
      },
    ],

    /*
     * XXXgijs: we would like to enable this, but we can't right now.
     * This is because Gecko uses a number of custom pseudoclasses,
     * and stylelint assumes that for `:unknown-pseudoclass(foo)`,
     * `foo` should be a known type.
     * This is tedious but workable for things like `-moz-locale-dir` where
     * the set of acceptable values (ltr/rtl) is small.
     * However, for tree cells, the set of values is unlimited (ie
     * user-defined, based on atoms sent by the JS tree view APIs).
     * There does not appear to be a way to exempt the contents of these
     * unknown pseudoclasses, and as a result, this rule is not
     * usable for us. The 'type' only includes the contents of the
     * pseudoclass, not the pseudo itself, so we can't filter based on the
     * pseudoclass either.
     * Ideally, we would either create an option to the builtin rule
     * in stylelint itself, or mimic the rule but exempt these, or
     * add parser support for our custom pseudoclasses.
     *
     * For now, we just disable this rule.
     */
    "selector-type-no-unknown": null,
    /*
     * See above - if we enabled this rule, we'd have to allow for a number
     * of custom elements we use, which are listed here:
    "selector-type-no-unknown": [
      true,
      {
        ignore: ["custom-elements"],
        ignoreTypes: [
          // Modern custom element / storybooked components:
          /^moz-/,
          // moz-locale-dir trips this rule for some reason:
          "rtl",
          "ltr",
          // Migrated XBL elements not part of core XUL that we use at the moment:
          "findbar",
          "panelmultiview",
          "panelview",
          "popupnotification",
          "popupnotificationcontent",
          // Legacy XUL elements:
          // (the commented out ones used to be a thing and aren't used in-tree anymore)
          "arrowscrollbox",
          "box",
          // "broadcaster",
          // "broadcasterset",
          "button",
          "browser",
          "checkbox",
          "caption",
          // clicktoscroll
          // colorpicker
          // column
          // columns
          "commandset",
          "command",
          // conditions
          // content
          // datepicker
          "deck",
          "description",
          "dialog",
          // dialogheader
          "dropmarker",
          "editor",
          // grid
          // grippy
          "groupbox",
          "hbox",
          // iframe
          // image
          "key",
          "keyset",
          // label
          "listbox",
          // listcell
          // listcol
          // listcols
          // listhead
          // listheader
          "listitem",
          // member
          "menu",
          "menubar",
          "menucaption",
          "menuitem",
          "menulist",
          "menupopup",
          "menuseparator",
          "notification",
          "notificationbox",
          "observes",
          // overlay
          // page
          "panel",
          // param
          "popupset",
          // preference
          // preferences
          // prefpane
          // prefwindow
          // progressmeter
          // query
          // queryset
          "radio",
          "radiogroup",
          // resizer
          "richlistbox",
          "richlistitem",
          // row
          // rows
          // rule
          // scale
          // script
          "scrollbar",
          "scrollbox",
          "scrollcorner",
          "separator",
          "spacer",
          // spinbuttons
          "splitter",
          "stack",
          // statusbar
          // statusbarpanel
          "stringbundle",
          "stringbundleset",
          "tab",
          "tabbox",
          "tabpanel",
          "tabpanels",
          "tabs",
          // template
          // textnode
          "textbox",
          // timepicker
          "titlebar",
          "toolbar",
          "toolbarbutton",
          // toolbargrippy
          "toolbaritem",
          "toolbarpalette",
          "toolbarpaletteitem",
          "toolbarseparator",
          "toolbarset",
          "toolbarspacer",
          "toolbarspring",
          "toolbartabstop",
          "toolbox",
          "tooltip",
          "tree",
          "treecell",
          "treechildren",
          "treecol",
          "treecols",
          "treeitem",
          "treerow",
          "treeseparator",
          // triple
          "vbox",
          // where
          "window",
          "wizard",
          "wizardpage",
        ],
      },
    ],
    */

    "selector-pseudo-class-no-unknown": [
      true,
      {
        ignorePseudoClasses: ["popover-open"],
      },
    ],
    "selector-pseudo-element-no-unknown": [
      true,
      {
        ignorePseudoElements: ["slider-track", "slider-fill", "slider-thumb"],
      },
    ],
  },

  overrides: [
    {
      files: "*.scss",
      customSyntax: "postcss-scss",
      extends: "stylelint-config-recommended-scss",
    },
    {
      files: "browser/components/newtab/**",
      customSyntax: "postcss-scss",
      extends: "stylelint-config-standard-scss",
      rules: {
        "at-rule-disallowed-list": [
          ["debug", "warn", "error"],
          {
            message: "Clean up %s directives before committing",
          },
        ],
        "at-rule-no-vendor-prefix": null,
        "color-function-notation": null,
        "color-hex-case": "upper",
        "comment-empty-line-before": [
          "always",
          {
            except: ["first-nested"],
            ignore: ["after-comment", "stylelint-commands"],
          },
        ],
        "custom-property-empty-line-before": null,
        "custom-property-pattern": null,
        "declaration-block-no-duplicate-properties": true,
        "declaration-block-no-redundant-longhand-properties": null,
        "declaration-no-important": true,
        "function-no-unknown": [
          true,
          {
            ignoreFunctions: ["div"],
          },
        ],
        "function-url-no-scheme-relative": true,
        indentation: 2,
        "keyframes-name-pattern": null,
        "media-feature-name-no-vendor-prefix": null,
        "no-descending-specificity": null,
        "no-eol-whitespace": true,
        "no-missing-end-of-source-newline": true,
        "number-leading-zero": "always",
        "number-no-trailing-zeros": true,
        "property-disallowed-list": [
          ["margin-left", "margin-right"],
          {
            message: "Use margin-inline instead of %s",
          },
        ],
        "property-no-unknown": true,
        "property-no-vendor-prefix": null,
        "scss/dollar-variable-empty-line-before": null,
        "scss/double-slash-comment-empty-line-before": [
          "always",
          {
            except: ["first-nested"],
            ignore: ["between-comments", "stylelint-commands", "inside-block"],
          },
        ],
        "selector-class-pattern": null,
        "selector-no-vendor-prefix": null,
        "string-quotes": [
          "single",
          {
            avoidEscape: true,
          },
        ],
        "value-keyword-case": null,
        "value-no-vendor-prefix": null,
      },
    },
  ],
};
