/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * !!!!                      TO BE REMOVED AFTER RELEASE 56                          !!!!
 * !!!!                                                                              !!!!
 * !!!! This file is a temporary panel that should only be used for release 56 to    !!!!
 * !!!! promote the new layout panel. After release 56, it should be removed.        !!!!
 * !!!! See bug 1355747.                                                             !!!!
 */

const Services = require("Services");
const { addons, createClass, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");

const { LocalizationHelper } = require("devtools/shared/l10n");

const LAYOUT_STRINGS_URI = "devtools/client/locales/layout.properties";
const LAYOUT_L10N = new LocalizationHelper(LAYOUT_STRINGS_URI);

const SHOW_PROMOTE_BAR_PREF = "devtools.promote.layoutview.showPromoteBar";

module.exports = createClass({

  displayName: "LayoutPromoteBar",

  propTypes: {
    onPromoteLearnMoreClick: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  getInitialState() {
    return {
      showPromoteBar: Services.prefs.getBoolPref(SHOW_PROMOTE_BAR_PREF)
    };
  },

  onPromoteCloseButtonClick() {
    Services.prefs.setBoolPref(SHOW_PROMOTE_BAR_PREF, false);
    this.setState({ showPromoteBar: false });
  },

  render() {
    let { onPromoteLearnMoreClick } = this.props;
    let { showPromoteBar } = this.state;

    return showPromoteBar ?
      dom.div({ className: "layout-promote-bar" },
        dom.span({ className: "layout-promote-info-icon" }),
        dom.div({ className: "layout-promote-message" },
          LAYOUT_L10N.getStr("layout.promoteMessage"),
          dom.a(
            {
              className: "layout-promote-learn-more-link theme-link",
              href: "#",
              onClick: onPromoteLearnMoreClick,
            },
            LAYOUT_L10N.getStr("layout.learnMore")
          )
        ),
        dom.button(
          {
            className: "layout-promote-close-button devtools-button",
            onClick: this.onPromoteCloseButtonClick,
          }
        )
      )
      :
      null;
  },

});
