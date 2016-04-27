/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

XPCOMUtils.defineLazyGetter(this, "History", () => {
  Cu.import("resource://gre/modules/PlacesUtils.jsm");
  return PlacesUtils.history;
});

extensions.registerSchemaAPI("history", "history", (extension, context) => {
  return {
    history: {
      deleteAll: function() {
        return History.clear();
      },
      deleteRange: function(filter) {
        let newFilter = {
          beginDate: new Date(filter.startTime),
          endDate: new Date(filter.endTime),
        };
        // History.removeVisitsByFilter returns a boolean, but our API should return nothing
        return History.removeVisitsByFilter(newFilter).then(() => undefined);
      },
      deleteUrl: function(details) {
        let url = details.url;
        // History.remove returns a boolean, but our API should return nothing
        return History.remove(url).then(() => undefined);
      },
    },
  };
});
