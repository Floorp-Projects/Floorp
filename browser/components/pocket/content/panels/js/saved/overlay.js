/* global Handlebars:false */

/*
SavedOverlay is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/

import pktPanelMessaging from "../messages.js";

var SavedOverlay = function(options) {
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

  this.parseHTML = function(htmlString) {
    const parser = new DOMParser();
    return parser.parseFromString(htmlString, `text/html`).documentElement;
  };
  this.fillTagContainer = function(tags, container, tagclass) {
    while (container.firstChild) {
      container.firstChild.remove();
    }

    for (let i = 0; i < tags.length; i++) {
      let newtag = this.parseHTML(
        `<li class="${tagclass}"><a href="#" class="token_tag">${tags[i]}</a></li>`
      );
      container.append(newtag);
    }
  };
  this.fillUserTags = function() {
    pktPanelMessaging.sendMessage("PKT_getTags", {}, function(resp) {
      const { data } = resp;
      if (typeof data == "object" && typeof data.tags == "object") {
        myself.userTags = data.tags;
      }
    });
  };
  this.fillSuggestedTags = function() {
    if (!document.querySelector(`.pkt_ext_suggestedtag_detail`)) {
      myself.suggestedTagsLoaded = true;
      return;
    }

    document.querySelector(`.pkt_ext_subshell`).style.display = `block`;

    pktPanelMessaging.sendMessage(
      "PKT_getSuggestedTags",
      {
        url: myself.savedUrl,
      },
      function(resp) {
        const { data } = resp;
        document
          .querySelector(`.pkt_ext_suggestedtag_detail`)
          .classList.remove(`pkt_ext_suggestedtag_detail_loading`);
        if (data.status == "success") {
          var newtags = [];
          for (let i = 0; i < data.value.suggestedTags.length; i++) {
            newtags.push(data.value.suggestedTags[i].tag);
          }
          myself.suggestedTagsLoaded = true;
          myself.fillTagContainer(
            newtags,
            document.querySelector(`.pkt_ext_suggestedtag_detail ul`),
            "token_suggestedtag"
          );
        } else if (data.status == "error") {
          let elMsg = myself.parseHTML(
            `<p class="suggestedtag_msg">${data.error.message}</p>`
          );
          document.querySelector(`.pkt_ext_suggestedtag_detail`).append(elMsg);
          this.suggestedTagsLoaded = true;
        }
      }
    );
  };
  this.closePopup = function() {
    pktPanelMessaging.sendMessage("PKT_close");
  };
  this.checkValidTagSubmit = function() {
    let inputlength = document
      .querySelector(
        `.pkt_ext_tag_input_wrapper .token-input-input-token input`
      )
      .value.trim().length;

    if (
      document.querySelector(`.pkt_ext_containersaved .token-input-token`) ||
      (inputlength > 0 && inputlength < 26)
    ) {
      document
        .querySelector(`.pkt_ext_containersaved .pkt_ext_btn`)
        .classList.remove(`pkt_ext_btn_disabled`);
    } else {
      document
        .querySelector(`.pkt_ext_containersaved .pkt_ext_btn`)
        .classList.add(`pkt_ext_btn_disabled`);
    }

    myself.updateSlidingTagList();
  };
  this.updateSlidingTagList = function() {
    let cssDir = document.dir === `ltr` ? `left` : `right`;
    let offsetDir = document.dir === `ltr` ? `offsetLeft` : `offsetRight`;
    let elTokenInputList = document.querySelector(`.token-input-list`);
    let inputleft = document.querySelector(`.token-input-input-token input`)[
      offsetDir
    ];
    let listleft = elTokenInputList[offsetDir];
    let listleftnatural =
      listleft - parseInt(getComputedStyle(elTokenInputList)[cssDir]);
    let leftwidth = document.querySelector(`.pkt_ext_tag_input_wrapper`)
      .offsetWidth;

    if (inputleft + listleft + 20 > leftwidth) {
      elTokenInputList.style[cssDir] = `${Math.min(
        (inputleft + listleftnatural - leftwidth + 20) * -1,
        0
      )}px`;
    } else {
      elTokenInputList.style[cssDir] = 0;
    }
  };
  this.checkPlaceholderStatus = function() {
    if (
      document.querySelector(
        `.pkt_ext_containersaved .pkt_ext_tag_input_wrapper .token-input-token`
      )
    ) {
      document
        .querySelector(`.pkt_ext_containersaved .token-input-input-token input`)
        .setAttribute(`placeholder`, ``);
    } else {
      let elTokenInput = document.querySelector(
        `.pkt_ext_containersaved .token-input-input-token input`
      );

      elTokenInput.setAttribute(
        `placeholder`,
        document.querySelector(`.pkt_ext_tag_input`).getAttribute(`placeholder`)
      );
      elTokenInput.style.width = `200px`;
    }
  };
  // TODO: Remove jQuery and re-enable eslint for this method once tokenInput is re-written as a React component
  /* eslint-disable no-undef */
  this.initTagInput = function() {
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
              returnlist.push({ name: myself.userTags[i] });
              limit--;
            }
          }
        }
        if (!$(".token-input-dropdown-tag").data("init")) {
          $(".token-input-dropdown-tag")
            .css("width", inputwrapper.outerWidth())
            .data("init");
          inputwrapper.append($(".token-input-dropdown-tag"));
        }
        cb(returnlist);
      },
      validateItem(item) {
        const text = item.name;
        if ($.trim(text).length > 25 || !$.trim(text).length) {
          if (text.length > 25) {
            myself.showTagsLocalizedError(
              "pocket-panel-saved-error-tag-length"
            );
            this.changestamp = Date.now();
            setTimeout(function() {
              $(".token-input-input-token input")
                .val(text)
                .focus();
            }, 10);
          }
          return false;
        }

        myself.hideTagsError();
        return true;
      },
      textToData(text) {
        return { name: myself.sanitizeText(text.toLowerCase()) };
      },
      onReady() {
        $(".token-input-dropdown").addClass("token-input-dropdown-tag");
        inputwrapper
          .find(".token-input-input-token input")
          // The token input does so element copy magic, but doesn't copy over l10n ids.
          // So we do it manually here.
          .attr(
            "data-l10n-id",
            inputwrapper.find(".pkt_ext_tag_input").attr("data-l10n-id")
          )
          .css("width", "200px");
        if ($(".pkt_ext_suggestedtag_detail").length) {
          $(".pkt_ext_containersaved")
            .find(".pkt_ext_suggestedtag_detail")
            .on("click", ".token_tag", function(e) {
              e.preventDefault();
              var tag = $(e.target);
              if (
                $(this).parents(".pkt_ext_suggestedtag_detail_disabled").length
              ) {
                return;
              }
              inputwrapper.find(".pkt_ext_tag_input").tokenInput("add", {
                id: inputwrapper.find(".token-input-token").length,
                name: tag.text(),
              });
              tag.addClass("token-suggestedtag-inactive");
              $(".token-input-input-token input").focus();
            });
        }
        $(".token-input-list")
          .on("keydown", "input", function(e) {
            if (e.which == 37) {
              myself.updateSlidingTagList();
            }
            if (e.which === 9) {
              $("a.pkt_ext_learn_more").focus();
            }
          })
          .on("keypress", "input", function(e) {
            if (e.which == 13) {
              if (
                typeof this.changestamp == "undefined" ||
                Date.now() - this.changestamp > 250
              ) {
                e.preventDefault();
                document
                  .querySelector(`.pkt_ext_containersaved .pkt_ext_btn`)
                  .click();
              }
            }
          })
          .on("keyup", "input", function(e) {
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
      },
    });
    $("body").on("keydown", function(e) {
      var key = e.keyCode || e.which;
      if (key == 8) {
        var selected = $(".token-input-selected-token");
        if (selected.length) {
          e.preventDefault();
          e.stopImmediatePropagation();
          inputwrapper
            .find(".pkt_ext_tag_input")
            .tokenInput("remove", { name: selected.find("p").text() });
        }
      } else if (
        $(e.target)
          .parent()
          .hasClass("token-input-input-token")
      ) {
        e.stopImmediatePropagation();
      }
    });
  };
  /* eslint-enable no-undef */
  this.disableInput = function() {
    document
      .querySelector(`.pkt_ext_containersaved .pkt_ext_item_actions`)
      .classList.add("pkt_ext_item_actions_disabled");
    document
      .querySelector(`.pkt_ext_containersaved .pkt_ext_btn`)
      .classList.add("pkt_ext_btn_disabled");
    document
      .querySelector(`.pkt_ext_containersaved .pkt_ext_tag_input_wrapper`)
      .classList.add("pkt_ext_tag_input_wrapper_disabled");
    if (
      document.querySelector(
        `.pkt_ext_containersaved .pkt_ext_suggestedtag_detail`
      )
    ) {
      document
        .querySelector(`.pkt_ext_containersaved .pkt_ext_suggestedtag_detail`)
        .classList.add("pkt_ext_suggestedtag_detail_disabled");
    }
  };
  this.enableInput = function() {
    document
      .querySelector(`.pkt_ext_containersaved .pkt_ext_item_actions`)
      .classList.remove("pkt_ext_item_actions_disabled");
    this.checkValidTagSubmit();
    document
      .querySelector(`.pkt_ext_containersaved .pkt_ext_tag_input_wrapper`)
      .classList.remove("pkt_ext_tag_input_wrapper_disabled");
    if (
      document.querySelector(
        `.pkt_ext_containersaved .pkt_ext_suggestedtag_detail`
      )
    ) {
      document
        .querySelector(`.pkt_ext_containersaved .pkt_ext_suggestedtag_detail`)
        .classList.remove("pkt_ext_suggestedtag_detail_disabled");
    }
  };
  this.initAddTagInput = function() {
    document.querySelector(`.pkt_ext_btn`).addEventListener(`click`, e => {
      e.preventDefault();

      if (
        e.target.classList.contains(`pkt_ext_btn_disabled`) ||
        document.querySelector(
          `.pkt_ext_edit_msg_active.pkt_ext_edit_msg_error`
        )
      ) {
        return;
      }

      myself.disableInput();

      document.l10n.setAttributes(
        document.querySelector(`.pkt_ext_containersaved .pkt_ext_detail h2`),
        "pocket-panel-saved-processing-tags"
      );

      let originaltags = [];

      document.querySelectorAll(`.token-input-token p`).forEach(el => {
        let text = el.textContent.trim();

        if (text.length) {
          originaltags.push(text);
        }
      });

      pktPanelMessaging.sendMessage(
        "PKT_addTags",
        {
          url: myself.savedUrl,
          tags: originaltags,
        },
        function(resp) {
          const { data } = resp;

          if (data.status == "success") {
            myself.showStateFinalLocalizedMsg("pocket-panel-saved-tags-saved");
          } else if (data.status == "error") {
            let elEditMsg = document.querySelector(`.pkt_ext_edit_msg`);

            elEditMsg.classList.add(
              `pkt_ext_edit_msg_error`,
              `pkt_ext_edit_msg_active`
            );
            elEditMsg.textContent = data.error.message;
          }
        }
      );
    });
  };
  this.initRemovePageInput = function() {
    document
      .querySelector(`.pkt_ext_removeitem`)
      .addEventListener(`click`, e => {
        document.querySelector(`.pkt_ext_subshell`).style.display = `none`;

        if (e.target.closest(`.pkt_ext_item_actions_disabled`)) {
          e.preventDefault();
          return;
        }

        if (e.target.classList.contains(`pkt_ext_removeitem`)) {
          e.preventDefault();
          myself.disableInput();

          document.l10n.setAttributes(
            document.querySelector(
              `.pkt_ext_containersaved .pkt_ext_detail h2`
            ),
            "pocket-panel-saved-processing-remove"
          );

          pktPanelMessaging.sendMessage(
            "PKT_deleteItem",
            {
              itemId: myself.savedItemId,
            },
            function(resp) {
              const { data } = resp;
              if (data.status == "success") {
                myself.showStateFinalLocalizedMsg(
                  "pocket-panel-saved-page-removed"
                );
              } else if (data.status == "error") {
                let elEditMsg = document.querySelector(`.pkt_ext_edit_msg`);

                elEditMsg.classList.add(
                  `pkt_ext_edit_msg_error`,
                  `pkt_ext_edit_msg_active`
                );
                elEditMsg.textContent = data.error.message;
              }
            }
          );
        }
      });
  };
  this.initOpenListInput = function() {
    pktPanelMessaging.clickHelper(
      document.querySelector(`.pkt_ext_openpocket`),
      {
        source: `view_list`,
      }
    );
  };

  this.showTagsLocalizedError = function(l10nId) {
    document
      .querySelector(`.pkt_ext_edit_msg`)
      ?.classList.add(`pkt_ext_edit_msg_error`, `pkt_ext_edit_msg_active`);
    document.l10n.setAttributes(
      document.querySelector(`.pkt_ext_edit_msg`),
      l10nId
    );
    document
      .querySelector(`.pkt_ext_tag_detail`)
      ?.classList.add(`pkt_ext_tag_error`);
  };
  this.hideTagsError = function() {
    document
      .querySelector(`.pkt_ext_edit_msg`)
      ?.classList.remove(`pkt_ext_edit_msg_error`, `pkt_ext_edit_msg_active`);
    document.querySelector(`.pkt_ext_edit_msg`).textContent = ``;
    document
      .querySelector(`pkt_ext_tag_detail`)
      ?.classList.remove(`pkt_ext_tag_error`);
  };
  this.showActiveTags = function() {
    if (!document.querySelector(`.pkt_ext_suggestedtag_detail`)) {
      return;
    }

    let activeTokens = [];

    document.querySelectorAll(`.token-input-token p`).forEach(el => {
      activeTokens.push(el.textContent);
    });

    let elInactiveTags = document.querySelectorAll(
      `.pkt_ext_suggestedtag_detail .token_tag_inactive`
    );

    elInactiveTags.forEach(el => {
      if (!activeTokens.includes(el.textContent)) {
        el.classList.remove(`token_tag_inactive`);
      }
    });
  };
  this.hideInactiveTags = function() {
    if (!document.querySelector(`.pkt_ext_suggestedtag_detail`)) {
      return;
    }

    let activeTokens = [];

    document.querySelectorAll(`.token-input-token p`).forEach(el => {
      activeTokens.push(el.textContent);
    });

    let elActiveTags = document.querySelectorAll(
      `.token_tag:not(.token_tag_inactive)`
    );

    elActiveTags.forEach(el => {
      if (activeTokens.includes(el.textContent)) {
        el.classList.add(`token_tag_inactive`);
      }
    });
  };
  this.showStateSaved = function(initobj) {
    document.l10n.setAttributes(
      document.querySelector(".pkt_ext_detail h2"),
      "pocket-panel-saved-page-saved"
    );
    document
      .querySelector(`.pkt_ext_btn`)
      .classList.add(`pkt_ext_btn_disabled`);
    if (typeof initobj.item == "object") {
      this.savedItemId = initobj.item.item_id;
      this.savedUrl = initobj.item.given_url;
    }

    document
      .querySelector(`.pkt_ext_containersaved`)
      .classList.add(`pkt_ext_container_detailactive`);
    document
      .querySelector(`.pkt_ext_containersaved`)
      .classList.remove(`pkt_ext_container_finalstate`);

    myself.fillUserTags();

    if (!myself.suggestedTagsLoaded) {
      myself.fillSuggestedTags();
    }
  };
  this.renderItemRecs = function(data) {
    if (data?.recommendations?.length) {
      // URL encode and append raw image source for Thumbor + CDN
      data.recommendations = data.recommendations.map(rec => {
        // Using array notation because there is a key titled `1` (`images` is an object)
        let rawSource = rec?.item?.top_image_url || rec?.item?.images["1"]?.src;

        // Append UTM to rec URLs (leave existing query strings intact)
        if (rec?.item?.resolved_url && !rec.item.resolved_url.match(/\?/)) {
          rec.item.resolved_url = `${rec.item.resolved_url}?utm_source=pocket-ff-recs`;
        }

        rec.item.encodedThumbURL = rawSource
          ? encodeURIComponent(rawSource)
          : null;

        return rec;
      });

      // This is the ML model used to recommend the story.
      // Right now this value is the same for all three items returned together,
      // so we can just use the first item's value for all.
      const model = data.recommendations[0].experiment;
      const renderedRecs = Handlebars.templates.item_recs(data);

      document.querySelector(`body`).classList.add(`recs_enabled`);
      document.querySelector(`.pkt_ext_subshell`).style.display = `block`;

      document
        .querySelector(`.pkt_ext_item_recs`)
        .append(myself.parseHTML(renderedRecs));

      pktPanelMessaging.clickHelper(
        document.querySelector(`.pkt_ext_learn_more`),
        {
          source: `recs_learn_more`,
        }
      );

      document
        .querySelectorAll(`.pkt_ext_item_recs_link`)
        .forEach((el, position) => {
          el.addEventListener(`click`, e => {
            e.preventDefault();

            pktPanelMessaging.sendMessage(`PKT_openTabWithPocketUrl`, {
              url: el.getAttribute(`href`),
              model,
              position,
            });
          });
        });
    }
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
  this.showStateFinalLocalizedMsg = function(l10nId) {
    document
      .querySelector(`.pkt_ext_containersaved .pkt_ext_tag_detail`)
      .addEventListener(
        `transitionend`,
        () => {
          document.l10n.setAttributes(
            document.querySelector(
              `.pkt_ext_containersaved .pkt_ext_detail h2`
            ),
            l10nId
          );
        },
        {
          once: true,
        }
      );

    document
      .querySelector(`.pkt_ext_containersaved`)
      .classList.add(`pkt_ext_container_finalstate`);
  };
  this.showStateLocalizedError = function(headlineL10nId, detailL10nId) {
    document.l10n.setAttributes(
      document.querySelector(`.pkt_ext_containersaved .pkt_ext_detail h2`),
      headlineL10nId
    );

    document.l10n.setAttributes(
      document.querySelector(`.pkt_ext_containersaved .pkt_ext_detail h3`),
      detailL10nId
    );

    document
      .querySelector(`.pkt_ext_containersaved`)
      .classList.add(
        `pkt_ext_container_detailactive`,
        `pkt_ext_container_finalstate`,
        `pkt_ext_container_finalerrorstate`
      );
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
    }

    // set host
    const templateData = {
      pockethost: this.pockethost,
    };

    // extra modifier class for language
    if (this.locale) {
      document
        .querySelector(`body`)
        .classList.add(`pkt_ext_saved_${this.locale}`);
    }

    const parser = new DOMParser();

    // Create actual content
    document
      .querySelector(`body`)
      .append(
        ...parser.parseFromString(
          Handlebars.templates.saved_shell(templateData),
          `text/html`
        ).body.childNodes
      );

    // Add in premium content (if applicable based on premium status)
    if (
      this.premiumStatus &&
      !document.querySelector(`.pkt_ext_suggestedtag_detail`)
    ) {
      let elSubshell = document.querySelector(`body .pkt_ext_subshell`);

      let elPremiumShellElements = parser.parseFromString(
        Handlebars.templates.saved_premiumshell(templateData),
        `text/html`
      ).body.childNodes;

      // Convert NodeList to Array and reverse it
      elPremiumShellElements = [].slice.call(elPremiumShellElements).reverse();

      elPremiumShellElements.forEach(el => {
        elSubshell.insertBefore(el, elSubshell.firstChild);
      });
    }

    // Initialize functionality for overlay
    this.initTagInput();
    this.initAddTagInput();
    this.initRemovePageInput();
    this.initOpenListInput();

    // wait confirmation of save before flipping to final saved state
    pktPanelMessaging.addMessageListener("PKT_saveLink", function(resp) {
      const { data } = resp;
      if (data.status == "error") {
        // Fallback to a generic catch all error.
        let errorLocalizedKey =
          data?.error?.localizedKey || "pocket-panel-saved-error-generic";
        myself.showStateLocalizedError(
          "pocket-panel-saved-error-not-saved",
          errorLocalizedKey
        );
        return;
      }

      myself.showStateSaved(data);
    });

    pktPanelMessaging.addMessageListener("PKT_renderItemRecs", function(resp) {
      const { data } = resp;
      myself.renderItemRecs(data);
    });

    // tell back end we're ready
    pktPanelMessaging.sendMessage("PKT_show_saved");
  },
};

export default SavedOverlay;
