/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";


this.devtools = class extends ExtensionAPI {
  getAPI(context) {
    return {
      devtools: {},
    };
  }
};

