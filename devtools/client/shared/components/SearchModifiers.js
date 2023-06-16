/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  div,
  span,
  button,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

loader.lazyGetter(this, "l10n", function () {
  const { LocalizationHelper } = require("resource://devtools/shared/l10n.js");
  return new LocalizationHelper(
    "devtools/client/locales/components.properties"
  );
});

const modifierOptions = [
  {
    value: "regexMatch",
    className: "regex-match-btn",
    svgName: "regex-match",
    tooltip: l10n.getStr("searchModifier.regExpModifier"),
  },
  {
    value: "caseSensitive",
    className: "case-sensitive-btn",
    svgName: "case-match",
    tooltip: l10n.getStr("searchModifier.caseSensitiveModifier"),
  },
  {
    value: "wholeWord",
    className: "whole-word-btn",
    svgName: "whole-word-match",
    tooltip: l10n.getStr("searchModifier.wholeWordModifier"),
  },
];

class SearchModifiers extends Component {
  static get propTypes() {
    return {
      modifiers: PropTypes.object.isRequired,
      onToggleSearchModifier: PropTypes.func.isRequired,
    };
  }

  #renderSearchModifier({ value, className, svgName, tooltip }) {
    const { modifiers, onToggleSearchModifier } = this.props;

    return button(
      {
        className: `${className} ${modifiers?.[value] ? "active" : ""}`,
        onMouseDown: () => {
          modifiers[value] = !modifiers[value];
          onToggleSearchModifier(modifiers);
        },
        onKeyDown: e => {
          if (e.key === "Enter") {
            modifiers[value] = !modifiers[value];
            onToggleSearchModifier(modifiers);
          }
        },
        title: tooltip,
      },
      span({ className: svgName })
    );
  }

  render() {
    return div(
      { className: "search-modifiers" },
      span({ className: "pipe-divider" }),
      modifierOptions.map(options => this.#renderSearchModifier(options))
    );
  }
}

module.exports = SearchModifiers;
