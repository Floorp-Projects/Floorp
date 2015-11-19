/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var expect = chai.expect;

describe("document.mozL10n", function() {
  "use strict";

  var l10nOptions;

  beforeEach(function() {
    l10nOptions = {
      locale: "en-US",
      getStrings: function(key) {
        if (key === "plural") {
          return '{"textContent":"{{num}} plural form;{{num}} plural forms"}';
        }

        return '{"textContent":"' + key + '"}';
      },
      getPluralForm: function(num, string) {
        return string.split(";")[num === 0 ? 0 : 1];
      }
    };

    document.mozL10n.initialize(l10nOptions);
  });

  it("should get a simple string", function() {
    expect(document.mozL10n.get("test")).eql("test");
  });

  it("should get a plural form", function() {
    expect(document.mozL10n.get("plural", { num: 10 })).eql("10 plural forms");
  });

  it("should correctly get a plural form for num = 0", function() {
    expect(document.mozL10n.get("plural", { num: 0 })).eql("0 plural form");
  });
});
