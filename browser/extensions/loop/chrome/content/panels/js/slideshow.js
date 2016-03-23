/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.slideshow = function (mozL10n) {
  "use strict";

  /**
   * Slideshow initialisation.
   */

  function init() {
    var requests = [["GetAllStrings"], ["GetLocale"], ["GetPluralRule"]];
    return loop.requestMulti.apply(null, requests).then(function (results) {
      // `requestIdx` is keyed off the order of the `requests`
      // array. Be careful to update both when making changes.
      var requestIdx = 0;
      // Do the initial L10n setup, we do this before anything
      // else to ensure the L10n environment is setup correctly.
      var stringBundle = results[requestIdx];
      var locale = results[++requestIdx];
      var pluralRule = results[++requestIdx];
      mozL10n.initialize({
        locale: locale,
        pluralRule: pluralRule,
        getStrings: function (key) {
          if (!(key in stringBundle)) {
            return "{ textContent: '' }";
          }

          return JSON.stringify({
            textContent: stringBundle[key]
          });
        }
      });

      document.documentElement.setAttribute("lang", mozL10n.language.code);
      document.documentElement.setAttribute("dir", mozL10n.language.direction);
      document.body.setAttribute("platform", loop.shared.utils.getPlatform());
      var clientSuperShortname = mozL10n.get("clientSuperShortname");
      var data = [{
        id: "slide1",
        imageClass: "slide1-image",
        title: mozL10n.get("fte_slide_1_title"),
        text: mozL10n.get("fte_slide_1_copy", {
          clientShortname2: mozL10n.get("clientShortname2")
        })
      }, {
        id: "slide2",
        imageClass: "slide2-image",
        title: mozL10n.get("fte_slide_2_title2"),
        text: mozL10n.get("fte_slide_2_copy2", {
          clientShortname2: mozL10n.get("clientShortname2")
        })
      }, {
        id: "slide3",
        imageClass: "slide3-image",
        title: mozL10n.get("fte_slide_3_title"),
        text: mozL10n.get("fte_slide_3_copy", {
          clientSuperShortname: clientSuperShortname
        })
      }, {
        id: "slide4",
        imageClass: "slide4-image",
        title: mozL10n.get("fte_slide_4_title", {
          clientSuperShortname: clientSuperShortname
        }),
        text: mozL10n.get("fte_slide_4_copy", {
          brandShortname: mozL10n.get("brandShortname")
        })
      }];

      loop.SimpleSlideshow.init("#main", data);
    });
  }

  return {
    init: init
  };
}(document.mozL10n);

document.addEventListener("DOMContentLoaded", loop.slideshow.init);
