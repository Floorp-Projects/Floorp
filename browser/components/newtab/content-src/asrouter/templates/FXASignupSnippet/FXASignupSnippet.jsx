import React from "react";
import {SubmitFormSnippet} from "../SubmitFormSnippet/SubmitFormSnippet.jsx";

export const FXASignupSnippet = props => {
  const userAgent = window.navigator.userAgent.match(/Firefox\/([0-9]+)\./);
  const firefox_version = userAgent ? parseInt(userAgent[1], 10) : 0;
  const extendedContent = {
    form_action: "https://accounts.firefox.com/",
    scene1_button_label: "Learn More",
    scene2_button_label: "Sign Me Up",
    scene2_email_placeholder_text: "Your Email Here",
    ...props.content,
    hidden_inputs: {
      action: "email",
      context: "fx_desktop_v3",
      entrypoint: "snippets",
      service: "sync",
      utm_source: "snippet",
      utm_content: firefox_version,
      utm_campaign: props.content.utm_campaign,
      utm_term: props.content.utm_term,
      ...props.content.hidden_inputs,
    },
  };

  return (<SubmitFormSnippet
    {...props}
    content={extendedContent}
    form_method="GET" />);
};
