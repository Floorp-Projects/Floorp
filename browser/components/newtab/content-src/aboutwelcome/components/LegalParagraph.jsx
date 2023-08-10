/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useCallback } from "react";
import { Localized } from "./MSLocalized";

export const LegalParagraph = props => {
  const { content, handleAction } = props;

  const handleParagraphAction = useCallback(
    event => {
      if (event.target.closest("a")) {
        handleAction({ ...event, currentTarget: event.target });
      }
    },
    [handleAction]
  );

  const onKeyPress = useCallback(
    event => {
      if (event.key === "Enter" && !event.repeat) {
        handleParagraphAction(event);
      }
    },
    [handleParagraphAction]
  );

  const { legal_paragraph } = content;

  if (!legal_paragraph.text) {
    return null;
  }

  return (
    /* eslint-disable jsx-a11y/no-static-element-interactions */
    <span
      className="legal-paragraph"
      onClick={handleParagraphAction}
      value="legal_paragraph"
      onKeyPress={onKeyPress}
    >
      <Localized text={legal_paragraph.text}>
        {legal_paragraph.text.string_id ? (
          <span>
            {/* <a> is valid here because of click and key press handling. */}
            {/* <button> cannot be used due to fluent integration. <a> content is provided by fluent */}
            {/* tabIndex must be included because href is not provided from fluent */}
            {/* eslint-disable jsx-a11y/anchor-is-valid */}
            {legal_paragraph.link_keys?.map(link => (
              <a
                key={content[link].string_label}
                value={link}
                data-l10n-name={link}
                role="button"
                tabIndex="0"
              >
                {" "}
              </a>
            ))}
          </span>
        ) : null}
      </Localized>
    </span>
  );
};
