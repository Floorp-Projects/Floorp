/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.shared.router", function() {
  "use strict";

  var sandbox;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("BaseRouter", function() {
    var router;

    beforeEach(function() {
      router = new loop.shared.router.BaseRouter();
    });

    describe("#loadView", function() {
      it("should set the active view", function() {
        var TestView = loop.shared.views.BaseView.extend({});
        var view = new TestView();

        router.loadView(view);

        expect(router.activeView).eql(view);
      });
    });
  });
});
