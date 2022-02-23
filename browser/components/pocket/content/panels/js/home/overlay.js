/* global Handlebars:false */

/*
HomeOverlay is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/

import React from "react";
import ReactDOM from "react-dom";
import PopularTopics from "../components/PopularTopics";
import pktPanelMessaging from "../messages.js";

var HomeOverlay = function(options) {
  this.inited = false;
  this.active = false;
  this.pockethost = "getpocket.com";
  this.parseHTML = function(htmlString) {
    const parser = new DOMParser();
    return parser.parseFromString(htmlString, `text/html`).documentElement;
  };

  this.setupClickEvents = function() {
    pktPanelMessaging.clickHelper(document.querySelector(`.pkt_ext_mylist`), {
      source: `home_view_list`,
    });
    pktPanelMessaging.clickHelper(document.querySelector(`.pkt_ext_discover`), {
      source: `home_discover`,
    });

    document.querySelectorAll(`.pkt_ext_topic`).forEach((el, position) => {
      pktPanelMessaging.clickHelper(el, {
        source: `home_topic`,
        position,
      });
    });
  };
};

HomeOverlay.prototype = {
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
    const templateData = {
      pockethost: this.pockethost,
      utmsource: "firefox-button",
    };

    // extra modifier class for language
    if (this.locale) {
      document
        .querySelector(`body`)
        .classList.add(`pkt_ext_home_${this.locale}`);
    }

    // Create actual content
    document
      .querySelector(`body`)
      .append(this.parseHTML(Handlebars.templates.home_shell(templateData)));

    // We only have topic pages in English,
    // so ensure we only show a topics section for English browsers.
    if (this.locale.startsWith("en")) {
      ReactDOM.render(
        <PopularTopics
          pockethost={templateData.pockethost}
          utmsource={templateData.utmsource}
          topics={[
            { title: "Self Improvement", topic: "self-improvement" },
            { title: "Food", topic: "food" },
            { title: "Entertainment", topic: "entertainment" },
            { title: "Science", topic: "science" },
          ]}
        />,
        document.querySelector(`.pkt_ext_more`)
      );
    } else if (enableLocalizedExploreMore) {
      // For non English, we have a slightly different component to the page.
      document
        .querySelector(`.pkt_ext_more`)
        .append(this.parseHTML(Handlebars.templates.explore_more()));
    }

    // click events
    this.setupClickEvents();

    // tell back end we're ready
    pktPanelMessaging.sendMessage("PKT_show_home");
  },
};

export default HomeOverlay;
