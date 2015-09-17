/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.shared.models", function() {
  "use strict";

  var expect = chai.expect;
  var l10n = navigator.mozL10n;
  var sharedModels = loop.shared.models;
  var sandbox;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("NotificationCollection", function() {
    var collection, notifData, testNotif;

    beforeEach(function() {
      collection = new sharedModels.NotificationCollection();
      sandbox.stub(l10n, "get", function(x, y) {
        return "translated:" + x + (y ? ":" + y : "");
      });
      notifData = {level: "error", message: "plop"};
      testNotif = new sharedModels.NotificationModel(notifData);
    });

    describe("#warn", function() {
      it("should add a warning notification to the stack", function() {
        collection.warn("watch out");

        expect(collection).to.have.length.of(1);
        expect(collection.at(0).get("level")).eql("warning");
        expect(collection.at(0).get("message")).eql("watch out");
      });
    });

    describe("#warnL10n", function() {
      it("should warn using a l10n string id", function() {
        collection.warnL10n("fakeId");

        expect(collection).to.have.length.of(1);
        expect(collection.at(0).get("level")).eql("warning");
        expect(collection.at(0).get("message")).eql("translated:fakeId");
      });
    });

    describe("#error", function() {
      it("should add an error notification to the stack", function() {
        collection.error("wrong");

        expect(collection).to.have.length.of(1);
        expect(collection.at(0).get("level")).eql("error");
        expect(collection.at(0).get("message")).eql("wrong");
      });
    });

    describe("#errorL10n", function() {
      it("should notify an error using a l10n string id", function() {
        collection.errorL10n("fakeId");

        expect(collection).to.have.length.of(1);
        expect(collection.at(0).get("level")).eql("error");
        expect(collection.at(0).get("message")).eql("translated:fakeId");
      });

      it("should notify an error using a l10n string id + l10n properties",
        function() {
          collection.errorL10n("fakeId", "fakeProp");

          expect(collection).to.have.length.of(1);
          expect(collection.at(0).get("level")).eql("error");
          expect(collection.at(0).get("message")).eql("translated:fakeId:fakeProp");
      });
    });
  });
});
