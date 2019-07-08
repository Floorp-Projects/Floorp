/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Actions = require("../../actions/index");
const Types = require("../../types/index");

const DetailsLog = createFactory(require("../shared/DetailsLog"));
const Message = createFactory(require("../shared/Message"));
const { MESSAGE_LEVEL } = require("../../constants");

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

  render() {
    return [
      dom.div(
        {
          className: "toolbar toolbar--right-align",
          key: "actions",
        },
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
        Localized(
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
        )
      ),
      this.renderReloadError(),
    ];
  }
}

module.exports = TemporaryExtensionAdditionalActions;
