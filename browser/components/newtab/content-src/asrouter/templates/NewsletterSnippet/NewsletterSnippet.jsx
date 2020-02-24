/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { SubmitFormSnippet } from "../SubmitFormSnippet/SubmitFormSnippet.jsx";

export const NewsletterSnippet = props => {
  const extendedContent = {
    scene1_button_label: "Learn more",
    retry_button_label: "Try again",
    scene2_email_placeholder_text: "Your email here",
    scene2_button_label: "Sign me up",
    scene2_dismiss_button_text: "Dismiss",
    scene2_newsletter: "mozilla-foundation",
    ...props.content,
    hidden_inputs: {
      newsletters: props.content.scene2_newsletter || "mozilla-foundation",
      fmt: "H",
      lang: props.content.locale || "en-US",
      source_url: `https://snippets.mozilla.com/show/${props.id}`,
      ...props.content.hidden_inputs,
    },
  };

  return (
    <SubmitFormSnippet
      {...props}
      content={extendedContent}
      form_action={"https://basket.mozilla.org/subscribe.json"}
      form_method="POST"
    />
  );
};
