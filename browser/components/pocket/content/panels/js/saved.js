/* global $:false, Handlebars:false, thePKT_PANEL:false */
/* import-globals-from messages.js */

/*
PKT_PANEL_OVERLAY is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/

var PKT_PANEL_OVERLAY = function(options) {
  var myself = this;

  this.inited = false;
  this.active = false;
  this.pockethost = "getpocket.com";
  this.savedItemId = 0;
  this.savedUrl = "";
  this.premiumStatus = false;
  this.preventCloseTimerCancel = false;
  this.closeValid = true;
  this.mouseInside = false;
  this.autocloseTimer = null;
  this.dictJSON = {};
  this.autocloseTimerEnabled = false;
  this.autocloseTiming = 4500;
  this.autocloseTimingFinalState = 2000;
  this.mouseInside = false;
  this.userTags = [];
  this.tagsDropdownOpen = false;
  this.cxt_suggested_available = 0;
  this.cxt_entered = 0;
  this.cxt_suggested = 0;
  this.cxt_removed = 0;
  this.justaddedsuggested = false;
  this.fxasignedin = false;
  this.premiumDetailsAdded = false;

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
      this.cxt_suggested_available++;
    }
  };
  this.fillUserTags = function() {
    thePKT_PANEL.sendMessage("PKT_getTags", {}, function(resp) {
      const { data } = resp;
      if (typeof data == "object" && typeof data.tags == "object") {
        myself.userTags = data.tags;
      }
    });
  };
  this.fillSuggestedTags = function() {
    if (!document.querySelector(`.pkt_ext_suggestedtag_detail`)) {
      myself.suggestedTagsLoaded = true;
      myself.startCloseTimer();
      return;
    }

    document.querySelector(`.pkt_ext_subshell`).style.display = `block`;

    thePKT_PANEL.sendMessage(
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
          if (!myself.mouseInside) {
            myself.startCloseTimer();
          }
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
          if (!myself.mouseInside) {
            myself.startCloseTimer();
          }
        }
      }
    );
  };
  this.initAutoCloseEvents = function() {
    document
      .querySelector(`.pkt_ext_containersaved`)
      .addEventListener(`mouseenter`, () => {
        myself.mouseInside = true;
        myself.stopCloseTimer();
      });

    document
      .querySelector(`.pkt_ext_containersaved`)
      .addEventListener(`mouseleave`, () => {
        myself.mouseInside = false;
        myself.startCloseTimer();
      });

    document
      .querySelector(`.pkt_ext_containersaved`)
      .addEventListener(`click`, () => {
        myself.closeValid = false;
      });

    document.querySelector(`body`).addEventListener(`keydown`, e => {
      if (e.key === `Tab`) {
        myself.mouseInside = true;
        myself.stopCloseTimer();
      }
    });
  };
  this.startCloseTimer = function(manualtime) {
    if (!myself.autocloseTimerEnabled) {
      return;
    }

    var settime = manualtime ? manualtime : myself.autocloseTiming;
    if (typeof myself.autocloseTimer == "number") {
      clearTimeout(myself.autocloseTimer);
    }
    myself.autocloseTimer = setTimeout(function() {
      if (myself.closeValid || myself.preventCloseTimerCancel) {
        myself.preventCloseTimerCancel = false;
        myself.closePopup();
      }
    }, settime);
  };
  this.stopCloseTimer = function() {
    if (myself.preventCloseTimerCancel) {
      return;
    }
    clearTimeout(myself.autocloseTimer);
  };
  this.closePopup = function() {
    myself.stopCloseTimer();
    thePKT_PANEL.sendMessage("PKT_close");
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
  // TODO: Remove jQuery from this method once tokenInput is re-written as a React component
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
      textToData(text) {
        if ($.trim(text).length > 25 || !$.trim(text).length) {
          if (text.length > 25) {
            myself.showTagsError(myself.dictJSON.maxtaglength);
            this.changestamp = Date.now();
            setTimeout(function() {
              $(".token-input-input-token input")
                .val(text)
                .focus();
            }, 10);
          }
          return null;
        }
        myself.hideTagsError();
        return { name: myself.sanitizeText(text.toLowerCase()) };
      },
      onReady() {
        $(".token-input-dropdown").addClass("token-input-dropdown-tag");
        inputwrapper
          .find(".token-input-input-token input")
          .attr("placeholder", $(".tag-input").attr("placeholder"))
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
              myself.justaddedsuggested = true;
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
                $(".pkt_ext_containersaved")
                  .find(".pkt_ext_btn")
                  .trigger("click");
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

      document.querySelector(
        `.pkt_ext_containersaved .pkt_ext_detail h2`
      ).textContent = myself.dictJSON.processingtags;

      let originaltags = [];

      document.querySelectorAll(`.token-input-token p`).forEach(el => {
        let text = el.textContent.trim();

        if (text.length) {
          originaltags.push(text);
        }
      });

      thePKT_PANEL.sendMessage(
        "PKT_addTags",
        {
          url: myself.savedUrl,
          tags: originaltags,
        },
        function(resp) {
          const { data } = resp;

          if (data.status == "success") {
            myself.showStateFinalMsg(myself.dictJSON.tagssaved);
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
          document.querySelector(
            `.pkt_ext_containersaved .pkt_ext_detail h2`
          ).textContent = myself.dictJSON.processingremove;

          thePKT_PANEL.sendMessage(
            "PKT_deleteItem",
            {
              itemId: myself.savedItemId,
            },
            function(resp) {
              const { data } = resp;
              if (data.status == "success") {
                myself.showStateFinalMsg(myself.dictJSON.pageremoved);
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
    document
      .querySelector(`.pkt_ext_openpocket`)
      .addEventListener(`click`, e => {
        e.preventDefault();

        thePKT_PANEL.sendMessage("PKT_openTabWithUrl", {
          url: e.target.getAttribute(`href`),
          activate: true,
          source: `view_list`,
        });
      });
  };
  this.showTagsError = function(msg) {
    document
      .querySelector(`.pkt_ext_edit_msg`)
      ?.classList.add(`pkt_ext_edit_msg_error`, `pkt_ext_edit_msg_active`);
    document.querySelector(`.pkt_ext_edit_msg`).textContent = msg;
    document
      .querySelector(`.pkt_ext_tag_detail`)
      ?.classList.add(`pkt_ext_tag_error`);
  };
  this.hideTagsError = function(msg) {
    document
      .querySelector(`.pkt_ext_edit_msg`)
      ?.classList.remove(`pkt_ext_edit_msg_error`, `pkt_ext_edit_msg_active`);
    document.querySelector(`.pkt_ext_edit_msg`).textContent = ``;
    document
      .querySelector(`pkt_ext_tag_detail`)
      ?.classList.remove(`pkt_ext_tag_error`);
  };
  this.showActiveTags = function() {
    if (!$(".pkt_ext_suggestedtag_detail").length) {
      return;
    }
    var activetokenstext = "|";
    $(".token-input-token").each(function(index, element) {
      activetokenstext +=
        $(element)
          .find("p")
          .text() + "|";
    });

    var inactivetags = $(".pkt_ext_suggestedtag_detail").find(
      ".token_tag_inactive"
    );
    inactivetags.each(function(index, element) {
      if (!activetokenstext.includes("|" + $(element).text() + "|")) {
        $(element).removeClass("token_tag_inactive");
      }
    });
  };
  this.hideInactiveTags = function() {
    if (!$(".pkt_ext_suggestedtag_detail").length) {
      return;
    }
    var activetokenstext = "|";
    $(".token-input-token").each(function(index, element) {
      activetokenstext +=
        $(element)
          .find("p")
          .text() + "|";
    });
    var activesuggestedtags = $(".token_tag").not(".token_tag_inactive");
    activesuggestedtags.each(function(index, element) {
      if (activetokenstext.indexOf("|" + $(element).text() + "|") > -1) {
        $(element).addClass("token_tag_inactive");
      }
    });
  };
  this.showStateSaved = function(initobj) {
    document.querySelector(
      `.pkt_ext_detail h2`
    ).textContent = this.dictJSON.pagesaved;
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

    if (myself.suggestedTagsLoaded) {
      myself.startCloseTimer();
    } else {
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
      $("body").addClass("recs_enabled");
      $(".pkt_ext_subshell").show();
      $(".pkt_ext_item_recs").append(renderedRecs);
      $(".pkt_ext_learn_more").click(function(e) {
        e.preventDefault();
        thePKT_PANEL.sendMessage("PKT_openTabWithUrl", {
          url: $(this).attr("href"),
          activate: true,
          source: "recs_learn_more",
        });
      });
      $(".pkt_ext_item_recs_link").click(function(e) {
        e.preventDefault();
        const url = $(this).attr("href");
        const position = $(".pkt_ext_item_recs_link").index(this);
        thePKT_PANEL.sendMessage("PKT_openTabWithPocketUrl", {
          url,
          model,
          position,
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
  this.showStateFinalMsg = function(msg) {
    $(".pkt_ext_containersaved")
      .find(".pkt_ext_tag_detail")
      .one(
        "webkitTransitionEnd transitionend msTransitionEnd oTransitionEnd",
        function(e) {
          $(this).off(
            "webkitTransitionEnd transitionend msTransitionEnd oTransitionEnd"
          );
          myself.preventCloseTimerCancel = true;
          myself.startCloseTimer(myself.autocloseTimingFinalState);
          $(".pkt_ext_containersaved")
            .find(".pkt_ext_detail h2")
            .text(msg);
        }
      );
    $(".pkt_ext_containersaved").addClass("pkt_ext_container_finalstate");
  };
  this.showStateError = function(headline, detail) {
    document
      .querySelector(`.pkt_ext_containersaved .pkt_ext_detail h2`)
      .textContent(headline);

    document
      .querySelector(`.pkt_ext_containersaved .pkt_ext_detail h3`)
      .textContent(detail);

    document
      .querySelector(`.pkt_ext_containersaved`)
      .classList.add(
        `pkt_ext_container_detailactive`,
        `pkt_ext_container_finalstate`,
        `pkt_ext_container_finalerrorstate`
      );

    this.preventCloseTimerCancel = true;
    this.startCloseTimer(myself.autocloseTimingFinalState);
  };
  this.getTranslations = function() {
    this.dictJSON = window.pocketStrings;
  };
};

PKT_PANEL_OVERLAY.prototype = {
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

    // set translations
    this.getTranslations();

    // set host
    this.dictJSON.pockethost = this.pockethost;

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
        parser.parseFromString(
          Handlebars.templates.saved_shell(this.dictJSON),
          `text/html`
        ).documentElement
      );

    // Add in premium content (if applicable based on premium status)
    if (
      this.premiumStatus &&
      !document.querySelector(`.pkt_ext_suggestedtag_detail`)
    ) {
      this.premiumDetailsAdded = true;

      let elSubshell = document.querySelector(`body .pkt_ext_subshell`);

      let elPremiumShell = parser.parseFromString(
        Handlebars.templates.saved_premiumshell(this.dictJSON),
        `text/html`
      ).documentElement;

      elSubshell.insertBefore(elPremiumShell, elSubshell.firstChild);
    }

    // Initialize functionality for overlay
    this.initTagInput();
    this.initAddTagInput();
    this.initRemovePageInput();
    this.initOpenListInput();
    this.initAutoCloseEvents();

    // wait confirmation of save before flipping to final saved state
    thePKT_PANEL.addMessageListener("PKT_saveLink", function(resp) {
      const { data } = resp;
      if (data.status == "error") {
        if (typeof data.error == "object") {
          if (data.error.localizedKey) {
            myself.showStateError(
              myself.dictJSON.pagenotsaved,
              myself.dictJSON[data.error.localizedKey]
            );
          } else {
            myself.showStateError(
              myself.dictJSON.pagenotsaved,
              data.error.message
            );
          }
        } else {
          myself.showStateError(
            myself.dictJSON.pagenotsaved,
            myself.dictJSON.errorgeneric
          );
        }
        return;
      }

      myself.showStateSaved(data);
    });

    thePKT_PANEL.addMessageListener("PKT_renderItemRecs", function(resp) {
      const { data } = resp;
      myself.renderItemRecs(data);
    });

    // tell back end we're ready
    thePKT_PANEL.sendMessage("PKT_show_saved");
  },
};
