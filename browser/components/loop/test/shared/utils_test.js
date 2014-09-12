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
