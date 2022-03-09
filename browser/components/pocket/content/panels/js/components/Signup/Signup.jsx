/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import Header from "../Header/Header";
import Button from "../Button/Button";

function Signup(props) {
  const { locale } = props;
  return (
    <div className="stp_panel_container">
      <div className="stp_panel stp_panel_signup">
        <Header>
          <Button style="secondary">
            <span data-l10n-id="pocket-panel-signup-login" />
          </Button>
        </Header>
        <hr />
        {locale?.startsWith("en") ? (
          <>
            <div className="stp_signup_content_wrapper">
              <h3
                className="header_medium"
                data-l10n-id="pocket-panel-signup-cta-a-fix"
              />
              <p data-l10n-id="pocket-panel-signup-cta-b" />
            </div>
            <div className="stp_signup_content_wrapper">
              <hr />
            </div>
            <div className="stp_signup_content_wrapper">
              <div className="stp_signup_img_rainbow_reader" />
              <h3 className="header_medium">
                Get thought-provoking article recommendations
              </h3>
              <p>
                Find stories that go deep into a subject or offer a new
                perspective.
              </p>
            </div>
          </>
        ) : (
          <div className="stp_signup_content_wrapper">
            <h3
              className="header_large"
              data-l10n-id="pocket-panel-signup-cta-a-fix"
            />
            <p data-l10n-id="pocket-panel-signup-cta-b-short" />
            <strong>
              <p data-l10n-id="pocket-panel-signup-cta-c" />
            </strong>
          </div>
        )}
        <hr />
        <span className="stp_button_wide">
          <Button style="primary">
            <span data-l10n-id="pocket-panel-button-activate" />
          </Button>
        </span>
      </div>
    </div>
  );
}

export default Signup;
