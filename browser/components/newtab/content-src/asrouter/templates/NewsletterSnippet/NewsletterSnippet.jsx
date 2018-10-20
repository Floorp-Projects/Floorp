import React from "react";
import {SubmitFormSnippet} from "../SubmitFormSnippet/SubmitFormSnippet.jsx";

export const NewsletterSnippet = props => {
  const extendedContent = {
    form_action: "https://basket.mozilla.org/subscribe.json",
    ...props.content,
    hidden_inputs: {
      newsletters: props.content.scene2_newsletter || "mozilla-foundation",
      fmt: "H",
      lang: "en-US",
      source_url: `https://snippets.mozilla.com/show/${props.id}`,
      ...props.content.hidden_inputs,
    },
  };

  return (<SubmitFormSnippet
    {...props}
    content={extendedContent}
    form_method="POST" />);
};
