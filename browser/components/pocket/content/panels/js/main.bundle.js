/******/ (() => { // webpackBootstrap
/******/ 	"use strict";
/******/ 	var __webpack_modules__ = ({

/***/ 299:
/***/ ((__unused_webpack_module, __unused_webpack___webpack_exports__, __webpack_require__) => {


// EXTERNAL MODULE: ./node_modules/react/index.js
var react = __webpack_require__(294);
// EXTERNAL MODULE: ./node_modules/react-dom/index.js
var react_dom = __webpack_require__(935);
;// CONCATENATED MODULE: ./content/panels/js/components/Header/Header.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


function Header(props) {
  return /*#__PURE__*/react.createElement("h1", {
    className: "stp_header"
  }, /*#__PURE__*/react.createElement("div", {
    className: "stp_header_logo"
  }), props.children);
}

/* harmony default export */ const Header_Header = (Header);
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
;// CONCATENATED MODULE: ./content/panels/js/components/TelemetryLink/TelemetryLink.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



function TelemetryLink(props) {
  function onClick(event) {
    if (props.onClick) {
      props.onClick(event);
    } else {
      event.preventDefault();
      messages.sendMessage("PKT_openTabWithUrl", {
        url: event.currentTarget.getAttribute(`href`),
        source: props.source,
        model: props.model,
        position: props.position
      });
    }
  }

  return /*#__PURE__*/react.createElement("a", {
    href: props.href,
    onClick: onClick,
    target: "_blank",
    className: props.className
  }, props.children);
}

/* harmony default export */ const TelemetryLink_TelemetryLink = (TelemetryLink);
;// CONCATENATED MODULE: ./content/panels/js/components/ArticleList/ArticleList.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



function ArticleUrl(props) {
  // We turn off the link if we're either a saved article, or if the url doesn't exist.
  if (props.savedArticle || !props.url) {
    return /*#__PURE__*/react.createElement("div", {
      className: "stp_article_list_saved_article"
    }, props.children);
  }

  return /*#__PURE__*/react.createElement(TelemetryLink_TelemetryLink, {
    className: "stp_article_list_link",
    href: props.url,
    source: props.source,
    position: props.position,
    model: props.model
  }, props.children);
}

function Article(props) {
  function encodeThumbnail(rawSource) {
    return rawSource ? `https://img-getpocket.cdn.mozilla.net/80x80/filters:format(jpeg):quality(60):no_upscale():strip_exif()/${encodeURIComponent(rawSource)}` : null;
  }

  const [thumbnailLoaded, setThumbnailLoaded] = (0,react.useState)(false);
  const [thumbnailLoadFailed, setThumbnailLoadFailed] = (0,react.useState)(false);
  const {
    article,
    savedArticle,
    position,
    source,
    model,
    utmParams,
    openInPocketReader
  } = props;

  if (!article.url && !article.resolved_url && !article.given_url) {
    return null;
  }

  const url = new URL(article.url || article.resolved_url || article.given_url);
  const urlSearchParams = new URLSearchParams(utmParams);

  if (openInPocketReader && article.item_id && !url.href.match(/getpocket\.com\/read/)) {
    url.href = `https://getpocket.com/read/${article.item_id}`;
  }

  for (let [key, val] of urlSearchParams.entries()) {
    url.searchParams.set(key, val);
  } // Using array notation because there is a key titled `1` (`images` is an object)


  const thumbnail = article.thumbnail || encodeThumbnail(article?.top_image_url || article?.images?.["1"]?.src);
  const alt = article.alt || "thumbnail image";
  const title = article.title || article.resolved_title || article.given_title; // Sometimes domain_metadata is not there, depending on the source.

  const publisher = article.publisher || article.domain_metadata?.name || article.resolved_domain;
  return /*#__PURE__*/react.createElement("li", {
    className: "stp_article_list_item"
  }, /*#__PURE__*/react.createElement(ArticleUrl, {
    url: url.href,
    savedArticle: savedArticle,
    position: position,
    source: source,
    model: model,
    utmParams: utmParams
  }, /*#__PURE__*/react.createElement(react.Fragment, null, thumbnail && !thumbnailLoadFailed ? /*#__PURE__*/react.createElement("img", {
    className: "stp_article_list_thumb",
    src: thumbnail,
    alt: alt,
    width: "40",
    height: "40",
    onLoad: () => {
      setThumbnailLoaded(true);
    },
    onError: () => {
      setThumbnailLoadFailed(true);
    },
    style: {
      visibility: thumbnailLoaded ? `visible` : `hidden`
    }
  }) : /*#__PURE__*/react.createElement("div", {
    className: "stp_article_list_thumb_placeholder"
  }), /*#__PURE__*/react.createElement("div", {
    className: "stp_article_list_meta"
  }, /*#__PURE__*/react.createElement("header", {
    className: "stp_article_list_header"
  }, title), /*#__PURE__*/react.createElement("p", {
    className: "stp_article_list_publisher"
  }, publisher)))));
}

function ArticleList(props) {
  return /*#__PURE__*/react.createElement("ul", {
    className: "stp_article_list"
  }, props.articles?.map((article, position) => /*#__PURE__*/react.createElement(Article, {
    article: article,
    savedArticle: props.savedArticle,
    position: position,
    source: props.source,
    model: props.model,
    utmParams: props.utmParams,
    openInPocketReader: props.openInPocketReader
  })));
}

/* harmony default export */ const ArticleList_ArticleList = (ArticleList);
;// CONCATENATED MODULE: ./content/panels/js/components/PopularTopics/PopularTopics.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



function PopularTopics(props) {
  return /*#__PURE__*/react.createElement("ul", {
    className: "stp_popular_topics"
  }, props.topics?.map((topic, position) => /*#__PURE__*/react.createElement("li", {
    key: `item-${topic.topic}`,
    className: "stp_popular_topic"
  }, /*#__PURE__*/react.createElement(TelemetryLink_TelemetryLink, {
    className: "stp_popular_topic_link",
    href: `https://${props.pockethost}/explore/${topic.topic}?${props.utmParams}`,
    source: props.source,
    position: position
  }, topic.title))));
}

/* harmony default export */ const PopularTopics_PopularTopics = (PopularTopics);
;// CONCATENATED MODULE: ./content/panels/js/components/Button/Button.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



function Button(props) {
  return /*#__PURE__*/react.createElement(TelemetryLink_TelemetryLink, {
    href: props.url,
    onClick: props.onClick,
    className: `stp_button${props?.style && ` stp_button_${props.style}`}`,
    source: props.source
  }, props.children);
}

/* harmony default export */ const Button_Button = (Button);
;// CONCATENATED MODULE: ./content/panels/js/components/Home/Home.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */







function Home(props) {
  const {
    locale,
    topics,
    pockethost,
    hideRecentSaves,
    utmSource,
    utmCampaign,
    utmContent
  } = props;
  const [{
    articles,
    status
  }, setArticlesState] = (0,react.useState)({
    articles: [],
    // Can be success, loading, or error.
    status: ""
  });
  const utmParams = `utm_source=${utmSource}${utmCampaign && utmContent ? `&utm_campaign=${utmCampaign}&utm_content=${utmContent}` : ``}`;
  (0,react.useEffect)(() => {
    if (!hideRecentSaves) {
      // We don't display the loading message until instructed. This is because cache
      // loads should be fast, so using the loading message for cache just adds loading jank.
      messages.addMessageListener("PKT_loadingRecentSaves", function (resp) {
        setArticlesState({
          articles,
          status: "loading"
        });
      });
      messages.addMessageListener("PKT_renderRecentSaves", function (resp) {
        const {
          data
        } = resp;

        if (data.status === "error") {
          setArticlesState({
            articles: [],
            status: "error"
          });
          return;
        }

        setArticlesState({
          articles: data,
          status: "success"
        });
      });
    } // tell back end we're ready


    messages.sendMessage("PKT_show_home");
  }, []);
  let recentSavesSection = null;

  if (status === "error" || hideRecentSaves) {
    recentSavesSection = /*#__PURE__*/react.createElement("h3", {
      className: "header_medium",
      "data-l10n-id": "pocket-panel-home-new-user-cta"
    });
  } else if (status === "loading") {
    recentSavesSection = /*#__PURE__*/react.createElement("span", {
      "data-l10n-id": "pocket-panel-home-most-recent-saves-loading"
    });
  } else if (status === "success") {
    if (articles?.length) {
      recentSavesSection = /*#__PURE__*/react.createElement(react.Fragment, null, /*#__PURE__*/react.createElement("h3", {
        className: "header_medium",
        "data-l10n-id": "pocket-panel-home-most-recent-saves"
      }), articles.length > 3 ? /*#__PURE__*/react.createElement(react.Fragment, null, /*#__PURE__*/react.createElement(ArticleList_ArticleList, {
        articles: articles.slice(0, 3),
        source: "home_recent_save",
        utmParams: utmParams,
        openInPocketReader: true
      }), /*#__PURE__*/react.createElement("span", {
        className: "stp_button_wide"
      }, /*#__PURE__*/react.createElement(Button_Button, {
        style: "secondary",
        url: `https://${pockethost}/a?${utmParams}`,
        source: "home_view_list"
      }, /*#__PURE__*/react.createElement("span", {
        "data-l10n-id": "pocket-panel-button-show-all"
      })))) : /*#__PURE__*/react.createElement(ArticleList_ArticleList, {
        articles: articles,
        source: "home_recent_save",
        utmParams: utmParams
      }));
    } else {
      recentSavesSection = /*#__PURE__*/react.createElement(react.Fragment, null, /*#__PURE__*/react.createElement("h3", {
        className: "header_medium",
        "data-l10n-id": "pocket-panel-home-new-user-cta"
      }), /*#__PURE__*/react.createElement("h3", {
        className: "header_medium",
        "data-l10n-id": "pocket-panel-home-new-user-message"
      }));
    }
  }

  return /*#__PURE__*/react.createElement("div", {
    className: "stp_panel_container"
  }, /*#__PURE__*/react.createElement("div", {
    className: "stp_panel stp_panel_home"
  }, /*#__PURE__*/react.createElement(Header_Header, null, /*#__PURE__*/react.createElement(Button_Button, {
    style: "primary",
    url: `https://${pockethost}/a?${utmParams}`,
    source: "home_view_list"
  }, /*#__PURE__*/react.createElement("span", {
    "data-l10n-id": "pocket-panel-header-my-list"
  }))), /*#__PURE__*/react.createElement("hr", null), recentSavesSection, /*#__PURE__*/react.createElement("hr", null), pockethost && locale?.startsWith("en") && topics?.length && /*#__PURE__*/react.createElement(react.Fragment, null, /*#__PURE__*/react.createElement("h3", {
    className: "header_medium"
  }, "Explore popular topics:"), /*#__PURE__*/react.createElement(PopularTopics_PopularTopics, {
    topics: topics,
    pockethost: pockethost,
    utmParams: utmParams,
    source: "home_popular_topic"
  }))));
}

/* harmony default export */ const Home_Home = (Home);
;// CONCATENATED MODULE: ./content/panels/js/home/overlay.js
/* global Handlebars:false */

/*
HomeOverlay is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/




var HomeOverlay = function (options) {
  this.inited = false;
  this.active = false;
};

HomeOverlay.prototype = {
  create({
    pockethost
  }) {
    const {
      searchParams
    } = new URL(window.location.href);
    const locale = searchParams.get(`locale`) || ``;
    const hideRecentSaves = searchParams.get(`hiderecentsaves`) === `true`;
    const utmSource = searchParams.get(`utmSource`);
    const utmCampaign = searchParams.get(`utmCampaign`);
    const utmContent = searchParams.get(`utmContent`);

    if (this.active) {
      return;
    }

    this.active = true;
    react_dom.render( /*#__PURE__*/react.createElement(Home_Home, {
      locale: locale,
      hideRecentSaves: hideRecentSaves,
      pockethost: pockethost,
      utmSource: utmSource,
      utmCampaign: utmCampaign,
      utmContent: utmContent,
      topics: [{
        title: "Technology",
        topic: "technology"
      }, {
        title: "Self Improvement",
        topic: "self-improvement"
      }, {
        title: "Food",
        topic: "food"
      }, {
        title: "Parenting",
        topic: "parenting"
      }, {
        title: "Science",
        topic: "science"
      }, {
        title: "Entertainment",
        topic: "entertainment"
      }, {
        title: "Career",
        topic: "career"
      }, {
        title: "Health",
        topic: "health"
      }, {
        title: "Travel",
        topic: "travel"
      }, {
        title: "Must-Reads",
        topic: "must-reads"
      }]
    }), document.querySelector(`body`));

    if (window?.matchMedia(`(prefers-color-scheme: dark)`).matches) {
      document.querySelector(`body`).classList.add(`theme_dark`);
    }
  }

};
/* harmony default export */ const overlay = (HomeOverlay);
;// CONCATENATED MODULE: ./content/panels/js/components/Signup/Signup.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */




function Signup(props) {
  const {
    locale,
    pockethost,
    utmSource,
    utmCampaign,
    utmContent
  } = props;
  const utmParams = `utm_source=${utmSource}${utmCampaign && utmContent ? `&utm_campaign=${utmCampaign}&utm_content=${utmContent}` : ``}`;
  return /*#__PURE__*/react.createElement("div", {
    className: "stp_panel_container"
  }, /*#__PURE__*/react.createElement("div", {
    className: "stp_panel stp_panel_signup"
  }, /*#__PURE__*/react.createElement(Header_Header, null, /*#__PURE__*/react.createElement(Button_Button, {
    style: "secondary",
    url: `https://${pockethost}/login?${utmParams}`,
    source: "log_in"
  }, /*#__PURE__*/react.createElement("span", {
    "data-l10n-id": "pocket-panel-signup-login"
  }))), /*#__PURE__*/react.createElement("hr", null), locale?.startsWith("en") ? /*#__PURE__*/react.createElement(react.Fragment, null, /*#__PURE__*/react.createElement("div", {
    className: "stp_signup_content_wrapper"
  }, /*#__PURE__*/react.createElement("h3", {
    className: "header_medium",
    "data-l10n-id": "pocket-panel-signup-cta-a-fix"
  }), /*#__PURE__*/react.createElement("p", {
    "data-l10n-id": "pocket-panel-signup-cta-b"
  })), /*#__PURE__*/react.createElement("div", {
    className: "stp_signup_content_wrapper"
  }, /*#__PURE__*/react.createElement("hr", null)), /*#__PURE__*/react.createElement("div", {
    className: "stp_signup_content_wrapper"
  }, /*#__PURE__*/react.createElement("div", {
    className: "stp_signup_img_rainbow_reader"
  }), /*#__PURE__*/react.createElement("h3", {
    className: "header_medium"
  }, "Get thought-provoking article recommendations"), /*#__PURE__*/react.createElement("p", null, "Find stories that go deep into a subject or offer a new perspective."))) : /*#__PURE__*/react.createElement("div", {
    className: "stp_signup_content_wrapper"
  }, /*#__PURE__*/react.createElement("h3", {
    className: "header_large",
    "data-l10n-id": "pocket-panel-signup-cta-a-fix"
  }), /*#__PURE__*/react.createElement("p", {
    "data-l10n-id": "pocket-panel-signup-cta-b-short"
  }), /*#__PURE__*/react.createElement("strong", null, /*#__PURE__*/react.createElement("p", {
    "data-l10n-id": "pocket-panel-signup-cta-c"
  }))), /*#__PURE__*/react.createElement("hr", null), /*#__PURE__*/react.createElement("span", {
    className: "stp_button_wide"
  }, /*#__PURE__*/react.createElement(Button_Button, {
    style: "primary",
    url: `https://${pockethost}/ff_signup?${utmParams}`,
    source: "sign_up_1"
  }, /*#__PURE__*/react.createElement("span", {
    "data-l10n-id": "pocket-panel-button-activate"
  })))));
}

/* harmony default export */ const Signup_Signup = (Signup);
;// CONCATENATED MODULE: ./content/panels/js/signup/overlay.js
/*
SignupOverlay is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/





var SignupOverlay = function (options) {
  this.inited = false;
  this.active = false;

  this.create = function ({
    pockethost
  }) {
    // Extract local variables passed into template via URL query params
    const {
      searchParams
    } = new URL(window.location.href);
    const locale = searchParams.get(`locale`) || ``;
    const utmSource = searchParams.get(`utmSource`);
    const utmCampaign = searchParams.get(`utmCampaign`);
    const utmContent = searchParams.get(`utmContent`);

    if (this.active) {
      return;
    }

    this.active = true; // Create actual content

    react_dom.render( /*#__PURE__*/react.createElement(Signup_Signup, {
      pockethost: pockethost,
      utmSource: utmSource,
      utmCampaign: utmCampaign,
      utmContent: utmContent,
      locale: locale
    }), document.querySelector(`body`));

    if (window?.matchMedia(`(prefers-color-scheme: dark)`).matches) {
      document.querySelector(`body`).classList.add(`theme_dark`);
    } // tell back end we're ready


    messages.sendMessage("PKT_show_signup");
  };
};

/* harmony default export */ const signup_overlay = (SignupOverlay);
;// CONCATENATED MODULE: ./content/panels/js/components/TagPicker/TagPicker.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */



function TagPicker(props) {
  const [tags, setTags] = (0,react.useState)(props.tags);
  const [duplicateTag, setDuplicateTag] = (0,react.useState)(null);
  const [inputValue, setInputValue] = (0,react.useState)("");
  const [usedTags, setUsedTags] = (0,react.useState)([]); // Status be success, waiting, or error.

  const [{
    tagInputStatus,
    tagInputErrorMessage
  }, setTagInputStatus] = (0,react.useState)({
    tagInputStatus: "",
    tagInputErrorMessage: ""
  });
  const inputToSubmit = inputValue.trim();
  const tagsToSubmit = [...tags, ...(inputToSubmit ? [inputToSubmit] : [])];

  let handleKeyDown = e => {
    const enterKey = e.keyCode === 13;
    const commaKey = e.keyCode === 188;
    const tabKey = inputValue && e.keyCode === 9; // Submit tags on enter with no input.
    // Enter tag on comma, tab, or enter with input.
    // Tab to next element with no input.

    if (commaKey || enterKey || tabKey) {
      e.preventDefault();

      if (inputValue) {
        addTag();
      } else if (enterKey) {
        submitTags();
      }
    }
  };

  let addTag = () => {
    let newDuplicateTag = tags.find(item => item === inputToSubmit);

    if (!inputToSubmit?.length) {
      return;
    }

    setInputValue(``); // Clear out input

    if (!newDuplicateTag) {
      setTags(tagsToSubmit);
    } else {
      setDuplicateTag(newDuplicateTag);
      setTimeout(() => {
        setDuplicateTag(null);
      }, 1000);
    }
  };

  let removeTag = index => {
    let updatedTags = tags.slice(0); // Shallow copied array

    updatedTags.splice(index, 1);
    setTags(updatedTags);
  };

  let submitTags = () => {
    if (!props.itemUrl || !tagsToSubmit?.length) {
      return;
    }

    setTagInputStatus({
      tagInputStatus: "waiting",
      tagInputErrorMessage: ""
    });
    messages.sendMessage("PKT_addTags", {
      url: props.itemUrl,
      tags: tagsToSubmit
    }, function (resp) {
      const {
        data
      } = resp;

      if (data.status === "success") {
        setTagInputStatus({
          tagInputStatus: "success",
          tagInputErrorMessage: ""
        });
      } else if (data.status === "error") {
        setTagInputStatus({
          tagInputStatus: "error",
          tagInputErrorMessage: data.error.message
        });
      }
    });
  };

  (0,react.useEffect)(() => {
    messages.sendMessage("PKT_getTags", {}, resp => setUsedTags(resp?.data?.tags));
  }, []);
  return /*#__PURE__*/react.createElement("div", {
    className: "stp_tag_picker"
  }, !tagInputStatus && /*#__PURE__*/react.createElement(react.Fragment, null, /*#__PURE__*/react.createElement("h3", {
    className: "header_small",
    "data-l10n-id": "pocket-panel-signup-add-tags"
  }), /*#__PURE__*/react.createElement("div", {
    className: "stp_tag_picker_tags"
  }, tags.map((tag, i) => /*#__PURE__*/react.createElement("div", {
    className: `stp_tag_picker_tag${duplicateTag === tag ? ` stp_tag_picker_tag_duplicate` : ``}`
  }, tag, /*#__PURE__*/react.createElement("button", {
    onClick: () => removeTag(i),
    className: `stp_tag_picker_tag_remove`
  }, "X"))), /*#__PURE__*/react.createElement("div", {
    className: "stp_tag_picker_input_wrapper"
  }, /*#__PURE__*/react.createElement("input", {
    className: "stp_tag_picker_input",
    type: "text",
    list: "tag-list",
    value: inputValue,
    onChange: e => setInputValue(e.target.value),
    onKeyDown: e => handleKeyDown(e),
    maxlength: "25"
  }), /*#__PURE__*/react.createElement("datalist", {
    id: "tag-list"
  }, usedTags.map(item => /*#__PURE__*/react.createElement("option", {
    key: item,
    value: item
  }))), /*#__PURE__*/react.createElement("button", {
    className: "stp_tag_picker_button",
    disabled: !tagsToSubmit?.length,
    "data-l10n-id": "pocket-panel-saved-save-tags",
    onClick: () => submitTags()
  })))), tagInputStatus === "waiting" && /*#__PURE__*/react.createElement("h3", {
    className: "header_large",
    "data-l10n-id": "pocket-panel-saved-processing-tags"
  }), tagInputStatus === "success" && /*#__PURE__*/react.createElement("h3", {
    className: "header_large",
    "data-l10n-id": "pocket-panel-saved-tags-saved"
  }), tagInputStatus === "error" && /*#__PURE__*/react.createElement("h3", {
    className: "header_small"
  }, tagInputErrorMessage));
}

/* harmony default export */ const TagPicker_TagPicker = (TagPicker);
;// CONCATENATED MODULE: ./content/panels/js/components/Saved/Saved.jsx
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */







function Saved(props) {
  const {
    locale,
    pockethost,
    utmSource,
    utmCampaign,
    utmContent
  } = props; // savedStatus can be success, loading, or error.

  const [{
    savedStatus,
    savedErrorId,
    itemId,
    itemUrl
  }, setSavedStatusState] = (0,react.useState)({
    savedStatus: "loading"
  }); // removedStatus can be removed, removing, or error.

  const [{
    removedStatus,
    removedErrorMessage
  }, setRemovedStatusState] = (0,react.useState)({});
  const [savedStory, setSavedStoryState] = (0,react.useState)();
  const [articleInfoAttempted, setArticleInfoAttempted] = (0,react.useState)();
  const [{
    similarRecs,
    similarRecsModel
  }, setSimilarRecsState] = (0,react.useState)({});
  const utmParams = `utm_source=${utmSource}${utmCampaign && utmContent ? `&utm_campaign=${utmCampaign}&utm_content=${utmContent}` : ``}`;

  function removeItem(event) {
    event.preventDefault();
    setRemovedStatusState({
      removedStatus: "removing"
    });
    messages.sendMessage("PKT_deleteItem", {
      itemId
    }, function (resp) {
      const {
        data
      } = resp;

      if (data.status == "success") {
        setRemovedStatusState({
          removedStatus: "removed"
        });
      } else if (data.status == "error") {
        let errorMessage = ""; // The server returns English error messages, so in the case of
        // non English, we do our best with a generic translated error.

        if (data.error.message && locale?.startsWith("en")) {
          errorMessage = data.error.message;
        }

        setRemovedStatusState({
          removedStatus: "error",
          removedErrorMessage: errorMessage
        });
      }
    });
  }

  (0,react.useEffect)(() => {
    // Wait confirmation of save before flipping to final saved state
    messages.addMessageListener("PKT_saveLink", function (resp) {
      const {
        data
      } = resp;

      if (data.status == "error") {
        // Use localizedKey or fallback to a generic catch all error.
        setSavedStatusState({
          savedStatus: "error",
          savedErrorId: data?.error?.localizedKey || "pocket-panel-saved-error-generic"
        });
        return;
      } // Success, so no localized error id needed.


      setSavedStatusState({
        savedStatus: "success",
        itemId: data.item?.item_id,
        itemUrl: data.item?.given_url,
        savedErrorId: ""
      });
    });
    messages.addMessageListener("PKT_articleInfoFetched", function (resp) {
      setSavedStoryState(resp?.data?.item_preview);
    });
    messages.addMessageListener("PKT_getArticleInfoAttempted", function (resp) {
      setArticleInfoAttempted(true);
    });
    messages.addMessageListener("PKT_renderItemRecs", function (resp) {
      const {
        data
      } = resp; // This is the ML model used to recommend the story.
      // Right now this value is the same for all three items returned together,
      // so we can just use the first item's value for all.

      const model = data?.recommendations?.[0]?.experiment || "";
      setSimilarRecsState({
        similarRecs: data?.recommendations?.map(rec => rec.item),
        similarRecsModel: model
      });
    }); // tell back end we're ready

    messages.sendMessage("PKT_show_saved");
  }, []);

  if (savedStatus === "error") {
    return /*#__PURE__*/react.createElement("div", {
      className: "stp_panel_container"
    }, /*#__PURE__*/react.createElement("div", {
      className: "stp_panel stp_panel_error"
    }, /*#__PURE__*/react.createElement("div", {
      className: "stp_panel_error_icon"
    }), /*#__PURE__*/react.createElement("h3", {
      className: "header_large",
      "data-l10n-id": "pocket-panel-saved-error-not-saved"
    }), /*#__PURE__*/react.createElement("p", {
      "data-l10n-id": savedErrorId
    })));
  }

  return /*#__PURE__*/react.createElement("div", {
    className: "stp_panel_container"
  }, /*#__PURE__*/react.createElement("div", {
    className: "stp_panel stp_panel_saved"
  }, /*#__PURE__*/react.createElement(Header_Header, null, /*#__PURE__*/react.createElement(Button_Button, {
    style: "primary",
    url: `https://${pockethost}/a?${utmParams}`,
    source: "view_list"
  }, /*#__PURE__*/react.createElement("span", {
    "data-l10n-id": "pocket-panel-header-my-list"
  }))), /*#__PURE__*/react.createElement("hr", null), !removedStatus && savedStatus === "success" && /*#__PURE__*/react.createElement(react.Fragment, null, /*#__PURE__*/react.createElement("h3", {
    className: "header_large header_flex"
  }, /*#__PURE__*/react.createElement("span", {
    "data-l10n-id": "pocket-panel-saved-page-saved-b"
  }), /*#__PURE__*/react.createElement(Button_Button, {
    style: "text",
    onClick: removeItem
  }, /*#__PURE__*/react.createElement("span", {
    "data-l10n-id": "pocket-panel-button-remove"
  }))), savedStory && /*#__PURE__*/react.createElement(ArticleList_ArticleList, {
    articles: [savedStory],
    openInPocketReader: true,
    utmParams: utmParams
  }), articleInfoAttempted && /*#__PURE__*/react.createElement(TagPicker_TagPicker, {
    tags: [],
    itemUrl: itemUrl
  }), articleInfoAttempted && similarRecs?.length && locale?.startsWith("en") && /*#__PURE__*/react.createElement(react.Fragment, null, /*#__PURE__*/react.createElement("hr", null), /*#__PURE__*/react.createElement("h3", {
    className: "header_medium"
  }, "Similar Stories"), /*#__PURE__*/react.createElement(ArticleList_ArticleList, {
    articles: similarRecs,
    source: "on_save_recs",
    model: similarRecsModel,
    utmParams: utmParams
  }))), savedStatus === "loading" && /*#__PURE__*/react.createElement("h3", {
    className: "header_large",
    "data-l10n-id": "pocket-panel-saved-saving-tags"
  }), removedStatus === "removing" && /*#__PURE__*/react.createElement("h3", {
    className: "header_large header_center",
    "data-l10n-id": "pocket-panel-saved-processing-remove"
  }), removedStatus === "removed" && /*#__PURE__*/react.createElement("h3", {
    className: "header_large header_center",
    "data-l10n-id": "pocket-panel-saved-removed"
  }), removedStatus === "error" && /*#__PURE__*/react.createElement(react.Fragment, null, /*#__PURE__*/react.createElement("h3", {
    className: "header_large",
    "data-l10n-id": "pocket-panel-saved-error-remove"
  }), removedErrorMessage && /*#__PURE__*/react.createElement("p", null, removedErrorMessage))));
}

/* harmony default export */ const Saved_Saved = (Saved);
;// CONCATENATED MODULE: ./content/panels/js/saved/overlay.js
/*
SavedOverlay is the view itself and contains all of the methods to manipute the overlay and messaging.
It does not contain any logic for saving or communication with the extension or server.
*/




var SavedOverlay = function (options) {
  this.inited = false;
  this.active = false;
};

SavedOverlay.prototype = {
  create({
    pockethost
  }) {
    if (this.active) {
      return;
    }

    this.active = true;
    const {
      searchParams
    } = new URL(window.location.href);
    const locale = searchParams.get(`locale`) || ``;
    const utmSource = searchParams.get(`utmSource`);
    const utmCampaign = searchParams.get(`utmCampaign`);
    const utmContent = searchParams.get(`utmContent`);
    react_dom.render( /*#__PURE__*/react.createElement(Saved_Saved, {
      locale: locale,
      pockethost: pockethost,
      utmSource: utmSource,
      utmCampaign: utmCampaign,
      utmContent: utmContent
    }), document.querySelector(`body`));

    if (window?.matchMedia(`(prefers-color-scheme: dark)`).matches) {
      document.querySelector(`body`).classList.add(`theme_dark`);
    }
  }

};
/* harmony default export */ const saved_overlay = (SavedOverlay);
;// CONCATENATED MODULE: ./content/panels/js/style-guide/overlay.js








var StyleGuideOverlay = function (options) {};

StyleGuideOverlay.prototype = {
  create() {
    // TODO: Wrap popular topics component in JSX to work without needing an explicit container hierarchy for styling
    react_dom.render( /*#__PURE__*/react.createElement("div", null, /*#__PURE__*/react.createElement("h3", null, "JSX Components:"), /*#__PURE__*/react.createElement("h4", {
      className: "stp_styleguide_h4"
    }, "Buttons"), /*#__PURE__*/react.createElement("h5", {
      className: "stp_styleguide_h5"
    }, "text"), /*#__PURE__*/react.createElement(Button_Button, {
      style: "text",
      url: "https://example.org",
      source: "styleguide"
    }, "Text Button"), /*#__PURE__*/react.createElement("h5", {
      className: "stp_styleguide_h5"
    }, "primary"), /*#__PURE__*/react.createElement(Button_Button, {
      style: "primary",
      url: "https://example.org",
      source: "styleguide"
    }, "Primary Button"), /*#__PURE__*/react.createElement("h5", {
      className: "stp_styleguide_h5"
    }, "secondary"), /*#__PURE__*/react.createElement(Button_Button, {
      style: "secondary",
      url: "https://example.org",
      source: "styleguide"
    }, "Secondary Button"), /*#__PURE__*/react.createElement("h5", {
      className: "stp_styleguide_h5"
    }, "primary wide"), /*#__PURE__*/react.createElement("span", {
      className: "stp_button_wide"
    }, /*#__PURE__*/react.createElement(Button_Button, {
      style: "primary",
      url: "https://example.org",
      source: "styleguide"
    }, "Primary Wide Button")), /*#__PURE__*/react.createElement("h5", {
      className: "stp_styleguide_h5"
    }, "secondary wide"), /*#__PURE__*/react.createElement("span", {
      className: "stp_button_wide"
    }, /*#__PURE__*/react.createElement(Button_Button, {
      style: "secondary",
      url: "https://example.org",
      source: "styleguide"
    }, "Secondary Wide Button")), /*#__PURE__*/react.createElement("h4", {
      className: "stp_styleguide_h4"
    }, "Header"), /*#__PURE__*/react.createElement(Header_Header, null, /*#__PURE__*/react.createElement(Button_Button, {
      style: "primary",
      url: "https://example.org",
      source: "styleguide"
    }, "View My List")), /*#__PURE__*/react.createElement("h4", {
      className: "stp_styleguide_h4"
    }, "PopularTopics"), /*#__PURE__*/react.createElement(PopularTopics_PopularTopics, {
      pockethost: `getpocket.com`,
      source: `styleguide`,
      utmParams: `utm_source=styleguide`,
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
    }), /*#__PURE__*/react.createElement("h4", {
      className: "stp_styleguide_h4"
    }, "ArticleList"), /*#__PURE__*/react.createElement(ArticleList_ArticleList, {
      source: `styleguide`,
      articles: [{
        title: "Article Title",
        publisher: "Publisher",
        thumbnail: "https://img-getpocket.cdn.mozilla.net/80x80/https://www.raritanheadwaters.org/wp-content/uploads/2020/04/red-fox.jpg",
        url: "https://example.org",
        alt: "Alt Text"
      }, {
        title: "Article Title (No Publisher)",
        thumbnail: "https://img-getpocket.cdn.mozilla.net/80x80/https://www.raritanheadwaters.org/wp-content/uploads/2020/04/red-fox.jpg",
        url: "https://example.org",
        alt: "Alt Text"
      }, {
        title: "Article Title (No Thumbnail)",
        publisher: "Publisher",
        url: "https://example.org",
        alt: "Alt Text"
      }]
    }), /*#__PURE__*/react.createElement("h4", {
      className: "stp_styleguide_h4"
    }, "TagPicker"), /*#__PURE__*/react.createElement(TagPicker_TagPicker, {
      tags: [`futurism`, `politics`, `mozilla`]
    }), /*#__PURE__*/react.createElement("h3", null, "Typography:"), /*#__PURE__*/react.createElement("h2", {
      className: "header_large"
    }, ".header_large"), /*#__PURE__*/react.createElement("h3", {
      className: "header_medium"
    }, ".header_medium"), /*#__PURE__*/react.createElement("p", null, "paragraph"), /*#__PURE__*/react.createElement("h3", null, "Native Elements:"), /*#__PURE__*/react.createElement("h4", {
      className: "stp_styleguide_h4"
    }, "Horizontal Rule"), /*#__PURE__*/react.createElement("hr", null)), document.querySelector(`#stp_style_guide_components`));
  }

};
/* harmony default export */ const style_guide_overlay = (StyleGuideOverlay);
;// CONCATENATED MODULE: ./content/panels/js/main.js
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global RPMGetStringPref:false */






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
    const pockethost = RPMGetStringPref("extensions.pocket.site") || "getpocket.com";
    this.overlay.create({
      pockethost
    });
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
/******/ 				installedChunks[chunkId] = 0;
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
/******/ 	var __webpack_exports__ = __webpack_require__.O(undefined, [736], () => (__webpack_require__(299)))
/******/ 	__webpack_exports__ = __webpack_require__.O(__webpack_exports__);
/******/ 	
/******/ })()
;