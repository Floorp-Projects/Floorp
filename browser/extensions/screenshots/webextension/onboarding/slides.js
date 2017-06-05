/* globals log, catcher, onboardingHtml, onboardingCss, util, shooter, callBackground, assertIsTrusted */

"use strict";

this.slides = (function() {
  let exports = {};

  const { watchFunction } = catcher;

  let iframe;
  let doc;
  let currentSlide = 1;
  let numberOfSlides;
  let callbacks;

  exports.display = function(addCallbacks) {
    if (iframe) {
      throw new Error("Attemted to call slides.display() twice");
    }
    return new Promise((resolve, reject) => {
      callbacks = addCallbacks;
      // FIXME: a lot of this iframe logic is in ui.js; maybe move to util.js
      iframe = document.createElement("iframe");
      iframe.src = browser.extension.getURL("blank.html");
      iframe.id = "firefox-screenshots-onboarding-iframe";
      iframe.style.zIndex = "99999999999";
      iframe.style.border = "none";
      iframe.style.position = "fixed";
      iframe.style.top = "0";
      iframe.style.left = "0";
      iframe.style.margin = "0";
      iframe.scrolling = "no";
      updateIframeSize();
      let html = onboardingHtml.replace('<style></style>', `<style>${onboardingCss}</style>`);
      html = html.replace(/MOZ_EXTENSION([^\"]+)/g, (match, filename) => {
        return browser.extension.getURL(filename);
      });
      iframe.onload = catcher.watchFunction(() => {
        let parsedDom = (new DOMParser()).parseFromString(
          html,
          "text/html"
        );
        doc = iframe.contentDocument;
        doc.replaceChild(
          doc.adoptNode(parsedDom.documentElement),
          doc.documentElement
        );
        doc.addEventListener("keyup", onKeyUp);
        doc.documentElement.dir = browser.i18n.getMessage("@@bidi_dir");
        doc.documentElement.lang = browser.i18n.getMessage("@@ui_locale");
        localizeText(doc);
        activateSlide(doc);
        resolve();
      });
      document.body.appendChild(iframe);
      iframe.focus();
      window.addEventListener("resize", onResize);
    });
  };

  exports.remove = exports.unload = function() {
    window.removeEventListener("resize", onResize);
    if (doc) {
      doc.removeEventListener("keyup", onKeyUp);
    }
    util.removeNode(iframe);
    iframe = doc = null;
    currentSlide = 1;
    numberOfSlides = undefined;
    callbacks = undefined;
  };

  function localizeText(doc) {
    let els = doc.querySelectorAll("[data-l10n-id]");
    for (let el of els) {
      let id = el.getAttribute("data-l10n-id");
      let text = browser.i18n.getMessage(id);
      el.textContent = text;
    }
    els = doc.querySelectorAll("[data-l10n-label-id]");
    for (let el of els) {
      let id = el.getAttribute("data-l10n-label-id");
      let text = browser.i18n.getMessage(id);
      el.setAttribute("aria-label", text);
    }
    // termsAndPrivacyNoticeCloudServices is a more complicated substitution:
    let termsContainer = doc.querySelector(".onboarding-legal-notice");
    termsContainer.innerHTML = "";
    let termsSentinel = "__TERMS__";
    let privacySentinel = "__PRIVACY__";
    let sentinelSplitter = "!!!";
    let linkTexts = {
      [termsSentinel]: browser.i18n.getMessage("termsAndPrivacyNoticeTermsLink"),
      [privacySentinel]: browser.i18n.getMessage("termsAndPrivacyNoticyPrivacyLink")
    };
    let linkUrls = {
      [termsSentinel]: "https://www.mozilla.org/about/legal/terms/services/",
      [privacySentinel]: "https://www.mozilla.org/privacy/firefox-cloud/"
    };
    let text = browser.i18n.getMessage(
      "termsAndPrivacyNoticeCloudServices",
      [sentinelSplitter + termsSentinel + sentinelSplitter,
       sentinelSplitter + privacySentinel + sentinelSplitter]);
    let parts = text.split(sentinelSplitter);
    for (let part of parts) {
      let el;
      if (part === termsSentinel || part === privacySentinel) {
        el = doc.createElement("a");
        el.href = linkUrls[part];
        el.textContent = linkTexts[part];
        el.target = "_blank";
        el.id = (part === termsSentinel) ? "terms" : "privacy";
      } else {
        el = doc.createTextNode(part);
      }
      termsContainer.appendChild(el);
    }
  }

  function activateSlide(doc) {
    numberOfSlides = parseInt(doc.querySelector("[data-number-of-slides]").getAttribute("data-number-of-slides"), 10);
    doc.querySelector("#next").addEventListener("click", watchFunction(assertIsTrusted(() => {
      shooter.sendEvent("navigate-slide", "next");
      next();
    })));
    doc.querySelector("#prev").addEventListener("click", watchFunction(assertIsTrusted(() => {
      shooter.sendEvent("navigate-slide", "prev");
      prev();
    })));
    for (let el of doc.querySelectorAll(".goto-slide")) {
      el.addEventListener("click", watchFunction(assertIsTrusted((event) => {
        shooter.sendEvent("navigate-slide", "goto");
        let el = event.target;
        let index = parseInt(el.getAttribute("data-number"), 10);
        setSlide(index);
      })));
    }
    doc.querySelector("#skip").addEventListener("click", watchFunction(assertIsTrusted((event) => {
      shooter.sendEvent("cancel-slides", "skip");
      callbacks.onEnd();
    })));
    doc.querySelector("#done").addEventListener("click", watchFunction(assertIsTrusted((event) => {
      shooter.sendEvent("finish-slides", "done");
      callbacks.onEnd();
    })));
    // Note: e10s breaks the terms and privacy anchor tags. Work around this by
    // manually opening the correct URLs on click until bug 1357589 is fixed.
    doc.querySelector("#terms").addEventListener("click", watchFunction(assertIsTrusted((event) => {
      event.preventDefault();
      callBackground("openTermsPage");
    })));
    doc.querySelector("#privacy").addEventListener("click", watchFunction(assertIsTrusted((event) => {
      event.preventDefault();
      callBackground("openPrivacyPage");
    })));
    setSlide(1);
  }

  function next() {
    setSlide(currentSlide + 1);
  }

  function prev() {
    setSlide(currentSlide - 1);
  }

  const onResize = catcher.watchFunction(function() {
    if (!iframe) {
      log.warn("slides onResize called when iframe is not setup");
      return;
    }
    updateIframeSize();
  });

  function updateIframeSize() {
    iframe.style.height = window.innerHeight + "px";
    iframe.style.width = window.innerWidth + "px";
  }

  const onKeyUp = catcher.watchFunction(assertIsTrusted(function(event) {
    if ((event.key || event.code) === "Escape") {
      shooter.sendEvent("cancel-slides", "keyboard-escape");
      callbacks.onEnd();
    }
    if ((event.key || event.code) === "ArrowRight") {
      shooter.sendEvent("navigate-slide", "keyboard-arrowright");
      next();
    }
    if ((event.key || event.code) === "ArrowLeft") {
      shooter.sendEvent("navigate-slide", "keyboard-arrowleft");
      prev();
    }
  }));

  function setSlide(index) {
    if (index < 1) {
      index = 1;
    }
    if (index > numberOfSlides) {
      index = numberOfSlides;
    }
    shooter.sendEvent("visited-slide", `slide-${index}`);
    currentSlide = index;
    let slideEl = doc.querySelector("#slide-container");
    for (let i = 1; i <= numberOfSlides; i++) {
      let className = `active-slide-${i}`;
      if (i == currentSlide) {
        slideEl.classList.add(className);
      } else {
        slideEl.classList.remove(className);
      }
    }
  }

  return exports;
})();
null;
