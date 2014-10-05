/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Set the current translation content
function setBody(data) {
  $("#legal-copy").html(data);
}

function normalizeLocale(lang) {
  return lang.replace(/-/g, "_");
}

$(document).ready(function() {
  // Get the favorite language
  var lang, defaultLang = "en-US";
  $.get(loop.config.serverUrl, function(data) {
    if (data.hasOwnProperty("i18n")) {
      lang = normalizeLocale(data.i18n.lang);
      defaultLang = normalizeLocale(data.i18n.defaultLang);
    }
    if (lang === undefined) {
      lang = normalizeLocale(defaultLang);
    }

    $.get(lang + ".html")
      .done(setBody)
      .fail(function() {
        $.get(defaultLang + ".html")
          .done(setBody);
      });
  });
});
