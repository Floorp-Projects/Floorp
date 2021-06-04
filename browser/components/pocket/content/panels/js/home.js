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
  this.pockethost = "getpocket.com";
  this.dictJSON = {};

  this.parseHTML = function(htmlString) {
    const parser = new DOMParser();
    return parser.parseFromString(htmlString, `text/html`).documentElement;
  };
  this.initCloseTabEvents = function() {
    function clickHelper(e, linkData) {
      e.preventDefault();
      thePKT_PANEL.sendMessage("PKT_openTabWithUrl", {
        url: linkData.url,
        activate: true,
        source: linkData.source || "",
        position: linkData.position,
      });
    }

    document.querySelector(`.pkt_ext_mylist`).addEventListener(`click`, e => {
      clickHelper(e, {
        source: "home_view_list",
        url: e.currentTarget.getAttribute(`href`),
      });
    });

    document.querySelectorAll(`.pkt_ext_topic`).forEach((el, position) => {
      el.addEventListener(`click`, e => {
        clickHelper(e, {
          source: "home_topic",
          url: e.currentTarget.getAttribute(`href`),
          position,
        });
      });
    });

    document.querySelector(`.pkt_ext_discover`).addEventListener(`click`, e => {
      clickHelper(e, {
        source: "home_discover",
        url: e.currentTarget.getAttribute(`href`),
      });
    });
  };
  this.getTranslations = function() {
    this.dictJSON = window.pocketStrings;
  };
};

PKT_PANEL_OVERLAY.prototype = {
  create() {
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

    // For English, we have a discover topics link.
    // For non English, we don't have a link yet for this.
    // When we do, we can consider flipping this on.
    const enableLocalizedExploreMore = false;

    // set translations
    this.getTranslations();
    this.dictJSON.pockethost = this.pockethost;
    this.dictJSON.utmsource = "firefox-button";

    // extra modifier class for language
    if (this.locale) {
      document
        .querySelector(`body`)
        .classList.add(`pkt_ext_home_${this.locale}`);
    }

    // Create actual content
    document
      .querySelector(`body`)
      .append(this.parseHTML(Handlebars.templates.home_shell(this.dictJSON)));

    // We only have topic pages in English,
    // so ensure we only show a topics section for English browsers.
    if (this.locale.startsWith("en")) {
      const data = {
        explorepopulartopics: this.dictJSON.explorepopulartopics,
        discovermore: this.dictJSON.discovermore,
        pockethost: this.dictJSON.pockethost,
        utmsource: this.dictJSON.utmsource,
        topics: [
          { title: "Self Improvement", topic: "self-improvement" },
          { title: "Food", topic: "food" },
          { title: "Entertainment", topic: "entertainment" },
          { title: "Science", topic: "science" },
        ],
      };
      document
        .querySelector(`.pkt_ext_more`)
        .append(this.parseHTML(Handlebars.templates.popular_topics(data)));
    } else if (enableLocalizedExploreMore) {
      // For non English, we have a slightly different component to the page.
      document
        .querySelector(`.pkt_ext_more`)
        .append(
          this.parseHTML(Handlebars.templates.explore_more(this.dictJSON))
        );
    }

    // close events
    this.initCloseTabEvents();

    // tell back end we're ready
    thePKT_PANEL.sendMessage("PKT_show_home");
  },
};
