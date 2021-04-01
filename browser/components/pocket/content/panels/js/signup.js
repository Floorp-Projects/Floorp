/* global $:false, Handlebars:false, thePKT_PANEL:false */
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
    function clickHelper(e, linkData) {
      e.preventDefault();
      thePKT_PANEL.sendMessage("PKT_openTabWithUrl", {
        url: linkData.url,
        activate: true,
        source: linkData.source || "",
      });
    }
    $(".pkt_ext_learnmore").click(function(e) {
      clickHelper(e, {
        source: "learn_more",
        url: $(this).attr("href"),
      });
    });
    $(".signup-btn-firefox").click(function(e) {
      clickHelper(e, {
        source: "sign_up_1",
        url: $(this).attr("href"),
      });
    });
    $(".signup-btn-email").click(function(e) {
      clickHelper(e, {
        source: "sign_up_2",
        url: $(this).attr("href"),
      });
    });
    $(".pkt_ext_login").click(function(e) {
      clickHelper(e, {
        source: "log_in",
        url: $(this).attr("href"),
      });
    });
    // A generic click we don't do anything special for.
    // Was used for an experiment, possibly not needed anymore.
    $(".signup-btn-tryitnow").click(function(e) {
      clickHelper(e, {
        url: $(this).attr("href"),
      });
    });
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
      $("body").addClass("pkt_ext_signup_" + this.locale);
    }

    // Create actual content
    if (this.variant == "overflow") {
      $("body").append(Handlebars.templates.signup_shell(this.dictJSON));
    } else {
      // Logged Out Display Variants for MV Testing
      let variants = {
        control: "signupstoryboard_shell",
        variant_a: "variant_a",
        variant_b: "variant_b",
        variant_c: "variant_c",
        button_variant: "signupstoryboard_shell",
        button_control: "signupstoryboard_shell",
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

      if (loggedOutVariantTemplate !== `signupstoryboard_shell`) {
        $("body").addClass(`
          los_variant los_${loggedOutVariantTemplate}
        `);
      }

      $("body").append(
        Handlebars.templates[loggedOutVariantTemplate || variants.control](
          this.dictJSON
        )
      );
    }

    // close events
    this.initCloseTabEvents();

    // tell back end we're ready
    thePKT_PANEL.sendMessage("PKT_show_signup");
  };
};
