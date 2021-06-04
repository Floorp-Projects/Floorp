/* global Handlebars:false, thePKT_PANEL:false */
/* import-globals-from messages.js */

/*
PKT_PANEL_OVERLAY is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/

var PKT_PANEL_OVERLAY = function(options) {
  this.inited = false;
  this.active = false;
  this.initCloseTabEvents = function() {
    function clickHelper(selector, source) {
      document.querySelector(selector)?.addEventListener(`click`, event => {
        event.preventDefault();

        thePKT_PANEL.sendMessage("PKT_openTabWithUrl", {
          url: event.currentTarget.getAttribute(`href`),
          activate: true,
          source: source || "",
        });
      });
    }

    clickHelper(`.pkt_ext_learnmore`, `learn_more`);
    clickHelper(`.signup-btn-firefox`, `sign_up_1`);
    clickHelper(`.signup-btn-email`, `sign_up_2`);
    clickHelper(`.pkt_ext_login`, `log_in`);
  };
  this.create = function() {
    const parser = new DOMParser();
    let elBody = document.querySelector(`body`);

    // Extract local variables passed into template via URL query params
    let queryParams = new URL(window.location.href).searchParams;
    let pockethost = queryParams.get(`pockethost`) || `getpocket.com`;
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
      utmCampaign: "firefox_door_hanger_menu",
      utmSource: "control",
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

    // close events
    this.initCloseTabEvents();

    // tell back end we're ready
    thePKT_PANEL.sendMessage("PKT_show_signup");
  };
};
