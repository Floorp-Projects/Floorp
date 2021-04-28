/* global Handlebars:false, thePKT_PANEL:false */
/* import-globals-from messages.js */

/*
PKT_PANEL_OVERLAY is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/

var PKT_PANEL_OVERLAY = function(options) {
  this.inited = false;
  this.active = false;
  this.delayedStateSaved = false;
  this.wrapper = null;
  this.variant = window.___PKT__SIGNUP_VARIANT;
  this.tagline = window.___PKT__SIGNUP_TAGLINE || "";
  this.preventCloseTimerCancel = false;
  this.translations = {};
  this.closeValid = true;
  this.mouseInside = false;
  this.autocloseTimer = null;
  this.variant = "";
  this.controlvariant;
  this.pockethost = "getpocket.com";
  this.loggedOutVariant = "control";
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
  this.sanitizeText = function(s) {
    var sanitizeMap = {
      "&": "&amp;",
      "<": "&lt;",
      ">": "&gt;",
      '"': "&quot;",
      "'": "&#39;",
    };
    if (typeof s !== "string") {
      return "";
    }
    return String(s).replace(/[&<>"']/g, function(str) {
      return sanitizeMap[str];
    });
  };
  this.getTranslations = function() {
    this.dictJSON = window.pocketStrings;
  };
  this.create = function() {
    const parser = new DOMParser();
    let elBody = document.querySelector(`body`);

    var controlvariant = window.location.href.match(
      /controlvariant=([\w|\.]*)&?/
    );
    if (controlvariant && controlvariant.length > 1) {
      this.controlvariant = controlvariant[1];
    }
    var variant = window.location.href.match(/variant=([\w|\.]*)&?/);
    if (variant && variant.length > 1) {
      this.variant = variant[1];
    }
    var loggedOutVariant = window.location.href.match(
      /loggedOutVariant=([\w|\.]*)&?/
    );
    if (loggedOutVariant && loggedOutVariant.length > 1) {
      this.loggedOutVariant = loggedOutVariant[1];
    }
    var host = window.location.href.match(/pockethost=([\w|\.]*)&?/);
    if (host && host.length > 1) {
      this.pockethost = host[1];
    }
    var locale = window.location.href.match(/locale=([\w|\.]*)&?/);
    if (locale && locale.length > 1) {
      this.locale = locale[1].toLowerCase();
    }

    if (this.active) {
      return;
    }
    this.active = true;

    // set translations
    this.getTranslations();
    this.dictJSON.controlvariant = this.controlvariant == "true" ? 1 : 0;
    this.dictJSON.variant = this.variant ? this.variant : "undefined";
    this.dictJSON.pockethost = this.pockethost;
    this.dictJSON.showlearnmore = true;
    this.dictJSON.utmCampaign = "logged_out_save_test";
    this.dictJSON.utmSource = "control";

    // extra modifier class for language
    if (this.locale) {
      elBody.classList.add(`pkt_ext_signup_${this.locale}`);
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

    let loggedOutVariantTemplate = variants[this.loggedOutVariant];
    if (
      this.loggedOutVariant === "button_variant" ||
      this.loggedOutVariant === "button_control"
    ) {
      this.dictJSON.buttonVariant = true;
      this.dictJSON.utmCampaign = "logged_out_button_test";
      this.dictJSON.utmSource = "button_control";
      if (this.loggedOutVariant === "button_variant") {
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
