/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { SubmitFormSnippet } from "../SubmitFormSnippet/SubmitFormSnippet.jsx";

export const FXASignupSnippet = props => {
  const userAgent = window.navigator.userAgent.match(/Firefox\/([0-9]+)\./);
  const firefox_version = userAgent ? parseInt(userAgent[1], 10) : 0;
  const extendedContent = {
    scene1_button_label: "Learn more",
    retry_button_label: "Try again",
    scene2_email_placeholder_text: "Your email here",
    scene2_button_label: "Sign me up",
    scene2_dismiss_button_text: "Dismiss",
    ...props.content,
    hidden_inputs: {
      action: "email",
      context: "fx_desktop_v3",
      entrypoint: "snippets",
      utm_source: "snippet",
      utm_content: firefox_version,
      utm_campaign: props.content.utm_campaign,
      utm_term: props.content.utm_term,
      ...props.content.hidden_inputs,
    },
  };

  return (
    <SubmitFormSnippet
      {...props}
      content={extendedContent}
      form_action={"https://accounts.firefox.com/"}
      form_method="GET"
    />
  );
};
