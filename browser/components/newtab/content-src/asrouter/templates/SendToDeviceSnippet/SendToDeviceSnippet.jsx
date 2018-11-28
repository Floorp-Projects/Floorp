import {isEmailOrPhoneNumber} from "./isEmailOrPhoneNumber";
import React from "react";
import schema from "./SendToDeviceSnippet.schema.json";
import {SubmitFormSnippet} from "../SubmitFormSnippet/SubmitFormSnippet.jsx";

function validateInput(value, content) {
  const type = isEmailOrPhoneNumber(value, content);
  return type ? "" : "Must be an email or a phone number.";
}

function processFormData(input, message) {
  const {content} = message;
  const type = content.include_sms ? isEmailOrPhoneNumber(input.value, content) : "email";
  const formData = new FormData();
  let url;
  if (type === "phone") {
    url = "https://basket.mozilla.org/news/subscribe_sms/";
    formData.append("mobile_number", input.value);
    formData.append("msg_name", content.message_id_sms);
    formData.append("country", content.country);
  } else if (type === "email") {
    url = "https://basket.mozilla.org/news/subscribe/";
    formData.append("email", input.value);
    formData.append("newsletters", content.message_id_email);
    formData.append("source_url", encodeURIComponent(`https://snippets.mozilla.com/show/${message.id}`));
  }
  formData.append("lang", content.locale);
  return {formData, url};
}

function addDefaultValues(props) {
  return {
    ...props,
    content: {
      scene1_button_label: schema.properties.scene1_button_label.default,
      scene2_dismiss_button_text: schema.properties.scene2_dismiss_button_text.default,
      scene2_button_label: schema.properties.scene2_button_label.default,
      scene2_input_placeholder: schema.properties.scene2_input_placeholder.default,
      locale: schema.properties.locale.default,
      country: schema.properties.country.default,
      message_id_email: "",
      include_sms: schema.properties.include_sms.default,
      ...props.content,
    },
  };
}

export const SendToDeviceSnippet = props => {
  const propsWithDefaults = addDefaultValues(props);

  return (<SubmitFormSnippet {...propsWithDefaults}
    form_method="POST"
    className="send_to_device_snippet"
    inputType={propsWithDefaults.content.include_sms ? "text" : "email"}
    validateInput={propsWithDefaults.content.include_sms ? validateInput : null}
    processFormData={processFormData} />);
};
