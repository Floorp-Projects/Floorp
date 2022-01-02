/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { Localized } from "./MSLocalized";

export const Themes = props => {
  return (
    <div className="tiles-theme-container">
      <div>
        <fieldset className="tiles-theme-section">
          <Localized text={props.content.subtitle}>
            <legend className="sr-only" />
          </Localized>
          {props.content.tiles.data.map(
            ({ theme, label, tooltip, description }) => (
              <Localized
                key={theme + label}
                text={typeof tooltip === "object" ? tooltip : {}}
              >
                <label className="theme" title={theme + label}>
                  <Localized
                    text={typeof description === "object" ? description : {}}
                  >
                    <input
                      type="radio"
                      value={theme}
                      name="theme"
                      checked={theme === props.activeTheme}
                      className="sr-only input"
                      onClick={props.handleAction}
                      data-l10n-attrs="aria-description"
                    />
                  </Localized>
                  <div
                    className={`icon ${
                      theme === props.activeTheme ? " selected" : ""
                    } ${theme}`}
                  />
                  {label && (
                    <Localized text={label}>
                      <div className="text" />
                    </Localized>
                  )}
                </label>
              </Localized>
            )
          )}
        </fieldset>
      </div>
    </div>
  );
};
