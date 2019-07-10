/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import schema from "./NewsletterSnippet.schema.json";
import { SubmitFormSnippet } from "../SubmitFormSnippet/SubmitFormSnippet.jsx";

export const NewsletterSnippet = props => {
  const extendedContent = {
    scene1_button_label: schema.properties.scene1_button_label.default,
    scene2_email_placeholder_text:
      schema.properties.scene2_email_placeholder_text.default,
    scene2_button_label: schema.properties.scene2_button_label.default,
    scene2_dismiss_button_text:
      schema.properties.scene2_dismiss_button_text.default,
    scene2_newsletter: schema.properties.scene2_newsletter.default,
    ...props.content,
    hidden_inputs: {
      newsletters:
        props.content.scene2_newsletter ||
        schema.properties.scene2_newsletter.default,
      fmt: schema.properties.hidden_inputs.properties.fmt.default,
      lang: props.content.locale || schema.properties.locale.default,
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
