/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const InspectAction = createFactory(require("./InspectAction"));

const Actions = require("../../actions/index");

/**
 * This component provides components that inspect/reload/remove temporary extension.
 */
class TemporaryExtensionAction extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      target: PropTypes.object.isRequired,
    };
  }

  reload() {
    const { dispatch, target } = this.props;
    dispatch(Actions.reloadTemporaryExtension(target.details.actor));
  }

  remove() {
    const { dispatch, target } = this.props;
    dispatch(Actions.removeTemporaryExtension(target.id));
  }

  render() {
    const { dispatch, target } = this.props;

    return dom.div(
      {},
      InspectAction({ dispatch, target }),
      Localized(
        {
          id: "about-debugging-tmp-extension-reload-button",
        },
        dom.button(
          {
            className: "aboutdebugging-button",
            onClick: e => this.reload(),
          },
          "Reload",
        )
      ),
      Localized(
        {
          id: "about-debugging-tmp-extension-remove-button",
        },
        dom.button(
          {
            className: "aboutdebugging-button js-temporary-extension-remove-button",
            onClick: e => this.remove(),
          },
          "Remove",
        )
      ),
    );
  }
}

module.exports = TemporaryExtensionAction;
