/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useCallback } from "react";
import { Localized } from "./MSLocalized";

export const LinkParagraph = props => {
  const { text_content, handleAction } = props;

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

  return (
    <Localized text={text_content.text}>
      {/* eslint-disable jsx-a11y/no-noninteractive-element-interactions */}
      <p
        className={
          text_content.font_styles === "legal"
            ? "legal-paragraph"
            : "link-paragraph"
        }
        onClick={handleParagraphAction}
        value="link_paragraph"
        onKeyPress={onKeyPress}
      >
        {/* eslint-disable jsx-a11y/anchor-is-valid */}
        {text_content.link_keys?.map(link => (
          <a
            key={link}
            value={link}
            role="link"
            className="text-link"
            data-l10n-name={link}
            // must pass in tabIndex when no href is provided
            tabIndex="0"
          >
            {" "}
          </a>
        ))}
      </p>
    </Localized>
  );
};
