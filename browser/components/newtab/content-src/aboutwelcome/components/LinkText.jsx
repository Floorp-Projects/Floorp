/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { Localized } from "./MSLocalized";

export const LinkText = props => {
  const { content, handleAction } = props;

  if (
    !(
      content?.text &&
      content?.button_label &&
      typeof handleAction === "function"
    )
  ) {
    return null;
  }

  return (
    <h2 className="cta-paragraph">
      <Localized text={content.text}>
        <span />
      </Localized>
      <Localized text={content.button_label}>
        <button
          onClick={handleAction}
          value="cta_paragraph"
          className="text-link"
        />
      </Localized>
    </h2>
  );
};
