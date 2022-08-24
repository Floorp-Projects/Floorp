/*
SignupOverlay is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/

import React from "react";
import ReactDOM from "react-dom";
import pktPanelMessaging from "../messages.js";
import Signup from "../components/Signup/Signup";

var SignupOverlay = function(options) {
  this.inited = false;
  this.active = false;

  this.create = function({ pockethost }) {
    // Extract local variables passed into template via URL query params
    const { searchParams } = new URL(window.location.href);
    const locale = searchParams.get(`locale`) || ``;
    const utmSource = searchParams.get(`utmSource`);
    const utmCampaign = searchParams.get(`utmCampaign`);
    const utmContent = searchParams.get(`utmContent`);

    if (this.active) {
      return;
    }

    this.active = true;

    // Create actual content
    ReactDOM.render(
      <Signup
        pockethost={pockethost}
        utmSource={utmSource}
        utmCampaign={utmCampaign}
        utmContent={utmContent}
        locale={locale}
      />,
      document.querySelector(`body`)
    );

    if (window?.matchMedia(`(prefers-color-scheme: dark)`).matches) {
      document.querySelector(`body`).classList.add(`theme_dark`);
    }

    // tell back end we're ready
    pktPanelMessaging.sendMessage("PKT_show_signup");
  };
};

export default SignupOverlay;
