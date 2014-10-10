/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */
/* jshint newcap:false */

var expect = chai.expect;

describe("loop.shared.utils", function() {
  "use strict";

  var sandbox;
  var sharedUtils = loop.shared.utils;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("Helper", function() {
    var helper;

    beforeEach(function() {
      helper = new sharedUtils.Helper();
    });

    describe("#isIOS", function() {
      it("should detect iOS", function() {
        expect(helper.isIOS("iPad")).eql(true);
        expect(helper.isIOS("iPod")).eql(true);
        expect(helper.isIOS("iPhone")).eql(true);
        expect(helper.isIOS("iPhone Simulator")).eql(true);
      });

      it("shouldn't detect iOS with other platforms", function() {
        expect(helper.isIOS("MacIntel")).eql(false);
      });
    });

    describe("#isFirefox", function() {
      it("should detect Firefox", function() {
        expect(helper.isFirefox("Firefox")).eql(true);
        expect(helper.isFirefox("Gecko/Firefox")).eql(true);
        expect(helper.isFirefox("Firefox/Gecko")).eql(true);
        expect(helper.isFirefox("Gecko/Firefox/Chuck Norris")).eql(true);
      });

      it("shouldn't detect Firefox with other platforms", function() {
        expect(helper.isFirefox("Opera")).eql(false);
      });
    });

    describe("#isFirefoxOS", function() {
      describe("without mozActivities", function() {
        it("shouldn't detect FirefoxOS on mobile platform", function() {
          expect(helper.isFirefoxOS("mobi")).eql(false);
        });

        it("shouldn't detect FirefoxOS on non mobile platform", function() {
          expect(helper.isFirefoxOS("whatever")).eql(false);
        });
      });

      describe("with mozActivities", function() {
        var realMozActivity;

        before(function() {
          realMozActivity = window.MozActivity;
          window.MozActivity = {};
        });

        after(function() {
          window.MozActivity = realMozActivity;
        });

        it("should detect FirefoxOS on mobile platform", function() {
          expect(helper.isFirefoxOS("mobi")).eql(true);
        });

        it("shouldn't detect FirefoxOS on non mobile platform", function() {
          expect(helper.isFirefoxOS("whatever")).eql(false);
        });
      });
    });
  });

  describe("#formatDate", function() {
    beforeEach(function() {
      sandbox.stub(Date.prototype, "toLocaleDateString").returns("fake result");
    });

    it("should call toLocaleDateString with arguments", function() {
      sharedUtils.formatDate(1000);

      sinon.assert.calledOnce(Date.prototype.toLocaleDateString);
      sinon.assert.calledWithExactly(Date.prototype.toLocaleDateString,
        navigator.language,
        {year: "numeric", month: "long", day: "numeric"}
      );
    });

    it("should return the formatted string", function() {
      expect(sharedUtils.formatDate(1000)).eql("fake result");
    });
  });

  describe("#getBoolPreference", function() {
    afterEach(function() {
      navigator.mozLoop = undefined;
      localStorage.removeItem("test.true");
    });

    describe("mozLoop set", function() {
      beforeEach(function() {
        navigator.mozLoop = {
          getLoopBoolPref: function(prefName) {
            return prefName === "test.true";
          }
        };
      });

      it("should return the mozLoop preference", function() {
        expect(sharedUtils.getBoolPreference("test.true")).eql(true);
      });

      it("should not use the localStorage value", function() {
        localStorage.setItem("test.false", true);

        expect(sharedUtils.getBoolPreference("test.false")).eql(false);
      });
    });

    describe("mozLoop not set", function() {
      it("should return the localStorage value", function() {
        localStorage.setItem("test.true", true);

        expect(sharedUtils.getBoolPreference("test.true")).eql(true);
      });
    });
  });
});
