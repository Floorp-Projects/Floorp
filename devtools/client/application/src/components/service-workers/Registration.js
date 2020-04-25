/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const {
  article,
  aside,
  header,
  li,
  p,
  span,
  time,
  ul,
} = require("devtools/client/shared/vendor/react-dom-factories");

const { getUnicodeUrl } = require("devtools/client/shared/unicode-url");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const Types = require("devtools/client/application/src/types/index");

const UIButton = createFactory(
  require("devtools/client/application/src/components/ui/UIButton")
);

const Worker = createFactory(
  require("devtools/client/application/src/components/service-workers/Worker")
);

/**
 * This component is dedicated to display a service worker registration, along
 * the list of attached workers to it.
 * It displays information about the registration as well as an Unregister
 * button.
 */
class Registration extends PureComponent {
  static get propTypes() {
    return {
      className: PropTypes.string,
      isDebugEnabled: PropTypes.bool.isRequired,
      registration: PropTypes.shape(Types.registration).isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.unregister = this.unregister.bind(this);
  }

  unregister() {
    const { registrationFront } = this.props.registration;
    registrationFront.unregister();
  }

  isActive() {
    const { workers } = this.props.registration;
    return workers.some(x => x.isActive);
  }

  formatScope(scope) {
    const [, remainder] = getUnicodeUrl(scope).split("://");
    return remainder || scope;
  }

  render() {
    const { registration, isDebugEnabled, className } = this.props;

    const unregisterButton = this.isActive()
      ? Localized(
          { id: "serviceworker-worker-unregister" },
          UIButton({
            onClick: this.unregister,
            className: "js-unregister-button",
          })
        )
      : null;

    const lastUpdated = registration.lastUpdateTime
      ? Localized(
          {
            id: "serviceworker-worker-updated",
            // XXX: $date should normally be a Date object, but we pass the timestamp as a
            // workaround. See Bug 1465718. registration.lastUpdateTime is in microseconds,
            // convert to a valid timestamp in milliseconds by dividing by 1000.
            $date: registration.lastUpdateTime / 1000,
            time: time({ className: "js-sw-updated" }),
          },
          span({ className: "registration__updated-time" })
        )
      : null;

    const scope = span(
      {
        title: registration.scope,
        className: "registration__scope js-sw-scope",
      },
      this.formatScope(registration.scope)
    );

    return li(
      { className: className ? className : "" },
      article(
        { className: "registration js-sw-container" },
        header(
          { className: "registration__header" },
          scope,
          aside({}, unregisterButton)
        ),
        lastUpdated ? p({}, lastUpdated) : null,
        // render list of workers
        ul(
          { className: "registration__workers" },
          registration.workers.map(worker => {
            return li(
              {
                key: worker.id,
                className: "registration__workers-item",
              },
              Worker({
                worker,
                isDebugEnabled,
              })
            );
          })
        )
      )
    );
  }
}

module.exports = Registration;
