/* global Handlebars:false, thePKT_PANEL:false */
/* import-globals-from messages.js */

/*
PKT_PANEL_OVERLAY is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/

var PKT_PANEL_OVERLAY = function(options) {
  this.inited = false;
  this.active = false;
  this.translations = {};
  this.dictJSON = {};

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

    // A generic click we don't do anything special for.
    // Was used for an experiment, possibly not needed anymore.
    clickHelper(`.signup-btn-tryitnow`);
  };

  this.getTranslations = function() {
    this.dictJSON = window.pocketStrings;
  };

  this.create = function() {
    const parser = new DOMParser();
    let elBody = document.querySelector(`body`);

    // Extract local variables passed into template via URL query params
    let queryParams = new URL(window.location.href).searchParams;
    let controlvariant = queryParams.get(`controlvariant`);
    let variant = queryParams.get(`variant`);
    let loggedOutVariant = queryParams.get(`loggedOutVariant`);
    let pockethost = queryParams.get(`pockethost`) || `getpocket.com`;
    let language = queryParams
      .get(`locale`)
      ?.split(`-`)[0]
      .toLowerCase();

    if (this.active) {
      return;
    }
    this.active = true;

    // set translations
    this.getTranslations();
    this.dictJSON.controlvariant = controlvariant == "true" ? 1 : 0;
    this.dictJSON.variant = variant ? variant : "undefined";
    this.dictJSON.pockethost = pockethost;
    this.dictJSON.showlearnmore = true;
    this.dictJSON.utmCampaign = "logged_out_save_test";
    this.dictJSON.utmSource = "control";

    // extra modifier class for language
    if (language) {
      elBody.classList.add(`pkt_ext_signup_${language}`);
    }

    // Logged Out Display Variants for MV Testing
    let variants = {
      control: "signup_shell",
      variant_a: "variant_a",
      variant_b: "variant_b",
      variant_c: "variant_c",
      button_variant: "signup_shell",
      button_control: "signup_shell",
    };

    let loggedOutVariantTemplate = variants[loggedOutVariant];

    if (
      loggedOutVariant === "button_variant" ||
      loggedOutVariant === "button_control"
    ) {
      this.dictJSON.buttonVariant = true;
      this.dictJSON.utmCampaign = "logged_out_button_test";
      this.dictJSON.utmSource = "button_control";
      if (loggedOutVariant === "button_variant") {
        this.dictJSON.oneButton = true;
        this.dictJSON.utmSource = "button_variant";
      }
    }

    if (loggedOutVariantTemplate !== `signup_shell`) {
      elBody.classList.add(`los_variant`);
      elBody.classList.add(`los_${loggedOutVariantTemplate}`);
    }

    // Create actual content
    elBody.append(
      parser.parseFromString(
        Handlebars.templates[loggedOutVariantTemplate || variants.control](
          this.dictJSON
        ),
        `text/html`
      ).documentElement
    );

    // close events
    this.initCloseTabEvents();

    // tell back end we're ready
    thePKT_PANEL.sendMessage("PKT_show_signup");
  };
};
