import React from "react";
import schema from "./EOYSnippet.schema.json";
import {SimpleSnippet} from "../SimpleSnippet/SimpleSnippet";

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
    this.setFrequencyValue();
    this.refs.form.submit();
    if (!this.props.content.do_not_autoblock) {
      this.props.onBlock();
    }
  }

  renderDonations() {
    const fieldNames = ["first", "second", "third", "fourth"];
    const numberFormat = new Intl.NumberFormat(this.props.content.locale || navigator.language, {
      style: "currency",
      currency: this.props.content.currency_code,
      minimumFractionDigits: 0,
    });
    // Default to `second` button
    const {selected_button} = this.props.content;
    const btnStyle = {
      color: this.props.content.button_color,
      backgroundColor: this.props.content.button_background_color,
    };

    return (<form className="EOYSnippetForm" action={this.props.content.donation_form_url} method={this.props.form_method} onSubmit={this.handleSubmit} ref="form">
      {fieldNames.map((field, idx) => {
        const button_name = `donation_amount_${field}`;
        const amount = this.props.content[button_name];
        return (<React.Fragment key={idx}>
            <input type="radio" name="amount" value={amount} id={field} defaultChecked={button_name === selected_button} />
            <label htmlFor={field} className="donation-amount">
              {numberFormat.format(amount)}
            </label>
          </React.Fragment>);
      })}

      <div className="monthly-checkbox-container">
        <input id="monthly-checkbox" type="checkbox" />
        <label htmlFor="monthly-checkbox">
          {this.props.content.monthly_checkbox_label_text}
        </label>
      </div>

      <input type="hidden" name="frequency" value="single" />
      <input type="hidden" name="currency" value={this.props.content.currency_code} />
      <input type="hidden" name="presets" value={fieldNames.map(field => this.props.content[`donation_amount_${field}`])} />
      <button style={btnStyle} type="submit" className="ASRouterButton primary donation-form-url">{this.props.content.button_label}</button>
    </form>);
  }

  render() {
    const textStyle = {
      color: this.props.content.text_color,
      backgroundColor: this.props.content.background_color,
    };
    const customElement = <em style={{backgroundColor: this.props.content.highlight_color}} />;
    return (<SimpleSnippet {...this.props}
      className={this.props.content.test}
      customElements={{em: customElement}}
      textStyle={textStyle}
      extraContent={this.renderDonations()} />);
  }
}

export const EOYSnippet = props => {
  const extendedContent = {
    monthly_checkbox_label_text: schema.properties.monthly_checkbox_label_text.default,
    locale: schema.properties.locale.default,
    currency_code: schema.properties.currency_code.default,
    selected_button: schema.properties.selected_button.default,
    ...props.content,
  };

  return (<EOYSnippetBase
    {...props}
    content={extendedContent}
    form_method="GET" />);
};
