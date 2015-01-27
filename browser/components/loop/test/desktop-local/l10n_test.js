/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var expect = chai.expect;

describe("document.mozL10n", function() {
  "use strict";

  var fakeMozLoop;

  beforeEach(function() {
    fakeMozLoop = {
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

    document.mozL10n.initialize(fakeMozLoop);
  });

  it("should get a simple string", function() {
    expect(document.mozL10n.get("test")).eql("test");
  });

  it("should get a plural form", function() {
    expect(document.mozL10n.get("plural", {num:10})).eql("10 plural forms");
  });
});
