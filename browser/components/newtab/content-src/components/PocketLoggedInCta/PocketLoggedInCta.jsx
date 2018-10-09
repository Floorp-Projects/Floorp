import {connect} from "react-redux";
import {FormattedMessage} from "react-intl";
import React from "react";

export class _PocketLoggedInCta extends React.PureComponent {
  render() {
    const {pocketCta} = this.props.Pocket;
    return (
      <span className="pocket-logged-in-cta">
        <a className="pocket-cta-button" href={pocketCta.ctaUrl ? pocketCta.ctaUrl : "https://getpocket.com/"}>
         {pocketCta.ctaButton ? pocketCta.ctaButton : <FormattedMessage id="pocket_cta_button" />}
        </a>

        <a href={pocketCta.ctaUrl ? pocketCta.ctaUrl : "https://getpocket.com/"}>
          <span className="cta-text">
           {pocketCta.ctaText ? pocketCta.ctaText : <FormattedMessage id="pocket_cta_text" />}
          </span>
        </a>
      </span>
    );
  }
}

export const PocketLoggedInCta = connect(state => ({Pocket: state.Pocket}))(_PocketLoggedInCta);
