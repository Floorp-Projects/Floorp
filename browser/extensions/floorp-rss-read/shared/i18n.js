"use strict";

const I18N = {
  getMessage(id, subst) {
    return chrome.i18n.getMessage(id, subst);
  },
  substituteAttribute(element, attribute, property, subst) {
    const message = this.getMessage(element.getAttribute(attribute), subst);
    element[property] = message;
  },
  substituteAllAttributes(attribute, property) {
    for (const element of document.querySelectorAll(`[${attribute}]`)) {
      if (!element.hasAttribute("data-i18n-manual")) {
        this.substituteAttribute(element, attribute, property);
      }
    }
  },
  init() {
    this.substituteAllAttributes("data-i18n-text", "textContent");
    this.substituteAllAttributes("data-i18n-title", "title");
  },
};

I18N.init();
