/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { addons, createClass, createFactory, DOM: dom, PropTypes } =
  require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const SearchBox = createFactory(require("devtools/client/shared/components/SearchBox"));
const FontList = createFactory(require("./FontList"));

const { getStr } = require("../utils/l10n");
const Types = require("../types");

const PREVIEW_UPDATE_DELAY = 150;

const App = createClass({

  displayName: "App",

  propTypes: {
    fonts: PropTypes.arrayOf(PropTypes.shape(Types.font)).isRequired,
    onPreviewFonts: PropTypes.func.isRequired,
    onShowAllFont: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  render() {
    let {
      fonts,
      onPreviewFonts,
      onShowAllFont,
    } = this.props;

    return dom.div(
      {
        className: "theme-sidebar inspector-tabpanel",
        id: "sidebar-panel-fontinspector"
      },
      dom.div(
        {
          className: "devtools-toolbar"
        },
        SearchBox({
          delay: PREVIEW_UPDATE_DELAY,
          placeholder: getStr("fontinspector.previewText"),
          type: "text",
          onChange: onPreviewFonts,
        }),
        dom.label(
          {
            id: "font-showall",
            className: "theme-link",
            title: getStr("fontinspector.seeAll.tooltip"),
            onClick: onShowAllFont,
          },
          getStr("fontinspector.seeAll")
        )
      ),
      fonts.length ?
        FontList({ fonts })
        :
        dom.div(
          {
            className: "devtools-sidepanel-no-result"
          },
          getStr("fontinspector.noFontsOnSelectedElement")
        )
    );
  }
});

module.exports = connect(state => state)(App);
