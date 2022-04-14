/* global Handlebars:false */

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

  this.setupClickEvents = function() {
    pktPanelMessaging.clickHelper(
      document.querySelector(`.pkt_ext_learnmore`),
      {
        source: `learn_more`,
      }
    );
    pktPanelMessaging.clickHelper(
      document.querySelector(`.signup-btn-firefox`),
      {
        source: `sign_up_1`,
      }
    );
    pktPanelMessaging.clickHelper(document.querySelector(`.signup-btn-email`), {
      source: `sign_up_2`,
    });
    pktPanelMessaging.clickHelper(document.querySelector(`.pkt_ext_login`), {
      source: `log_in`,
    });
  };
  this.create = function({ pockethost }) {
    const parser = new DOMParser();
    let elBody = document.querySelector(`body`);

    // Extract local variables passed into template via URL query params
    const { searchParams } = new URL(window.location.href);
    const isEmailSignupEnabled = searchParams.get(`emailButton`) === `true`;
    const locale = searchParams.get(`locale`) || ``;
    const language = locale.split(`-`)[0].toLowerCase();
    const layoutRefresh = searchParams.get(`layoutRefresh`) === `true`;
    const utmSource = searchParams.get(`utmSource`);
    const utmCampaign = searchParams.get(`utmCampaign`);
    const utmContent = searchParams.get(`utmContent`);

    if (this.active) {
      return;
    }
    this.active = true;

    if (layoutRefresh) {
      // For now, we need to do a little work on the body element
      // to support both old and new versions.
      document
        .querySelector(`.pkt_ext_containersignup`)
        ?.classList.add(`stp_signup_body`);
      document
        .querySelector(`.pkt_ext_containersignup`)
        ?.classList.remove(`pkt_ext_containersignup`);
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
    } else {
      const templateData = {
        pockethost,
        utmCampaign: utmCampaign || `firefox_door_hanger_menu`,
        // utmContent is now used for experiment branch in the new layouts,
        // but for backwards comp reasons, we pass it in the old way as utmSource.
        utmSource: utmContent || `control`,
      };

      // extra modifier class for language
      if (language) {
        elBody.classList.add(`pkt_ext_signup_${language}`);
      }

      // Create actual content
      elBody.append(
        parser.parseFromString(
          Handlebars.templates.signup_shell(templateData),
          `text/html`
        ).documentElement
      );

      // Remove email button based on `extensions.pocket.refresh.emailButton.enabled` pref
      if (!isEmailSignupEnabled) {
        document.querySelector(`.btn-container-email`).remove();
      }

      // click events
      this.setupClickEvents();
    }

    // tell back end we're ready
    pktPanelMessaging.sendMessage("PKT_show_signup");
  };
};

export default SignupOverlay;
