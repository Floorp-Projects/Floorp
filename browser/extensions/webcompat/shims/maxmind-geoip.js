/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1754389 - Shim Maxmind GeoIP library
 *
 * Some sites rely on Maxmind's GeoIP library which gets blocked by ETP's
 * fingerprinter blocking. With the library window global not being defined
 * functionality may break or the site does not render at all. This shim
 * has it return the United States as the location for all users.
 */

if (!window.geoip2) {
  const continent = {
    code: "NA",
    geoname_id: 6255149,
    names: {
      de: "Nordamerika",
      en: "North America",
      es: "Norteamérica",
      fr: "Amérique du Nord",
      ja: "北アメリカ",
      "pt-BR": "América do Norte",
      ru: "Северная Америка",
      "zh-CN": "北美洲",
    },
  };

  const country = {
    geoname_id: 6252001,
    iso_code: "US",
    names: {
      de: "USA",
      en: "United States",
      es: "Estados Unidos",
      fr: "États-Unis",
      ja: "アメリカ合衆国",
      "pt-BR": "Estados Unidos",
      ru: "США",
      "zh-CN": "美国",
    },
  };

  const city = {
    names: {
      en: "",
    },
  };

  const callback = onSuccess => {
    requestAnimationFrame(() => {
      onSuccess({
        city,
        continent,
        country,
        registered_country: country,
      });
    });
  };

  window.geoip2 = {
    country: callback,
    city: callback,
    insights: callback,
  };
}
