/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  PureComponent,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const MODE_PREF = "devtools.browsertoolbox.scope";

const MODE_VALUES = {
  PARENT_PROCESS: "parent-process",
  EVERYTHING: "everything",
};

const MODE_DATA = {
  [MODE_VALUES.PARENT_PROCESS]: {
    containerL10nId: "toolbox-mode-parent-process-container",
    labelL10nId: "toolbox-mode-parent-process-label",
    subLabelL10nId: "toolbox-mode-parent-process-sub-label",
  },
  [MODE_VALUES.EVERYTHING]: {
    containerL10nId: "toolbox-mode-everything-container",
    labelL10nId: "toolbox-mode-everything-label",
    subLabelL10nId: "toolbox-mode-everything-sub-label",
  },
};

/**
 * Toolbar displayed on top of the regular toolbar in the Browser Toolbox and Browser Console,
 * displaying chrome-debugging-specific options.
 */
class ChromeDebugToolbar extends PureComponent {
  static get propTypes() {
    return {
      isBrowserConsole: PropTypes.bool,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      mode: Services.prefs.getCharPref(MODE_PREF),
    };

    this.onModePrefChanged = this.onModePrefChanged.bind(this);
    Services.prefs.addObserver(MODE_PREF, this.onModePrefChanged);
  }

  componentWillUnmount() {
    Services.prefs.removeObserver(MODE_PREF, this.onModePrefChanged);
  }

  onModePrefChanged() {
    this.setState({
      mode: Services.prefs.getCharPref(MODE_PREF),
    });
  }

  renderModeItem(value) {
    const { containerL10nId, labelL10nId, subLabelL10nId } = MODE_DATA[value];

    const checked = this.state.mode == value;
    return Localized(
      {
        id: containerL10nId,
        attrs: { title: true },
      },
      dom.label(
        {
          className: checked ? "selected" : null,
        },
        dom.input({
          type: `radio`,
          name: `chrome-debug-mode`,
          value,
          checked: checked || null,
          onChange: () => {
            Services.prefs.setCharPref(MODE_PREF, value);
          },
        }),
        Localized({ id: labelL10nId }, dom.span({ className: "mode__label" })),
        Localized(
          { id: subLabelL10nId },
          dom.span({ className: "mode__sublabel" })
        )
      )
    );
  }

  render() {
    return dom.header(
      {
        className: "chrome-debug-toolbar",
      },
      dom.section(
        {
          className: "chrome-debug-toolbar__modes",
        },
        Localized(
          {
            id: this.props.isBrowserConsole
              ? "toolbox-mode-browser-console-label"
              : "toolbox-mode-browser-toolbox-label",
          },
          dom.h3({})
        ),
        this.renderModeItem(MODE_VALUES.PARENT_PROCESS),
        this.renderModeItem(MODE_VALUES.EVERYTHING)
      )
    );
  }
}

module.exports = ChromeDebugToolbar;
