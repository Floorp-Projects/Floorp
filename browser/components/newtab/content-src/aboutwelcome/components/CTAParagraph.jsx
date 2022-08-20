/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { Localized } from "./MSLocalized";

export const CTAParagraph = props => {
  const { content, handleAction } = props;

  if (!content?.text) {
    return null;
  }

  return (
    <h2 className="cta-paragraph">
      <Localized text={content.text}>
        {content.text.string_name && typeof handleAction === "function" ? (
          <span
            data-l10n-id={content.text.string_id}
            onClick={handleAction}
            onKeyUp={event =>
              ["Enter", " "].includes(event.key) ? handleAction(event) : null
            }
            value="cta_paragraph"
            role="button"
            tabIndex="0"
          >
            {" "}
            {/* <a> is valid here because of click and keyup handling. */}
            {/* <button> cannot be used due to fluent integration. <a> content is provided by fluent */}
            {/* eslint-disable jsx-a11y/anchor-is-valid */}
            <a
              role="button"
              tabIndex="0"
              data-l10n-name={content.text.string_name}
            >
              {" "}
            </a>
          </span>
        ) : null}
      </Localized>
    </h2>
  );
};
