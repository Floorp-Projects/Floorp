/******/ (() => { // webpackBootstrap
/******/ 	"use strict";
/******/ 	var __webpack_modules__ = ({

/***/ 883:
/***/ ((__unused_webpack_module, __unused_webpack___webpack_exports__, __webpack_require__) => {


// EXTERNAL MODULE: ./node_modules/react/index.js
var react = __webpack_require__(294);
// EXTERNAL MODULE: ./node_modules/react-dom/index.js
var react_dom = __webpack_require__(935);
;// CONCATENATED MODULE: ./content/panels/js/components/PopularTopics.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


function PopularTopics(props) {
  return /*#__PURE__*/react.createElement(react.Fragment, null, /*#__PURE__*/react.createElement("h3", {
    "data-l10n-id": "pocket-panel-home-explore-popular-topics"
  }), /*#__PURE__*/react.createElement("ul", null, props.topics.map(item => /*#__PURE__*/react.createElement("li", {
    key: `item-${item.topic}`
  }, /*#__PURE__*/react.createElement("a", {
    className: "pkt_ext_topic",
    href: `https://${props.pockethost}/explore/${item.topic}?utm_source=${props.utmsource}`
  }, item.title, /*#__PURE__*/react.createElement("span", {
    className: "pkt_ext_chevron_right"
  }))))), /*#__PURE__*/react.createElement("a", {
    className: "pkt_ext_discover",
    href: `https://${props.pockethost}/explore?utm_source=${props.utmsource}`,
    "data-l10n-id": "pocket-panel-home-discover-more"
  }));
}

/* harmony default export */ const components_PopularTopics = (PopularTopics);
;// CONCATENATED MODULE: ./content/panels/js/messages.js
/* global RPMRemoveMessageListener:false, RPMAddMessageListener:false, RPMSendAsyncMessage:false */
var pktPanelMessaging = {
  removeMessageListener(messageId, callback) {
    RPMRemoveMessageListener(messageId, callback);
  },

  addMessageListener(messageId, callback = () => {}) {
    RPMAddMessageListener(messageId, callback);
  },

  sendMessage(messageId, payload = {}, callback) {
    if (callback) {
      // If we expect something back, we use RPMSendAsyncMessage and not RPMSendQuery.
      // Even though RPMSendQuery returns something, our frame could be closed at any moment,
      // and we don't want to close a RPMSendQuery promise loop unexpectedly.
      // So instead we setup a response event.
      const responseMessageId = `${messageId}_response`;

      var responseListener = responsePayload => {
        callback(responsePayload);
        this.removeMessageListener(responseMessageId, responseListener);
      };

      this.addMessageListener(responseMessageId, responseListener);
    } // Send message


    RPMSendAsyncMessage(messageId, payload);
  },

  // Click helper to reduce bugs caused by oversight
  // from different implementations of similar code.
  clickHelper(element, {
    source = "",
    position
  }) {
    element?.addEventListener(`click`, event => {
      event.preventDefault();
      this.sendMessage("PKT_openTabWithUrl", {
        url: event.currentTarget.getAttribute(`href`),
        activate: true,
        source,
        position
      });
    });
  },

  log() {
    RPMSendAsyncMessage("PKT_log", arguments);
  }

};
/* harmony default export */ const messages = (pktPanelMessaging);
;// CONCATENATED MODULE: ./content/panels/js/home/overlay.js
/* global Handlebars:false */

/*
HomeOverlay is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/





var HomeOverlay = function (options) {
  this.inited = false;
  this.active = false;
  this.pockethost = "getpocket.com";

  this.parseHTML = function (htmlString) {
    const parser = new DOMParser();
    return parser.parseFromString(htmlString, `text/html`).documentElement;
  };

  this.setupClickEvents = function () {
    messages.clickHelper(document.querySelector(`.pkt_ext_mylist`), {
      source: `home_view_list`
    });
    messages.clickHelper(document.querySelector(`.pkt_ext_discover`), {
      source: `home_discover`
    });
    document.querySelectorAll(`.pkt_ext_topic`).forEach((el, position) => {
      messages.clickHelper(el, {
        source: `home_topic`,
        position
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

    this.active = true; // For English, we have a discover topics link.
    // For non English, we don't have a link yet for this.
    // When we do, we can consider flipping this on.

    const enableLocalizedExploreMore = false;
    const templateData = {
      pockethost: this.pockethost,
      utmsource: "firefox-button"
    }; // extra modifier class for language

    if (this.locale) {
      document.querySelector(`body`).classList.add(`pkt_ext_home_${this.locale}`);
    } // Create actual content


    document.querySelector(`body`).append(this.parseHTML(Handlebars.templates.home_shell(templateData))); // We only have topic pages in English,
    // so ensure we only show a topics section for English browsers.

    if (this.locale.startsWith("en")) {
      react_dom.render( /*#__PURE__*/react.createElement(components_PopularTopics, {
        pockethost: templateData.pockethost,
        utmsource: templateData.utmsource,
        topics: [{
          title: "Self Improvement",
          topic: "self-improvement"
        }, {
          title: "Food",
          topic: "food"
        }, {
          title: "Entertainment",
          topic: "entertainment"
        }, {
          title: "Science",
          topic: "science"
        }]
      }), document.querySelector(`.pkt_ext_more`));
    } else if (enableLocalizedExploreMore) {
      // For non English, we have a slightly different component to the page.
      document.querySelector(`.pkt_ext_more`).append(this.parseHTML(Handlebars.templates.explore_more()));
    } // click events


    this.setupClickEvents(); // tell back end we're ready

    messages.sendMessage("PKT_show_home");
  }

};
/* harmony default export */ const overlay = (HomeOverlay);
;// CONCATENATED MODULE: ./content/panels/js/signup/overlay.js
/* global Handlebars:false */

/*
SignupOverlay is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/


var SignupOverlay = function (options) {
  this.inited = false;
  this.active = false;

  this.setupClickEvents = function () {
    messages.clickHelper(document.querySelector(`.pkt_ext_learnmore`), {
      source: `learn_more`
    });
    messages.clickHelper(document.querySelector(`.signup-btn-firefox`), {
      source: `sign_up_1`
    });
    messages.clickHelper(document.querySelector(`.signup-btn-email`), {
      source: `sign_up_2`
    });
    messages.clickHelper(document.querySelector(`.pkt_ext_login`), {
      source: `log_in`
    });
  };

  this.create = function () {
    const parser = new DOMParser();
    let elBody = document.querySelector(`body`); // Extract local variables passed into template via URL query params

    let queryParams = new URL(window.location.href).searchParams;
    let isEmailSignupEnabled = queryParams.get(`emailButton`) === `true`;
    let pockethost = queryParams.get(`pockethost`) || `getpocket.com`;
    let utmCampaign = queryParams.get(`utmCampaign`) || `firefox_door_hanger_menu`;
    let utmSource = queryParams.get(`utmSource`) || `control`;
    let language = queryParams.get(`locale`)?.split(`-`)[0].toLowerCase();

    if (this.active) {
      return;
    }

    this.active = true;
    const templateData = {
      pockethost,
      utmCampaign,
      utmSource
    }; // extra modifier class for language

    if (language) {
      elBody.classList.add(`pkt_ext_signup_${language}`);
    } // Create actual content


    elBody.append(parser.parseFromString(Handlebars.templates.signup_shell(templateData), `text/html`).documentElement); // Remove email button based on `extensions.pocket.refresh.emailButton.enabled` pref

    if (!isEmailSignupEnabled) {
      document.querySelector(`.btn-container-email`).remove();
    } // click events


    this.setupClickEvents(); // tell back end we're ready

    messages.sendMessage("PKT_show_signup");
  };
};

/* harmony default export */ const signup_overlay = (SignupOverlay);
;// CONCATENATED MODULE: ./content/panels/js/saved/overlay.js
/* global Handlebars:false */

/*
SavedOverlay is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/


var SavedOverlay = function (options) {
  var myself = this;
  this.inited = false;
  this.active = false;
  this.pockethost = "getpocket.com";
  this.savedItemId = 0;
  this.savedUrl = "";
  this.premiumStatus = false;
  this.userTags = [];
  this.tagsDropdownOpen = false;
  this.fxasignedin = false;

  this.parseHTML = function (htmlString) {
    const parser = new DOMParser();
    return parser.parseFromString(htmlString, `text/html`).documentElement;
  };

  this.fillTagContainer = function (tags, container, tagclass) {
    while (container.firstChild) {
      container.firstChild.remove();
    }

    for (let i = 0; i < tags.length; i++) {
      let newtag = this.parseHTML(`<li class="${tagclass}"><a href="#" class="token_tag">${tags[i]}</a></li>`);
      container.append(newtag);
    }
  };

  this.fillUserTags = function () {
    messages.sendMessage("PKT_getTags", {}, function (resp) {
      const {
        data
      } = resp;

      if (typeof data == "object" && typeof data.tags == "object") {
        myself.userTags = data.tags;
      }
    });
  };

  this.fillSuggestedTags = function () {
    if (!document.querySelector(`.pkt_ext_suggestedtag_detail`)) {
      myself.suggestedTagsLoaded = true;
      return;
    }

    document.querySelector(`.pkt_ext_subshell`).style.display = `block`;
    messages.sendMessage("PKT_getSuggestedTags", {
      url: myself.savedUrl
    }, function (resp) {
      const {
        data
      } = resp;
      document.querySelector(`.pkt_ext_suggestedtag_detail`).classList.remove(`pkt_ext_suggestedtag_detail_loading`);

      if (data.status == "success") {
        var newtags = [];

        for (let i = 0; i < data.value.suggestedTags.length; i++) {
          newtags.push(data.value.suggestedTags[i].tag);
        }

        myself.suggestedTagsLoaded = true;
        myself.fillTagContainer(newtags, document.querySelector(`.pkt_ext_suggestedtag_detail ul`), "token_suggestedtag");
      } else if (data.status == "error") {
        let elMsg = myself.parseHTML(`<p class="suggestedtag_msg">${data.error.message}</p>`);
        document.querySelector(`.pkt_ext_suggestedtag_detail`).append(elMsg);
        this.suggestedTagsLoaded = true;
      }
    });
  };

  this.closePopup = function () {
    messages.sendMessage("PKT_close");
  };

  this.checkValidTagSubmit = function () {
    let inputlength = document.querySelector(`.pkt_ext_tag_input_wrapper .token-input-input-token input`).value.trim().length;

    if (document.querySelector(`.pkt_ext_containersaved .token-input-token`) || inputlength > 0 && inputlength < 26) {
      document.querySelector(`.pkt_ext_containersaved .pkt_ext_btn`).classList.remove(`pkt_ext_btn_disabled`);
    } else {
      document.querySelector(`.pkt_ext_containersaved .pkt_ext_btn`).classList.add(`pkt_ext_btn_disabled`);
    }

    myself.updateSlidingTagList();
  };

  this.updateSlidingTagList = function () {
    let cssDir = document.dir === `ltr` ? `left` : `right`;
    let offsetDir = document.dir === `ltr` ? `offsetLeft` : `offsetRight`;
    let elTokenInputList = document.querySelector(`.token-input-list`);
    let inputleft = document.querySelector(`.token-input-input-token input`)[offsetDir];
    let listleft = elTokenInputList[offsetDir];
    let listleftnatural = listleft - parseInt(getComputedStyle(elTokenInputList)[cssDir]);
    let leftwidth = document.querySelector(`.pkt_ext_tag_input_wrapper`).offsetWidth;

    if (inputleft + listleft + 20 > leftwidth) {
      elTokenInputList.style[cssDir] = `${Math.min((inputleft + listleftnatural - leftwidth + 20) * -1, 0)}px`;
    } else {
      elTokenInputList.style[cssDir] = 0;
    }
  };

  this.checkPlaceholderStatus = function () {
    if (document.querySelector(`.pkt_ext_containersaved .pkt_ext_tag_input_wrapper .token-input-token`)) {
      document.querySelector(`.pkt_ext_containersaved .token-input-input-token input`).setAttribute(`placeholder`, ``);
    } else {
      let elTokenInput = document.querySelector(`.pkt_ext_containersaved .token-input-input-token input`);
      elTokenInput.setAttribute(`placeholder`, document.querySelector(`.pkt_ext_tag_input`).getAttribute(`placeholder`));
      elTokenInput.style.width = `200px`;
    }
  }; // TODO: Remove jQuery and re-enable eslint for this method once tokenInput is re-written as a React component

  /* eslint-disable no-undef */


  this.initTagInput = function () {
    var inputwrapper = $(".pkt_ext_tag_input_wrapper");
    inputwrapper.find(".pkt_ext_tag_input").tokenInput([], {
      searchDelay: 200,
      minChars: 1,
      animateDropdown: false,
      noResultsHideDropdown: true,
      scrollKeyboard: true,
      emptyInputLength: 200,

      search_function(term, cb) {
        var returnlist = [];

        if (term.length) {
          var limit = 15;
          var r = new RegExp("^" + term);

          for (var i = 0; i < myself.userTags.length; i++) {
            if (r.test(myself.userTags[i]) && limit > 0) {
              returnlist.push({
                name: myself.userTags[i]
              });
              limit--;
            }
          }
        }

        if (!$(".token-input-dropdown-tag").data("init")) {
          $(".token-input-dropdown-tag").css("width", inputwrapper.outerWidth()).data("init");
          inputwrapper.append($(".token-input-dropdown-tag"));
        }

        cb(returnlist);
      },

      validateItem(item) {
        const text = item.name;

        if ($.trim(text).length > 25 || !$.trim(text).length) {
          if (text.length > 25) {
            myself.showTagsLocalizedError("pocket-panel-saved-error-tag-length");
            this.changestamp = Date.now();
            setTimeout(function () {
              $(".token-input-input-token input").val(text).focus();
            }, 10);
          }

          return false;
        }

        myself.hideTagsError();
        return true;
      },

      textToData(text) {
        return {
          name: myself.sanitizeText(text.toLowerCase())
        };
      },

      onReady() {
        $(".token-input-dropdown").addClass("token-input-dropdown-tag");
        inputwrapper.find(".token-input-input-token input") // The token input does so element copy magic, but doesn't copy over l10n ids.
        // So we do it manually here.
        .attr("data-l10n-id", inputwrapper.find(".pkt_ext_tag_input").attr("data-l10n-id")).css("width", "200px");

        if ($(".pkt_ext_suggestedtag_detail").length) {
          $(".pkt_ext_containersaved").find(".pkt_ext_suggestedtag_detail").on("click", ".token_tag", function (e) {
            e.preventDefault();
            var tag = $(e.target);

            if ($(this).parents(".pkt_ext_suggestedtag_detail_disabled").length) {
              return;
            }

            inputwrapper.find(".pkt_ext_tag_input").tokenInput("add", {
              id: inputwrapper.find(".token-input-token").length,
              name: tag.text()
            });
            tag.addClass("token-suggestedtag-inactive");
            $(".token-input-input-token input").focus();
          });
        }

        $(".token-input-list").on("keydown", "input", function (e) {
          if (e.which == 37) {
            myself.updateSlidingTagList();
          }

          if (e.which === 9) {
            $("a.pkt_ext_learn_more").focus();
          }
        }).on("keypress", "input", function (e) {
          if (e.which == 13) {
            if (typeof this.changestamp == "undefined" || Date.now() - this.changestamp > 250) {
              e.preventDefault();
              document.querySelector(`.pkt_ext_containersaved .pkt_ext_btn`).click();
            }
          }
        }).on("keyup", "input", function (e) {
          myself.checkValidTagSubmit();
        });
        myself.checkPlaceholderStatus();
      },

      onAdd() {
        myself.checkValidTagSubmit();
        this.changestamp = Date.now();
        myself.hideInactiveTags();
        myself.checkPlaceholderStatus();
      },

      onDelete() {
        myself.checkValidTagSubmit();
        this.changestamp = Date.now();
        myself.showActiveTags();
        myself.checkPlaceholderStatus();
      },

      onShowDropdown() {
        myself.tagsDropdownOpen = true;
      },

      onHideDropdown() {
        myself.tagsDropdownOpen = false;
      }

    });
    $("body").on("keydown", function (e) {
      var key = e.keyCode || e.which;

      if (key == 8) {
        var selected = $(".token-input-selected-token");

        if (selected.length) {
          e.preventDefault();
          e.stopImmediatePropagation();
          inputwrapper.find(".pkt_ext_tag_input").tokenInput("remove", {
            name: selected.find("p").text()
          });
        }
      } else if ($(e.target).parent().hasClass("token-input-input-token")) {
        e.stopImmediatePropagation();
      }
    });
  };
  /* eslint-enable no-undef */


  this.disableInput = function () {
    document.querySelector(`.pkt_ext_containersaved .pkt_ext_item_actions`).classList.add("pkt_ext_item_actions_disabled");
    document.querySelector(`.pkt_ext_containersaved .pkt_ext_btn`).classList.add("pkt_ext_btn_disabled");
    document.querySelector(`.pkt_ext_containersaved .pkt_ext_tag_input_wrapper`).classList.add("pkt_ext_tag_input_wrapper_disabled");

    if (document.querySelector(`.pkt_ext_containersaved .pkt_ext_suggestedtag_detail`)) {
      document.querySelector(`.pkt_ext_containersaved .pkt_ext_suggestedtag_detail`).classList.add("pkt_ext_suggestedtag_detail_disabled");
    }
  };

  this.enableInput = function () {
    document.querySelector(`.pkt_ext_containersaved .pkt_ext_item_actions`).classList.remove("pkt_ext_item_actions_disabled");
    this.checkValidTagSubmit();
    document.querySelector(`.pkt_ext_containersaved .pkt_ext_tag_input_wrapper`).classList.remove("pkt_ext_tag_input_wrapper_disabled");

    if (document.querySelector(`.pkt_ext_containersaved .pkt_ext_suggestedtag_detail`)) {
      document.querySelector(`.pkt_ext_containersaved .pkt_ext_suggestedtag_detail`).classList.remove("pkt_ext_suggestedtag_detail_disabled");
    }
  };

  this.initAddTagInput = function () {
    document.querySelector(`.pkt_ext_btn`).addEventListener(`click`, e => {
      e.preventDefault();

      if (e.target.classList.contains(`pkt_ext_btn_disabled`) || document.querySelector(`.pkt_ext_edit_msg_active.pkt_ext_edit_msg_error`)) {
        return;
      }

      myself.disableInput();
      document.l10n.setAttributes(document.querySelector(`.pkt_ext_containersaved .pkt_ext_detail h2`), "pocket-panel-saved-processing-tags");
      let originaltags = [];
      document.querySelectorAll(`.token-input-token p`).forEach(el => {
        let text = el.textContent.trim();

        if (text.length) {
          originaltags.push(text);
        }
      });
      messages.sendMessage("PKT_addTags", {
        url: myself.savedUrl,
        tags: originaltags
      }, function (resp) {
        const {
          data
        } = resp;

        if (data.status == "success") {
          myself.showStateFinalLocalizedMsg("pocket-panel-saved-tags-saved");
        } else if (data.status == "error") {
          let elEditMsg = document.querySelector(`.pkt_ext_edit_msg`);
          elEditMsg.classList.add(`pkt_ext_edit_msg_error`, `pkt_ext_edit_msg_active`);
          elEditMsg.textContent = data.error.message;
        }
      });
    });
  };

  this.initRemovePageInput = function () {
    document.querySelector(`.pkt_ext_removeitem`).addEventListener(`click`, e => {
      document.querySelector(`.pkt_ext_subshell`).style.display = `none`;

      if (e.target.closest(`.pkt_ext_item_actions_disabled`)) {
        e.preventDefault();
        return;
      }

      if (e.target.classList.contains(`pkt_ext_removeitem`)) {
        e.preventDefault();
        myself.disableInput();
        document.l10n.setAttributes(document.querySelector(`.pkt_ext_containersaved .pkt_ext_detail h2`), "pocket-panel-saved-processing-remove");
        messages.sendMessage("PKT_deleteItem", {
          itemId: myself.savedItemId
        }, function (resp) {
          const {
            data
          } = resp;

          if (data.status == "success") {
            myself.showStateFinalLocalizedMsg("pocket-panel-saved-page-removed");
          } else if (data.status == "error") {
            let elEditMsg = document.querySelector(`.pkt_ext_edit_msg`);
            elEditMsg.classList.add(`pkt_ext_edit_msg_error`, `pkt_ext_edit_msg_active`);
            elEditMsg.textContent = data.error.message;
          }
        });
      }
    });
  };

  this.initOpenListInput = function () {
    messages.clickHelper(document.querySelector(`.pkt_ext_openpocket`), {
      source: `view_list`
    });
  };

  this.showTagsLocalizedError = function (l10nId) {
    document.querySelector(`.pkt_ext_edit_msg`)?.classList.add(`pkt_ext_edit_msg_error`, `pkt_ext_edit_msg_active`);
    document.l10n.setAttributes(document.querySelector(`.pkt_ext_edit_msg`), l10nId);
    document.querySelector(`.pkt_ext_tag_detail`)?.classList.add(`pkt_ext_tag_error`);
  };

  this.hideTagsError = function () {
    document.querySelector(`.pkt_ext_edit_msg`)?.classList.remove(`pkt_ext_edit_msg_error`, `pkt_ext_edit_msg_active`);
    document.querySelector(`.pkt_ext_edit_msg`).textContent = ``;
    document.querySelector(`pkt_ext_tag_detail`)?.classList.remove(`pkt_ext_tag_error`);
  };

  this.showActiveTags = function () {
    if (!document.querySelector(`.pkt_ext_suggestedtag_detail`)) {
      return;
    }

    let activeTokens = [];
    document.querySelectorAll(`.token-input-token p`).forEach(el => {
      activeTokens.push(el.textContent);
    });
    let elInactiveTags = document.querySelectorAll(`.pkt_ext_suggestedtag_detail .token_tag_inactive`);
    elInactiveTags.forEach(el => {
      if (!activeTokens.includes(el.textContent)) {
        el.classList.remove(`token_tag_inactive`);
      }
    });
  };

  this.hideInactiveTags = function () {
    if (!document.querySelector(`.pkt_ext_suggestedtag_detail`)) {
      return;
    }

    let activeTokens = [];
    document.querySelectorAll(`.token-input-token p`).forEach(el => {
      activeTokens.push(el.textContent);
    });
    let elActiveTags = document.querySelectorAll(`.token_tag:not(.token_tag_inactive)`);
    elActiveTags.forEach(el => {
      if (activeTokens.includes(el.textContent)) {
        el.classList.add(`token_tag_inactive`);
      }
    });
  };

  this.showStateSaved = function (initobj) {
    document.l10n.setAttributes(document.querySelector(".pkt_ext_detail h2"), "pocket-panel-saved-page-saved");
    document.querySelector(`.pkt_ext_btn`).classList.add(`pkt_ext_btn_disabled`);

    if (typeof initobj.item == "object") {
      this.savedItemId = initobj.item.item_id;
      this.savedUrl = initobj.item.given_url;
    }

    document.querySelector(`.pkt_ext_containersaved`).classList.add(`pkt_ext_container_detailactive`);
    document.querySelector(`.pkt_ext_containersaved`).classList.remove(`pkt_ext_container_finalstate`);
    myself.fillUserTags();

    if (!myself.suggestedTagsLoaded) {
      myself.fillSuggestedTags();
    }
  };

  this.renderItemRecs = function (data) {
    if (data?.recommendations?.length) {
      // URL encode and append raw image source for Thumbor + CDN
      data.recommendations = data.recommendations.map(rec => {
        // Using array notation because there is a key titled `1` (`images` is an object)
        let rawSource = rec?.item?.top_image_url || rec?.item?.images["1"]?.src; // Append UTM to rec URLs (leave existing query strings intact)

        if (rec?.item?.resolved_url && !rec.item.resolved_url.match(/\?/)) {
          rec.item.resolved_url = `${rec.item.resolved_url}?utm_source=pocket-ff-recs`;
        }

        rec.item.encodedThumbURL = rawSource ? encodeURIComponent(rawSource) : null;
        return rec;
      }); // This is the ML model used to recommend the story.
      // Right now this value is the same for all three items returned together,
      // so we can just use the first item's value for all.

      const model = data.recommendations[0].experiment;
      const renderedRecs = Handlebars.templates.item_recs(data);
      document.querySelector(`body`).classList.add(`recs_enabled`);
      document.querySelector(`.pkt_ext_subshell`).style.display = `block`;
      document.querySelector(`.pkt_ext_item_recs`).append(myself.parseHTML(renderedRecs));
      messages.clickHelper(document.querySelector(`.pkt_ext_learn_more`), {
        source: `recs_learn_more`
      });
      document.querySelectorAll(`.pkt_ext_item_recs_link`).forEach((el, position) => {
        el.addEventListener(`click`, e => {
          e.preventDefault();
          messages.sendMessage(`PKT_openTabWithPocketUrl`, {
            url: el.getAttribute(`href`),
            model,
            position
          });
        });
      });
    }
  };

  this.sanitizeText = function (s) {
    var sanitizeMap = {
      "&": "&amp;",
      "<": "&lt;",
      ">": "&gt;",
      '"': "&quot;",
      "'": "&#39;"
    };

    if (typeof s !== "string") {
      return "";
    }

    return String(s).replace(/[&<>"']/g, function (str) {
      return sanitizeMap[str];
    });
  };

  this.showStateFinalLocalizedMsg = function (l10nId) {
    document.querySelector(`.pkt_ext_containersaved .pkt_ext_tag_detail`).addEventListener(`transitionend`, () => {
      document.l10n.setAttributes(document.querySelector(`.pkt_ext_containersaved .pkt_ext_detail h2`), l10nId);
    }, {
      once: true
    });
    document.querySelector(`.pkt_ext_containersaved`).classList.add(`pkt_ext_container_finalstate`);
  };

  this.showStateLocalizedError = function (headlineL10nId, detailL10nId) {
    document.l10n.setAttributes(document.querySelector(`.pkt_ext_containersaved .pkt_ext_detail h2`), headlineL10nId);
    document.l10n.setAttributes(document.querySelector(`.pkt_ext_containersaved .pkt_ext_detail h3`), detailL10nId);
    document.querySelector(`.pkt_ext_containersaved`).classList.add(`pkt_ext_container_detailactive`, `pkt_ext_container_finalstate`, `pkt_ext_container_finalerrorstate`);
  };
};

SavedOverlay.prototype = {
  create() {
    if (this.active) {
      return;
    }

    this.active = true;
    var myself = this;
    var url = window.location.href.match(/premiumStatus=([\w|\d|\.]*)&?/);

    if (url && url.length > 1) {
      this.premiumStatus = url[1] == "1";
    }

    var fxasignedin = window.location.href.match(/fxasignedin=([\w|\d|\.]*)&?/);

    if (fxasignedin && fxasignedin.length > 1) {
      this.fxasignedin = fxasignedin[1] == "1";
    }

    var host = window.location.href.match(/pockethost=([\w|\.]*)&?/);

    if (host && host.length > 1) {
      this.pockethost = host[1];
    }

    var locale = window.location.href.match(/locale=([\w|\.]*)&?/);

    if (locale && locale.length > 1) {
      this.locale = locale[1].toLowerCase();
    } // set host


    const templateData = {
      pockethost: this.pockethost
    }; // extra modifier class for language

    if (this.locale) {
      document.querySelector(`body`).classList.add(`pkt_ext_saved_${this.locale}`);
    }

    const parser = new DOMParser(); // Create actual content

    document.querySelector(`body`).append(...parser.parseFromString(Handlebars.templates.saved_shell(templateData), `text/html`).body.childNodes); // Add in premium content (if applicable based on premium status)

    if (this.premiumStatus && !document.querySelector(`.pkt_ext_suggestedtag_detail`)) {
      let elSubshell = document.querySelector(`body .pkt_ext_subshell`);
      let elPremiumShellElements = parser.parseFromString(Handlebars.templates.saved_premiumshell(templateData), `text/html`).body.childNodes; // Convert NodeList to Array and reverse it

      elPremiumShellElements = [].slice.call(elPremiumShellElements).reverse();
      elPremiumShellElements.forEach(el => {
        elSubshell.insertBefore(el, elSubshell.firstChild);
      });
    } // Initialize functionality for overlay


    this.initTagInput();
    this.initAddTagInput();
    this.initRemovePageInput();
    this.initOpenListInput(); // wait confirmation of save before flipping to final saved state

    messages.addMessageListener("PKT_saveLink", function (resp) {
      const {
        data
      } = resp;

      if (data.status == "error") {
        // Fallback to a generic catch all error.
        let errorLocalizedKey = data?.error?.localizedKey || "pocket-panel-saved-error-generic";
        myself.showStateLocalizedError("pocket-panel-saved-error-not-saved", errorLocalizedKey);
        return;
      }

      myself.showStateSaved(data);
    });
    messages.addMessageListener("PKT_renderItemRecs", function (resp) {
      const {
        data
      } = resp;
      myself.renderItemRecs(data);
    }); // tell back end we're ready

    messages.sendMessage("PKT_show_saved");
  }

};
/* harmony default export */ const saved_overlay = (SavedOverlay);
;// CONCATENATED MODULE: ./content/panels/js/style-guide/overlay.js




var StyleGuideOverlay = function (options) {};

StyleGuideOverlay.prototype = {
  create() {
    // TODO: Wrap popular topics component in JSX to work without needing an explicit container hierarchy for styling
    react_dom.render( /*#__PURE__*/react.createElement(components_PopularTopics, {
      pockethost: `getpocket.com`,
      utmsource: `styleguide`,
      topics: [{
        title: "Self Improvement",
        topic: "self-improvement"
      }, {
        title: "Food",
        topic: "food"
      }, {
        title: "Entertainment",
        topic: "entertainment"
      }, {
        title: "Science",
        topic: "science"
      }]
    }), document.querySelector(`#stp_style_guide_components`));
  }

};
/* harmony default export */ const style_guide_overlay = (StyleGuideOverlay);
;// CONCATENATED MODULE: ./content/panels/js/main.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */






var PKT_PANEL = function () {};

PKT_PANEL.prototype = {
  initHome() {
    this.overlay = new overlay();
    this.init();
  },

  initSignup() {
    this.overlay = new signup_overlay();
    this.init();
  },

  initSaved() {
    this.overlay = new saved_overlay();
    this.init();
  },

  initStyleGuide() {
    this.overlay = new style_guide_overlay();
    this.init();
  },

  setupObservers() {
    this.setupMutationObserver(); // Mutation observer isn't always enough for fast loading, static pages.
    // Sometimes the mutation observer fires before the page is totally visible.
    // In this case, the resize tries to fire with 0 height,
    // and because it's a static page, it only does one mutation.
    // So in this case, we have a backup intersection observer that fires when
    // the page is first visible, and thus, the page is going to guarantee a height.

    this.setupIntersectionObserver();
  },

  init() {
    if (this.inited) {
      return;
    }

    this.setupObservers();
    this.inited = true;
  },

  resizeParent() {
    let clientHeight = document.body.clientHeight;

    if (this.overlay.tagsDropdownOpen) {
      clientHeight = Math.max(clientHeight, 252);
    } // We can ignore 0 height here.
    // We rely on intersection observer to do the
    // resize for 0 height loads.


    if (clientHeight) {
      messages.sendMessage("PKT_resizePanel", {
        width: document.body.clientWidth,
        height: clientHeight
      });
    }
  },

  setupIntersectionObserver() {
    const observer = new IntersectionObserver(entries => {
      if (entries.find(e => e.isIntersecting)) {
        this.resizeParent();
        observer.unobserve(document.body);
      }
    });
    observer.observe(document.body);
  },

  setupMutationObserver() {
    // Select the node that will be observed for mutations
    const targetNode = document.body; // Options for the observer (which mutations to observe)

    const config = {
      attributes: false,
      childList: true,
      subtree: true
    }; // Callback function to execute when mutations are observed

    const callback = (mutationList, observer) => {
      mutationList.forEach(mutation => {
        switch (mutation.type) {
          case "childList":
            {
              /* One or more children have been added to and/or removed
                 from the tree.
                 (See mutation.addedNodes and mutation.removedNodes.) */
              this.resizeParent();
              break;
            }
        }
      });
    }; // Create an observer instance linked to the callback function


    const observer = new MutationObserver(callback); // Start observing the target node for configured mutations

    observer.observe(targetNode, config);
  },

  create() {
    this.overlay.create();
  }

};
window.PKT_PANEL = PKT_PANEL;
window.pktPanelMessaging = messages;

/***/ })

/******/ 	});
/************************************************************************/
/******/ 	// The module cache
/******/ 	var __webpack_module_cache__ = {};
/******/ 	
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/ 		// Check if module is in cache
/******/ 		var cachedModule = __webpack_module_cache__[moduleId];
/******/ 		if (cachedModule !== undefined) {
/******/ 			return cachedModule.exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = __webpack_module_cache__[moduleId] = {
/******/ 			// no module.id needed
/******/ 			// no module.loaded needed
/******/ 			exports: {}
/******/ 		};
/******/ 	
/******/ 		// Execute the module function
/******/ 		__webpack_modules__[moduleId](module, module.exports, __webpack_require__);
/******/ 	
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/ 	
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = __webpack_modules__;
/******/ 	
/************************************************************************/
/******/ 	/* webpack/runtime/chunk loaded */
/******/ 	(() => {
/******/ 		var deferred = [];
/******/ 		__webpack_require__.O = (result, chunkIds, fn, priority) => {
/******/ 			if(chunkIds) {
/******/ 				priority = priority || 0;
/******/ 				for(var i = deferred.length; i > 0 && deferred[i - 1][2] > priority; i--) deferred[i] = deferred[i - 1];
/******/ 				deferred[i] = [chunkIds, fn, priority];
/******/ 				return;
/******/ 			}
/******/ 			var notFulfilled = Infinity;
/******/ 			for (var i = 0; i < deferred.length; i++) {
/******/ 				var [chunkIds, fn, priority] = deferred[i];
/******/ 				var fulfilled = true;
/******/ 				for (var j = 0; j < chunkIds.length; j++) {
/******/ 					if ((priority & 1 === 0 || notFulfilled >= priority) && Object.keys(__webpack_require__.O).every((key) => (__webpack_require__.O[key](chunkIds[j])))) {
/******/ 						chunkIds.splice(j--, 1);
/******/ 					} else {
/******/ 						fulfilled = false;
/******/ 						if(priority < notFulfilled) notFulfilled = priority;
/******/ 					}
/******/ 				}
/******/ 				if(fulfilled) {
/******/ 					deferred.splice(i--, 1)
/******/ 					var r = fn();
/******/ 					if (r !== undefined) result = r;
/******/ 				}
/******/ 			}
/******/ 			return result;
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/hasOwnProperty shorthand */
/******/ 	(() => {
/******/ 		__webpack_require__.o = (obj, prop) => (Object.prototype.hasOwnProperty.call(obj, prop))
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/jsonp chunk loading */
/******/ 	(() => {
/******/ 		// no baseURI
/******/ 		
/******/ 		// object to store loaded and loading chunks
/******/ 		// undefined = chunk not loaded, null = chunk preloaded/prefetched
/******/ 		// [resolve, reject, Promise] = chunk loading, 0 = chunk loaded
/******/ 		var installedChunks = {
/******/ 			179: 0
/******/ 		};
/******/ 		
/******/ 		// no chunk on demand loading
/******/ 		
/******/ 		// no prefetching
/******/ 		
/******/ 		// no preloaded
/******/ 		
/******/ 		// no HMR
/******/ 		
/******/ 		// no HMR manifest
/******/ 		
/******/ 		__webpack_require__.O.j = (chunkId) => (installedChunks[chunkId] === 0);
/******/ 		
/******/ 		// install a JSONP callback for chunk loading
/******/ 		var webpackJsonpCallback = (parentChunkLoadingFunction, data) => {
/******/ 			var [chunkIds, moreModules, runtime] = data;
/******/ 			// add "moreModules" to the modules object,
/******/ 			// then flag all "chunkIds" as loaded and fire callback
/******/ 			var moduleId, chunkId, i = 0;
/******/ 			if(chunkIds.some((id) => (installedChunks[id] !== 0))) {
/******/ 				for(moduleId in moreModules) {
/******/ 					if(__webpack_require__.o(moreModules, moduleId)) {
/******/ 						__webpack_require__.m[moduleId] = moreModules[moduleId];
/******/ 					}
/******/ 				}
/******/ 				if(runtime) var result = runtime(__webpack_require__);
/******/ 			}
/******/ 			if(parentChunkLoadingFunction) parentChunkLoadingFunction(data);
/******/ 			for(;i < chunkIds.length; i++) {
/******/ 				chunkId = chunkIds[i];
/******/ 				if(__webpack_require__.o(installedChunks, chunkId) && installedChunks[chunkId]) {
/******/ 					installedChunks[chunkId][0]();
/******/ 				}
/******/ 				installedChunks[chunkIds[i]] = 0;
/******/ 			}
/******/ 			return __webpack_require__.O(result);
/******/ 		}
/******/ 		
/******/ 		var chunkLoadingGlobal = self["webpackChunksave_to_pocket_ff"] = self["webpackChunksave_to_pocket_ff"] || [];
/******/ 		chunkLoadingGlobal.forEach(webpackJsonpCallback.bind(null, 0));
/******/ 		chunkLoadingGlobal.push = webpackJsonpCallback.bind(null, chunkLoadingGlobal.push.bind(chunkLoadingGlobal));
/******/ 	})();
/******/ 	
/************************************************************************/
/******/ 	
/******/ 	// startup
/******/ 	// Load entry module and return exports
/******/ 	// This entry module depends on other loaded chunks and execution need to be delayed
/******/ 	var __webpack_exports__ = __webpack_require__.O(undefined, [736], () => (__webpack_require__(883)))
/******/ 	__webpack_exports__ = __webpack_require__.O(__webpack_exports__);
/******/ 	
/******/ })()
;