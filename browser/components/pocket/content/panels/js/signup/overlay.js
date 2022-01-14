/* global Handlebars:false */

/*
SignupOverlay is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/

import pktPanelMessaging from "../messages.js";

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
  this.create = function() {
    const parser = new DOMParser();
    let elBody = document.querySelector(`body`);

    // Extract local variables passed into template via URL query params
    let queryParams = new URL(window.location.href).searchParams;
    let isEmailSignupEnabled = queryParams.get(`emailButton`) === `true`;
    let pockethost = queryParams.get(`pockethost`) || `getpocket.com`;
    let utmCampaign =
      queryParams.get(`utmCampaign`) || `firefox_door_hanger_menu`;
    let utmSource = queryParams.get(`utmSource`) || `control`;
    let language = queryParams
      .get(`locale`)
      ?.split(`-`)[0]
      .toLowerCase();

    if (this.active) {
      return;
    }
    this.active = true;

    const templateData = {
      pockethost,
      utmCampaign,
      utmSource,
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

    // tell back end we're ready
    pktPanelMessaging.sendMessage("PKT_show_signup");
  };
};

export default SignupOverlay;
