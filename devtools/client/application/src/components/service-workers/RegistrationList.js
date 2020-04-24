/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { openTrustedLink } = require("devtools/client/shared/link");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const {
  a,
  article,
  footer,
  h1,
  ul,
} = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Types = require("devtools/client/application/src/types/index");
const Registration = createFactory(
  require("devtools/client/application/src/components/service-workers/Registration")
);

/**
 * This component handles the list of service workers displayed in the application panel
 * and also displays a suggestion to use about debugging for debugging other service
 * workers.
 */
class RegistrationList extends PureComponent {
  static get propTypes() {
    return {
      canDebugWorkers: PropTypes.bool.isRequired,
      registrations: Types.registrationArray.isRequired,
    };
  }

  render() {
    const { canDebugWorkers, registrations } = this.props;

    return [
      article(
        {
          className: "registrations-container",
          key: "registrations-container",
        },
        Localized(
          { id: "serviceworker-list-header" },
          h1({
            className: "app-page__title",
          })
        ),
        ul(
          {},
          registrations.map(registration =>
            Registration({
              key: registration.id,
              isDebugEnabled: canDebugWorkers,
              registration,
              className: "registrations-container__item",
            })
          )
        )
      ),
      Localized(
        {
          id: "serviceworker-list-aboutdebugging",
          key: "serviceworkerlist-footer",
          a: a({
            className: "aboutdebugging-plug__link",
            onClick: () => openTrustedLink("about:debugging#workers"),
          }),
        },
        footer({ className: "aboutdebugging-plug" })
      ),
    ];
  }
}

// Exports
module.exports = RegistrationList;
