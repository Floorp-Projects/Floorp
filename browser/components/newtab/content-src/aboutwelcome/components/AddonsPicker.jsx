/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { AboutWelcomeUtils } from "../../lib/aboutwelcome-utils";
import { Localized } from "./MSLocalized";

export const AddonsPicker = props => {
  const { content } = props;

  if (!content) {
    return null;
  }

  function handleAction(event) {
    const { message_id } = props;
    let { action, source_id } = content.tiles.data[event.currentTarget.value];
    let { type, data } = action;

    if (type === "INSTALL_ADDON_FROM_URL") {
      if (!data) {
        return;
      }
    }

    AboutWelcomeUtils.handleUserAction({ type, data });
    AboutWelcomeUtils.sendActionTelemetry(message_id, source_id);
  }

  return (
    <div className={"addons-picker-container"}>
      {content.tiles.data.map(
        ({ id, install_label, name, type, description, icon }, index) =>
          name ? (
            <div key={id} className="addon-container">
              <div className="rtamo-icon">
                <img
                  className={`${
                    type === "theme" ? "rtamo-theme-icon" : "brand-logo"
                  }`}
                  src={icon}
                  role="presentation"
                  alt=""
                />
              </div>
              <div className="addon-details">
                <Localized text={name}>
                  <div className="addon-title" />
                </Localized>
                <Localized text={description}>
                  <div className="addon-description" />
                </Localized>
              </div>
              <Localized text={install_label}>
                <button
                  id={name}
                  value={index}
                  onClick={handleAction}
                  className="primary"
                />
              </Localized>
            </div>
          ) : null
      )}
    </div>
  );
};
