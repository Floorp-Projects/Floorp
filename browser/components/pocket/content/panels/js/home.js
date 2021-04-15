/* global $:false, Handlebars:false, thePKT_PANEL:false */
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
    $(".pkt_ext_mylist").click(function(e) {
      clickHelper(e, {
        source: "home_view_list",
        url: $(this).attr("href"),
      });
    });
    $(".pkt_ext_topic").click(function(e) {
      const position = $(".pkt_ext_topic").index(this);
      clickHelper(e, {
        source: "home_topic",
        url: $(this).attr("href"),
        position,
      });
    });
    $(".pkt_ext_discover").click(function(e) {
      clickHelper(e, {
        source: "home_discover",
        url: $(this).attr("href"),
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
      $("body").addClass("pkt_ext_home_" + this.locale);
    }

    // Create actual content
    $("body").append(Handlebars.templates.home_shell(this.dictJSON));

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
      const topics = Handlebars.templates.popular_topics(data);
      $(".pkt_ext_more").append(topics);
    } else if (enableLocalizedExploreMore) {
      // For non English, we have a slightly different component to the page.
      const explore = Handlebars.templates.explore_more(this.dictJSON);
      $(".pkt_ext_more").append(explore);
    }

    // close events
    this.initCloseTabEvents();

    // tell back end we're ready
    thePKT_PANEL.sendMessage("PKT_show_home");
  },
};
