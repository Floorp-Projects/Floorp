/* global Handlebars:false */

/*
HomeOverlay is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/

import React from "react";
import ReactDOM from "react-dom";
import PopularTopicsLegacy from "../components/PopularTopicsLegacy/PopularTopicsLegacy";
import Home from "../components/Home/Home";
import pktPanelMessaging from "../messages.js";

var HomeOverlay = function(options) {
  this.inited = false;
  this.active = false;
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
  create({ pockethost }) {
    const { searchParams } = new URL(window.location.href);
    const locale = searchParams.get(`locale`) || ``;
    const layoutRefresh = searchParams.get(`layoutRefresh`) === `true`;
    const hideRecentSaves = searchParams.get(`hiderecentsaves`) === `true`;
    const utmSource = searchParams.get(`utmSource`);
    const utmCampaign = searchParams.get(`utmCampaign`);
    const utmContent = searchParams.get(`utmContent`);

    if (this.active) {
      return;
    }
    this.active = true;

    if (layoutRefresh) {
      // Create actual content
      ReactDOM.render(
        <Home
          locale={locale}
          hideRecentSaves={hideRecentSaves}
          pockethost={pockethost}
          utmSource={utmSource}
          utmCampaign={utmCampaign}
          utmContent={utmContent}
          topics={[
            { title: "Technology", topic: "technology" },
            { title: "Self Improvement", topic: "self-improvement" },
            { title: "Food", topic: "food" },
            { title: "Parenting", topic: "parenting" },
            { title: "Science", topic: "science" },
            { title: "Entertainment", topic: "entertainment" },
            { title: "Career", topic: "career" },
            { title: "Health", topic: "health" },
            { title: "Travel", topic: "travel" },
            { title: "Must-Reads", topic: "must-reads" },
          ]}
        />,
        document.querySelector(`body`)
      );

      if (window?.matchMedia(`(prefers-color-scheme: dark)`).matches) {
        document.querySelector(`body`).classList.add(`theme_dark`);
      }
    } else {
      // For English, we have a discover topics link.
      // For non English, we don't have a link yet for this.
      // When we do, we can consider flipping this on.
      const enableLocalizedExploreMore = false;
      const templateData = {
        pockethost,
        utmsource: `firefox-button`,
      };

      // Create actual content
      document
        .querySelector(`body`)
        .append(this.parseHTML(Handlebars.templates.home_shell(templateData)));

      // We only have topic pages in English,
      // so ensure we only show a topics section for English browsers.
      if (locale.startsWith("en")) {
        ReactDOM.render(
          <PopularTopicsLegacy
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
    }
  },
};

export default HomeOverlay;
