/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React & Redux
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  div,
  p,
  img,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const LearnMoreLink = createFactory(
  require("resource://devtools/client/accessibility/components/LearnMoreLink.js")
);

// Localization
const {
  L10N,
} = require("resource://devtools/client/accessibility/utils/l10n.js");

const {
  A11Y_LEARN_MORE_LINK,
} = require("resource://devtools/client/accessibility/constants.js");

/**
 * Landing UI for the accessibility panel when Accessibility features are
 * deactivated.
 */
function Description() {
  return div(
    { className: "description", role: "presentation", tabIndex: "-1" },
    div(
      { className: "general", role: "presentation", tabIndex: "-1" },
      img({
        src: "chrome://devtools/skin/images/accessibility.svg",
        alt: L10N.getStr("accessibility.logo"),
      }),
      div(
        { role: "presentation", tabIndex: "-1" },
        LearnMoreLink({
          href: A11Y_LEARN_MORE_LINK,
          learnMoreStringKey: "accessibility.learnMore",
          l10n: L10N,
          messageStringKey: "accessibility.description.general.p1",
        }),
        p({}, L10N.getStr("accessibility.enable.disabledTitle"))
      )
    )
  );
}

// Exports from this module
exports.Description = Description;
