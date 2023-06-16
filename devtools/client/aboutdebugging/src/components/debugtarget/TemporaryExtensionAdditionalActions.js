/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const Actions = require("resource://devtools/client/aboutdebugging/src/actions/index.js");
const Types = require("resource://devtools/client/aboutdebugging/src/types/index.js");

const DetailsLog = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/shared/DetailsLog.js")
);
const Message = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/shared/Message.js")
);
const {
  MESSAGE_LEVEL,
} = require("resource://devtools/client/aboutdebugging/src/constants.js");

/**
 * This component provides components that reload/remove temporary extension.
 */
class TemporaryExtensionAdditionalActions extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      target: Types.debugTarget.isRequired,
    };
  }

  reload() {
    const { dispatch, target } = this.props;
    dispatch(Actions.reloadTemporaryExtension(target.id));
  }

  terminateBackgroundScript() {
    const { dispatch, target } = this.props;
    dispatch(Actions.terminateExtensionBackgroundScript(target.id));
  }

  remove() {
    const { dispatch, target } = this.props;
    dispatch(Actions.removeTemporaryExtension(target.id));
  }

  renderReloadError() {
    const { reloadError } = this.props.target.details;

    if (!reloadError) {
      return null;
    }

    return Message(
      {
        className: "qa-temporary-extension-reload-error",
        level: MESSAGE_LEVEL.ERROR,
        key: "reload-error",
      },
      DetailsLog(
        {
          type: MESSAGE_LEVEL.ERROR,
        },
        dom.p(
          {
            className: "technical-text",
          },
          reloadError
        )
      )
    );
  }

  renderTerminateBackgroundScriptError() {
    const { lastTerminateBackgroundScriptError } = this.props.target.details;

    if (!lastTerminateBackgroundScriptError) {
      return null;
    }

    return Message(
      {
        className: "qa-temporary-extension-terminate-backgroundscript-error",
        level: MESSAGE_LEVEL.ERROR,
        key: "terminate-backgroundscript-error",
      },
      DetailsLog(
        {
          type: MESSAGE_LEVEL.ERROR,
        },
        dom.p(
          {
            className: "technical-text",
          },
          lastTerminateBackgroundScriptError
        )
      )
    );
  }

  renderTerminateBackgroundScriptButton() {
    const { persistentBackgroundScript } = this.props.target.details;

    // For extensions with a non persistent background script
    // also include a "terminate background script" action.
    if (persistentBackgroundScript !== false) {
      return null;
    }

    return Localized(
      {
        id: "about-debugging-tmp-extension-terminate-bgscript-button",
      },
      dom.button(
        {
          className:
            "default-button default-button--micro " +
            "qa-temporary-extension-terminate-bgscript-button",
          onClick: e => this.terminateBackgroundScript(),
        },
        "Terminate Background Script"
      )
    );
  }

  renderRemoveButton() {
    return Localized(
      {
        id: "about-debugging-tmp-extension-remove-button",
      },
      dom.button(
        {
          className:
            "default-button default-button--micro " +
            "qa-temporary-extension-remove-button",
          onClick: e => this.remove(),
        },
        "Remove"
      )
    );
  }

  render() {
    return [
      dom.div(
        {
          className: "toolbar toolbar--right-align",
          key: "actions",
        },
        this.renderTerminateBackgroundScriptButton(),
        Localized(
          {
            id: "about-debugging-tmp-extension-reload-button",
          },
          dom.button(
            {
              className:
                "default-button default-button--micro " +
                "qa-temporary-extension-reload-button",
              onClick: e => this.reload(),
            },
            "Reload"
          )
        ),
        this.renderRemoveButton()
      ),
      this.renderReloadError(),
      this.renderTerminateBackgroundScriptError(),
    ];
  }
}

module.exports = TemporaryExtensionAdditionalActions;
