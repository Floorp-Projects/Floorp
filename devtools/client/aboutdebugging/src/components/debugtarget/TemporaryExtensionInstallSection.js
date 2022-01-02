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

const DetailsLog = createFactory(
  require("devtools/client/aboutdebugging/src/components/shared/DetailsLog")
);
const Message = createFactory(
  require("devtools/client/aboutdebugging/src/components/shared/Message")
);
const TemporaryExtensionInstaller = createFactory(
  require("devtools/client/aboutdebugging/src/components/debugtarget/TemporaryExtensionInstaller")
);

const {
  MESSAGE_LEVEL,
} = require("devtools/client/aboutdebugging/src/constants");

/**
 * This component provides an installer and error message area for temporary extension.
 */
class TemporaryExtensionInstallSection extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      temporaryInstallError: PropTypes.object,
    };
  }

  renderError() {
    const { temporaryInstallError } = this.props;

    if (!temporaryInstallError) {
      return null;
    }

    const errorMessages = [
      temporaryInstallError.message,
      ...(temporaryInstallError.additionalErrors || []),
    ];

    const errors = errorMessages.map((message, index) => {
      return dom.p(
        {
          className: "technical-text",
          key: "tmp-extension-install-error-" + index,
        },
        message
      );
    });

    return Message(
      {
        level: MESSAGE_LEVEL.ERROR,
        className: "qa-tmp-extension-install-error",
        isCloseable: true,
      },
      Localized(
        {
          id: "about-debugging-tmp-extension-install-error",
        },
        dom.p({}, "about-debugging-tmp-extension-install-error")
      ),
      DetailsLog(
        {
          type: MESSAGE_LEVEL.ERROR,
        },
        errors
      )
    );
  }

  render() {
    const { dispatch } = this.props;

    return dom.section(
      {},
      dom.div(
        {
          className: "temporary-extension-install-section__toolbar",
        },
        TemporaryExtensionInstaller({ dispatch })
      ),
      this.renderError()
    );
  }
}

module.exports = TemporaryExtensionInstallSection;
