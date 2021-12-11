"use strict";

/* exported PrefixUtils */
const PrefixUtils = {
  getFormattedPrefix(prefix) {
    return prefix + " ";
  },
  removePrefix(prefix, title) {
    if (this.hasPrefix(prefix, title)) {
      return title.substring(this.getFormattedPrefix(prefix).length);
    }
    return title;
  },
  addPrefix(prefix, title) {
    if (prefix && !this.hasPrefix(prefix, title)) {
      return this.getFormattedPrefix(prefix) + title;
    }
    return title;
  },
  hasPrefix(prefix, title) {
    if (prefix && prefix.length > 0) {
      return title.startsWith(this.getFormattedPrefix(prefix));
    }
    return false;
  }
};
