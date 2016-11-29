/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test Amazon search plugin URLs
 */

"use strict";

const BROWSER_SEARCH_PREF = "browser.search.";

function test() {
  let engine = Services.search.getEngineByName("Amazon.com");
  ok(engine, "Amazon.com");

  let base = "https://www.amazon.com/exec/obidos/external-search/?field-keywords=foo&ie=UTF-8&mode=blended&tag=mozilla-20&sourceid=Mozilla-search";
  let url;

  // Test search URLs (including purposes).
  url = engine.getSubmission("foo").uri.spec;
  is(url, base, "Check search URL for 'foo'");

  // Check search suggestion URL.
  url = engine.getSubmission("foo", "application/x-suggestions+json").uri.spec;
  is(url, "https://completion.amazon.com/search/complete?q=foo&search-alias=aps&mkt=1", "Check search suggestion URL for 'foo'");

  // Check all other engine properties.
  const EXPECTED_ENGINE = {
    name: "Amazon.com",
    alias: null,
    description: "Amazon.com Search",
    searchForm: "https://www.amazon.com/exec/obidos/external-search/?field-keywords=&ie=UTF-8&mode=blended&tag=mozilla-20&sourceid=Mozilla-search",
    hidden: false,
    wrappedJSObject: {
      queryCharset: "UTF-8",
      "_iconURL": "data:image/x-icon;base64,AAABAAIAEBAAAAAAAAC0AQAAJgAAACAgAAAAAAAA6QIAANoBAACJUE5HDQoaCgAAAA1JSERSAAAAEAAAABAIBgAAAB/z/2EAAAF7SURBVDjLlZPLasJAFIaFRF+iVV+h6hO0GF+gVB9AaHwDt64qCG03tQgtdCFIuyhUelmGli66MXThSt24kNiFBUlAYi6ezjnNxSuawB/ITP7v/HNmJgQAEaZzpgHs/gwcTyTEXuXl2U6nA8ViEbK5HKler28CVRAwnB9ptVrAh8MrQuCaZ4iA8fzIqSgCxwzpTIaSuN/RWGwdYLwCUBQFZFkGSZLgqdmEE7YEN8VOAKyaSKUW4nNBAFmnYiKZpDRX1WqwBBzP089n5f/NEQsFL4WqqtsBWJlzDAJr5PwSMM1awEzzdxIbGI3Hvc6jCZeVFgRQRwpY7Qcw3ktgfpR8wLRxCPaot/X4GS95MppfF6DX9n2A3f+kAZycaT8bAZjU6r6B/duD6d3BYg9wQq/tkYzHY1blEiz5lmQyGc95mrO6r2CxgpjCBXgNsJVviolpXJiraeOIjJRE10juUa4sR8V+mO17VvmGqtuOcdNlwut8zTQJcJ0njifyB2bgTdKh6w4BAAAAAElFTkSuQmCCiVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAACsElEQVRYw71XQWsTURBe2LQgeNKLB+tVemt6txcteNSD/QGC6VEIGDx5s+eKPQqFgJhLNdFLBWMP7cU0oSAWjB70koC9WHbVQ5SO8+XtS14mr7svyaYDH9m87Jv55puZt1nPi4yIzjMeMj7T9OwjI88455nGC1cZX+nsDESumJmPFDwIAqrX6z00Gg1qt9vjkJgFgUeuO16Vy3RjeZkyMzM9+MY1fsM9I9h9zyV7ZAznZrA4FAoFVwJ1z+WuOysrg1lnMolkHJX4k0igzI5sARYWF7vEZEk0rvO6iyUSuJfLJUqM7zYSqRDIra4OOUZPmNZsNrsl8UVTpkJAjh1GzmaSpJ8mAWmYeZB5urHRhW5SNOfUCCDo47W1bvPZsp2qAhipy3Nz1kaLG8dUCEBqM5AvpgElqFar01NgIZsdco7Zb7VasU2YigIYL5tjqCL7Q5YkFQXKlcqQ7DbHthIALk/IWAKor82xPIhshxWABCYioDMz51sexcVi0XoG4DPLIyvJjkTArK3scDQnRvO0MdTrUHGiKZCP4tNgO6BAEI08EQH9Z2Qow0hyPypJGIa9p6JWKCn4SA8jSKmJIDgyRvPJkcRxjfUwNGr/i8+Mo32iHzWiThBD4NM60bet9P77/ubA728RlTjMiwiH6zEEfvIrwdZFtQmMJ7W/ofIDBZD5m3mVZGwJcOP2kmILIlCkE45HoPWurwCSg0+UQRD4ZyXxId+T7gQb9+4q9sioY5ltrOG3L5vqXiiJffDx/aUi83ZJ7jr2ohcEu8Hh6/m+I7OWGiVxbWKHsz+O3vSOakqFQdsFgQeJUiKD7Wv9YKXBgCeSUC3v2kM5EJhlHDh3NcgcPlG1BXZu98sDmTuBa4fsMnz9fniJUaGzs+eMC540XuR0aDO2L8Y3qPyMcdOM+R/8XcqRA3qp9gAAAABJRU5ErkJggg==",
      _urls : [
        {
          type: "application/x-suggestions+json",
          method: "GET",
          template: "https://completion.amazon.com/search/complete?q={searchTerms}&search-alias=aps&mkt=1",
          params: "",
        },
        {
          type: "text/html",
          method: "GET",
          template: "https://www.amazon.com/exec/obidos/external-search/",
          params: [
            {
              name: "field-keywords",
              value: "{searchTerms}",
              purpose: undefined,
            },
            {
              name: "ie",
              value: "{inputEncoding}",
              purpose: undefined,
            },
            {
              name: "mode",
              value: "blended",
              purpose: undefined,
            },
            {
              name: "tag",
              value: "mozilla-20",
              purpose: undefined,
            },
            {
              name: "sourceid",
              value: "Mozilla-search",
              purpose: undefined,
            },
          ],
          mozparams: {},
        },
      ],
    },
  };

  isSubObjectOf(EXPECTED_ENGINE, engine, "Amazon");
}
