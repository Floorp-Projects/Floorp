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

  return (
    <Localized text={legal_paragraph.text}>
      {/* eslint-disable jsx-a11y/no-static-element-interactions */}
      <span
        className="legal-paragraph"
        onClick={handleParagraphAction}
        value="legal_paragraph"
        onKeyPress={onKeyPress}
      >
        {/* eslint-disable jsx-a11y/anchor-is-valid */}
        {legal_paragraph.link_keys?.map(link => (
          <a
            key={link}
            value={link}
            data-l10n-name={link}
            role="button"
            tabIndex="0" // must pass in tabIndex when no href is provided
          >
            {" "}
          </a>
        ))}
      </span>
    </Localized>
  );
};
