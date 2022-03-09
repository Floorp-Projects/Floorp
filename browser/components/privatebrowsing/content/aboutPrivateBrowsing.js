/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

/**
 * Determines whether a given value is a fluent id or plain text and adds it to an element
 * @param {Array<[HTMLElement, string]>} items An array of [element, value] where value is
 *                                       a fluent id starting with "fluent:" or plain text
 */
async function translateElements(container, items) {
  // We need to wait for fluent to initialize
  await document.l10n.ready;

  items.forEach(([element, value]) => {
    // Skip empty text or elements
    if (!element || !value) {
      return;
    }
    const fluentId = value.replace(/^fluent:/, "");
    if (fluentId !== value) {
      document.l10n.setAttributes(element, fluentId);
    } else {
      element.textContent = value;
      element.removeAttribute("data-l10n-id");
    }
  });

  document.l10n.translateFragment(container);
}

async function renderInfo({
  infoEnabled = false,
  infoTitle,
  infoTitleEnabled,
  infoBody,
  infoLinkText,
  infoLinkUrl,
  infoIcon,
} = {}) {
  const container = document.querySelector(".info");
  if (infoEnabled === false) {
    container.remove();
    return;
  }

  const titleEl = document.getElementById("info-title");
  const bodyEl = document.getElementById("info-body");
  const linkEl = document.getElementById("private-browsing-myths");

  if (infoIcon) {
    container.style.backgroundImage = `url(${infoIcon})`;
  }

  if (!infoTitleEnabled) {
    titleEl.remove();
  }

  await translateElements(container, [
    [titleEl, infoTitle],
    [bodyEl, infoBody],
    [linkEl, infoLinkText],
  ]);

  linkEl.setAttribute(
    "href",
    infoLinkUrl ||
      RPMGetFormatURLPref("app.support.baseURL") + "private-browsing-myths"
  );
  linkEl.setAttribute("target", "_blank");

  linkEl.addEventListener("click", () => {
    window.PrivateBrowsingRecordClick("info_link");
  });
}

async function renderPromo({
  messageId = null,
  promoEnabled = false,
  promoTitle,
  promoTitleEnabled,
  promoLinkText,
  promoLinkUrl,
  promoLinkType,
  promoSectionStyle,
  promoHeader,
  promoImageLarge,
  promoImageSmall,
} = {}) {
  const container = document.querySelector(".promo");
  if (promoEnabled === false) {
    container.remove();
    return false;
  }

  const titleEl = document.getElementById("private-browsing-vpn-text");
  let linkEl = document.getElementById("private-browsing-vpn-link");
  const promoHeaderEl = document.getElementById("promo-header");
  const infoContainerEl = document.querySelector(".info");
  const promoImageLargeEl = document.querySelector(".promo-image-large img");
  const promoImageSmallEl = document.querySelector(".promo-image-small img");
  const dismissBtn = document.querySelector("#dismiss-btn");

  // Setup the private browsing VPN link.
  const vpnPromoUrl =
    promoLinkUrl || RPMGetFormatURLPref("browser.privatebrowsing.vpnpromourl");

  if (promoLinkType === "button") {
    linkEl.classList.add("button");
  }

  if (vpnPromoUrl) {
    linkEl.setAttribute("href", vpnPromoUrl);
    linkEl.setAttribute("target", "_blank");
    linkEl.addEventListener("click", () => {
      window.PrivateBrowsingRecordClick("promo_link");
    });
  } else {
    // If the link is undefined, remove the promo completely
    container.remove();
    return false;
  }

  const onDismissBtnClick = () => {
    window.ASRouterMessage({
      type: "BLOCK_MESSAGE_BY_ID",
      data: { id: messageId },
    });
    window.PrivateBrowsingRecordClick("dismiss_button");
    container.remove();
  };

  if (dismissBtn && messageId) {
    dismissBtn.addEventListener("click", onDismissBtnClick, { once: true });
  }

  if (promoSectionStyle) {
    container.classList.add(promoSectionStyle);

    switch (promoSectionStyle) {
      case "below-search":
        container.remove();
        infoContainerEl?.insertAdjacentElement("beforebegin", container);
        break;
      case "top":
        container.remove();
        document.body.insertAdjacentElement("afterbegin", container);
    }
  }

  if (promoImageLarge) {
    promoImageLargeEl.src = promoImageLarge;
  } else {
    promoImageLargeEl.parentNode.remove();
  }

  if (promoImageSmall) {
    promoImageSmallEl.src = promoImageSmall;
  } else {
    promoImageSmallEl.parentNode.remove();
  }

  if (!promoTitleEnabled) {
    titleEl.remove();
  }

  if (!promoHeader) {
    promoHeaderEl.remove();
  }

  await translateElements(container, [
    [titleEl, promoTitle],
    [linkEl, promoLinkText],
    [promoHeaderEl, promoHeader],
  ]);

  // Only make promo section visible after adding content
  // and translations to prevent layout shifting in page
  container.classList.add("promo-visible");
  return true;
}

/**
 * For every PB newtab loaded a second is pre-rendered in the background.
 * We need to guard against invalid impressions by checking visibility state.
 * If visible record otherwise listen for visibility change and record later.
 */
function recordOnceVisible(message) {
  const recordImpression = () => {
    if (document.visibilityState === "visible") {
      window.ASRouterMessage({
        type: "IMPRESSION",
        data: message,
      });
      // Similar telemetry, but for Nimbus experiments
      window.PrivateBrowsingExposureTelemetry();
      document.removeEventListener("visibilitychange", recordImpression);
    }
  };

  if (document.visibilityState === "visible") {
    window.ASRouterMessage({
      type: "IMPRESSION",
      data: message,
    });
    // Similar telemetry, but for Nimbus experiments
    window.PrivateBrowsingExposureTelemetry();
  } else {
    document.addEventListener("visibilitychange", recordImpression);
  }
}

async function setupFeatureConfig() {
  let config = null;
  let message = null;
  try {
    config = window.PrivateBrowsingFeatureConfig();
  } catch (e) {}
  if (!Object.keys(config).length) {
    try {
      let response = await window.ASRouterMessage({
        type: "PBNEWTAB_MESSAGE_REQUEST",
        data: {},
      });
      message = response?.message;
      config = message?.content;
      config.messageId = message?.id;
    } catch (e) {}
  }

  await renderInfo(config);
  // Check the current geo and don't render if we're in the wrong one.
  const shouldShow = await RPMSendQuery("ShouldShowVPNPromo", {});
  if (shouldShow) {
    let hasRendered = await renderPromo(config);
    if (hasRendered && message) {
      recordOnceVisible(message);
    }
  }
  // For tests
  document.documentElement.setAttribute("PrivateBrowsingRenderComplete", true);
}

document.addEventListener("DOMContentLoaded", function() {
  if (!RPMIsWindowPrivate()) {
    document.documentElement.classList.remove("private");
    document.documentElement.classList.add("normal");
    document
      .getElementById("startPrivateBrowsing")
      .addEventListener("click", function() {
        RPMSendAsyncMessage("OpenPrivateWindow");
      });
    return;
  }

  // We don't do this setup until now, because we don't want to record any impressions until we're
  // sure we're actually running a private window, not just about:privatebrowsing in a normal window.
  setupFeatureConfig();

  // Set up the private search banner.
  const privateSearchBanner = document.getElementById("search-banner");

  RPMSendQuery("ShouldShowSearchBanner", {}).then(engineName => {
    if (engineName) {
      document.l10n.setAttributes(
        document.getElementById("about-private-browsing-search-banner-title"),
        "about-private-browsing-search-banner-title",
        { engineName }
      );
      privateSearchBanner.removeAttribute("hidden");
      document.body.classList.add("showBanner");
    }

    // We set this attribute so that tests know when we are done.
    document.documentElement.setAttribute("SearchBannerInitialized", true);
  });

  function hideSearchBanner() {
    privateSearchBanner.hidden = true;
    document.body.classList.remove("showBanner");
    RPMSendAsyncMessage("SearchBannerDismissed");
  }

  document
    .getElementById("search-banner-close-button")
    .addEventListener("click", () => {
      hideSearchBanner();
    });

  let openSearchOptions = document.getElementById(
    "about-private-browsing-search-banner-description"
  );
  let openSearchOptionsEvtHandler = evt => {
    if (
      evt.target.id == "open-search-options-link" &&
      (evt.keyCode == evt.DOM_VK_RETURN || evt.type == "click")
    ) {
      RPMSendAsyncMessage("OpenSearchPreferences");
      hideSearchBanner();
    }
  };
  openSearchOptions.addEventListener("click", openSearchOptionsEvtHandler);
  openSearchOptions.addEventListener("keypress", openSearchOptionsEvtHandler);

  // Setup the search hand-off box.
  let btn = document.getElementById("search-handoff-button");
  RPMSendQuery("ShouldShowSearch", {}).then(
    ([engineName, shouldHandOffToSearchMode]) => {
      let input = document.querySelector(".fake-textbox");
      if (shouldHandOffToSearchMode) {
        document.l10n.setAttributes(btn, "about-private-browsing-search-btn");
        document.l10n.setAttributes(
          input,
          "about-private-browsing-search-placeholder"
        );
      } else if (engineName) {
        document.l10n.setAttributes(btn, "about-private-browsing-handoff", {
          engine: engineName,
        });
        document.l10n.setAttributes(
          input,
          "about-private-browsing-handoff-text",
          {
            engine: engineName,
          }
        );
      } else {
        document.l10n.setAttributes(
          btn,
          "about-private-browsing-handoff-no-engine"
        );
        document.l10n.setAttributes(
          input,
          "about-private-browsing-handoff-text-no-engine"
        );
      }
    }
  );

  let editable = document.getElementById("fake-editable");
  let DISABLE_SEARCH_TOPIC = "DisableSearch";
  let SHOW_SEARCH_TOPIC = "ShowSearch";
  let SEARCH_HANDOFF_TOPIC = "SearchHandoff";

  function showSearch() {
    btn.classList.remove("focused");
    btn.classList.remove("disabled");
    RPMRemoveMessageListener(SHOW_SEARCH_TOPIC, showSearch);
  }

  function disableSearch() {
    btn.classList.add("disabled");
  }

  function handoffSearch(text) {
    RPMSendAsyncMessage(SEARCH_HANDOFF_TOPIC, { text });
    RPMAddMessageListener(SHOW_SEARCH_TOPIC, showSearch);
    if (text) {
      disableSearch();
    } else {
      btn.classList.add("focused");
      RPMAddMessageListener(DISABLE_SEARCH_TOPIC, disableSearch);
    }
  }
  btn.addEventListener("focus", function() {
    handoffSearch();
  });
  btn.addEventListener("click", function() {
    handoffSearch();
  });

  // Hand-off any text that gets dropped or pasted
  editable.addEventListener("drop", function(ev) {
    ev.preventDefault();
    let text = ev.dataTransfer.getData("text");
    if (text) {
      handoffSearch(text);
    }
  });
  editable.addEventListener("paste", function(ev) {
    ev.preventDefault();
    handoffSearch(ev.clipboardData.getData("Text"));
  });

  // Load contentSearchUI so it sets the search engine icon and name for us.
  new window.ContentSearchHandoffUIController();
});
