/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React from "react";
import Header from "../Header/Header";

function Signup(props) {
  const { locale } = props;
  return (
    <div className="stp_panel_container">
      <div className="stp_panel stp_panel_home">
        <Header>
          <a>
            <span data-l10n-id="pocket-panel-header-sign-in"></span>
          </a>
        </Header>
        <hr />
        <p data-l10n-id="pocket-panel-signup-cta-a"></p>
        <p data-l10n-id="pocket-panel-signup-cta-b"></p>
        {locale?.startsWith("en") ? (
          <>
            <hr />
            <p>Get thought-provoking article recommendations</p>
            <p>
              Find stories that go deep into a subject or offer a new
              perspective.
            </p>
          </>
        ) : (
          <p data-l10n-id="pocket-panel-signup-cta-c"></p>
        )}
        <hr />
        <p data-l10n-id="pocket-panel-button-activate"></p>
      </div>
    </div>
  );
}

export default Signup;
