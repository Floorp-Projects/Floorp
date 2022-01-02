/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import { SimpleSnippet } from "../SimpleSnippet/SimpleSnippet";

class EOYSnippetBase extends React.PureComponent {
  constructor(props) {
    super(props);
    this.handleSubmit = this.handleSubmit.bind(this);
  }

  /**
   * setFrequencyValue - `frequency` form parameter value should be `monthly`
   *                     if `monthly-checkbox` is selected or `single` otherwise
   */
  setFrequencyValue() {
    const frequencyCheckbox = this.refs.form.querySelector("#monthly-checkbox");
    if (frequencyCheckbox.checked) {
      this.refs.form.querySelector("[name='frequency']").value = "monthly";
    }
  }

  handleSubmit(event) {
    event.preventDefault();
    this.props.sendClick(event);
    this.setFrequencyValue();
    if (!this.props.content.do_not_autoblock) {
      this.props.onBlock();
    }
    this.refs.form.submit();
  }

  renderDonations() {
    const fieldNames = ["first", "second", "third", "fourth"];
    const numberFormat = new Intl.NumberFormat(
      this.props.content.locale || navigator.language,
      {
        style: "currency",
        currency: this.props.content.currency_code,
        minimumFractionDigits: 0,
      }
    );
    // Default to `second` button
    const { selected_button } = this.props.content;
    const btnStyle = {
      color: this.props.content.button_color,
      backgroundColor: this.props.content.button_background_color,
    };
    const donationURLParams = [];
    const paramsStartIndex = this.props.content.donation_form_url.indexOf("?");
    for (const entry of new URLSearchParams(
      this.props.content.donation_form_url.slice(paramsStartIndex)
    ).entries()) {
      donationURLParams.push(entry);
    }

    return (
      <form
        className="EOYSnippetForm"
        action={this.props.content.donation_form_url}
        method={this.props.form_method}
        onSubmit={this.handleSubmit}
        data-metric="EOYSnippetForm"
        ref="form"
      >
        {donationURLParams.map(([key, value], idx) => (
          <input type="hidden" name={key} value={value} key={idx} />
        ))}
        {fieldNames.map((field, idx) => {
          const button_name = `donation_amount_${field}`;
          const amount = this.props.content[button_name];
          return (
            <React.Fragment key={idx}>
              <input
                type="radio"
                name="amount"
                value={amount}
                id={field}
                defaultChecked={button_name === selected_button}
              />
              <label htmlFor={field} className="donation-amount">
                {numberFormat.format(amount)}
              </label>
            </React.Fragment>
          );
        })}

        <div className="monthly-checkbox-container">
          <input id="monthly-checkbox" type="checkbox" />
          <label htmlFor="monthly-checkbox">
            {this.props.content.monthly_checkbox_label_text}
          </label>
        </div>

        <input type="hidden" name="frequency" value="single" />
        <input
          type="hidden"
          name="currency"
          value={this.props.content.currency_code}
        />
        <input
          type="hidden"
          name="presets"
          value={fieldNames.map(
            field => this.props.content[`donation_amount_${field}`]
          )}
        />
        <button
          style={btnStyle}
          type="submit"
          className="ASRouterButton primary donation-form-url"
        >
          {this.props.content.button_label}
        </button>
      </form>
    );
  }

  render() {
    const textStyle = {
      color: this.props.content.text_color,
      backgroundColor: this.props.content.background_color,
    };
    const customElement = (
      <em style={{ backgroundColor: this.props.content.highlight_color }} />
    );
    return (
      <SimpleSnippet
        {...this.props}
        className={this.props.content.test}
        customElements={{ em: customElement }}
        textStyle={textStyle}
        extraContent={this.renderDonations()}
      />
    );
  }
}

export const EOYSnippet = props => {
  const extendedContent = {
    monthly_checkbox_label_text: "Make my donation monthly",
    locale: "en-US",
    currency_code: "usd",
    selected_button: "donation_amount_second",
    ...props.content,
  };

  return (
    <EOYSnippetBase {...props} content={extendedContent} form_method="GET" />
  );
};
