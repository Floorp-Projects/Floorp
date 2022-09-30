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
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

const FluentReact = require("resource://devtools/client/shared/vendor/fluent-react.js");
const Localized = createFactory(FluentReact.Localized);

const {
  getCurrentRuntimeDetails,
} = require("resource://devtools/client/aboutdebugging/src/modules/runtimes-state-helper.js");

const DetailsLog = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/shared/DetailsLog.js")
);
const FieldPair = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/debugtarget/FieldPair.js")
);
const Message = createFactory(
  require("resource://devtools/client/aboutdebugging/src/components/shared/Message.js")
);

const {
  EXTENSION_BGSCRIPT_STATUSES,
  MESSAGE_LEVEL,
  RUNTIMES,
} = require("resource://devtools/client/aboutdebugging/src/constants.js");
const Types = require("resource://devtools/client/aboutdebugging/src/types/index.js");

/**
 * This component displays detail information for extension.
 */
class ExtensionDetail extends PureComponent {
  static get propTypes() {
    return {
      children: PropTypes.node,
      // Provided by wrapping the component with FluentReact.withLocalization.
      getString: PropTypes.func.isRequired,
      // Provided by redux state
      runtimeDetails: Types.runtimeDetails.isRequired,
      target: Types.debugTarget.isRequired,
    };
  }

  renderWarnings() {
    const { warnings } = this.props.target.details;

    if (!warnings.length) {
      return null;
    }

    return dom.section(
      {
        className: "debug-target-item__messages",
      },
      warnings.map((warning, index) => {
        return Message(
          {
            level: MESSAGE_LEVEL.WARNING,
            isCloseable: true,
            key: `warning-${index}`,
          },
          DetailsLog(
            {
              type: MESSAGE_LEVEL.WARNING,
            },
            dom.p(
              {
                className: "technical-text",
              },
              warning
            )
          )
        );
      })
    );
  }

  renderUUID() {
    const { uuid } = this.props.target.details;
    if (!uuid) {
      return null;
    }

    return Localized(
      {
        id: "about-debugging-extension-uuid",
        attrs: { label: true },
      },
      FieldPair({
        label: "Internal UUID",
        value: uuid,
      })
    );
  }

  renderExtensionId() {
    const { id } = this.props.target;

    return Localized(
      {
        id: "about-debugging-extension-id",
        attrs: { label: true },
      },
      FieldPair({
        label: "Extension ID",
        value: id,
      })
    );
  }

  renderLocation() {
    const { location } = this.props.target.details;
    if (!location) {
      return null;
    }

    return Localized(
      {
        id: "about-debugging-extension-location",
        attrs: { label: true },
      },
      FieldPair({
        label: "Location",
        value: location,
      })
    );
  }

  renderManifest() {
    // Manifest links are only relevant when debugging the current Firefox
    // instance.
    if (this.props.runtimeDetails.info.type !== RUNTIMES.THIS_FIREFOX) {
      return null;
    }

    const { manifestURL } = this.props.target.details;
    const link = dom.a(
      {
        className: "qa-manifest-url",
        href: manifestURL,
        target: "_blank",
      },
      manifestURL
    );

    return Localized(
      {
        id: "about-debugging-extension-manifest-url",
        attrs: { label: true },
      },
      FieldPair({
        label: "Manifest URL",
        value: link,
      })
    );
  }

  renderBackgroundScriptStatus() {
    // The status of the background script is only relevant if it is
    // not persistent.
    const { persistentBackgroundScript } = this.props.target.details;
    if (!(persistentBackgroundScript === false)) {
      return null;
    }

    const { backgroundScriptStatus } = this.props.target.details;

    let status;
    let statusLocalizationId;
    let statusClassName;

    if (backgroundScriptStatus === EXTENSION_BGSCRIPT_STATUSES.RUNNING) {
      status = `extension-backgroundscript__status--running`;
      statusLocalizationId = `about-debugging-extension-backgroundscript-status-running`;
      statusClassName = `extension-backgroundscript__status--running`;
    } else {
      status = `extension-backgroundscript__status--stopped`;
      statusLocalizationId = `about-debugging-extension-backgroundscript-status-stopped`;
      statusClassName = `extension-backgroundscript__status--stopped`;
    }

    return Localized(
      {
        id: "about-debugging-extension-backgroundscript",
        attrs: { label: true },
      },
      FieldPair({
        label: "Background Script",
        value: Localized(
          {
            id: statusLocalizationId,
          },
          dom.span(
            {
              className: `extension-backgroundscript__status qa-extension-backgroundscript-status ${statusClassName}`,
            },
            status
          )
        ),
      })
    );
  }

  render() {
    return dom.section(
      {
        className: "debug-target-item__detail",
      },
      this.renderWarnings(),
      dom.dl(
        {},
        this.renderLocation(),
        this.renderExtensionId(),
        this.renderUUID(),
        this.renderManifest(),
        this.renderBackgroundScriptStatus(),
        this.props.children
      )
    );
  }
}

const mapStateToProps = state => {
  return {
    runtimeDetails: getCurrentRuntimeDetails(state.runtimes),
  };
};

module.exports = FluentReact.withLocalization(
  connect(mapStateToProps)(ExtensionDetail)
);
