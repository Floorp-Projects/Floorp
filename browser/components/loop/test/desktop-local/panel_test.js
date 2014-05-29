/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.panel", function() {
  "use strict";

  var sandbox, fakeXHR, requests = [];

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    fakeXHR = sandbox.useFakeXMLHttpRequest();
    requests = [];
    // https://github.com/cjohansen/Sinon.JS/issues/393
    fakeXHR.xhr.onCreate = function (xhr) {
      requests.push(xhr);
    };
  });

  afterEach(function() {
    $("#fixtures").empty();
    sandbox.restore();
  });

  describe("loop.panel.NotificationView", function() {
    describe("#render", function() {
      it("should render template with model attribute values", function() {
        var view = new loop.panel.NotificationView({
          el: $("#fixtures"),
          model: new loop.panel.NotificationModel({
            level: "error",
            message: "plop"
          })
        });

        view.render();

        expect(view.$(".message").text()).eql("plop");
      });
    });
  });

  describe("loop.panel.NotificationListView", function() {
    describe("Collection events", function() {
      var coll, testNotif, view;

      beforeEach(function() {
        sandbox.stub(loop.panel.NotificationListView.prototype, "render");
        testNotif = new loop.panel.NotificationModel({
          level: "error",
          message: "plop"
        });
        coll = new loop.panel.NotificationCollection();
        view = new loop.panel.NotificationListView({collection: coll});
      });

      it("should render on notification added to the collection", function() {
        coll.add(testNotif);

        sinon.assert.calledOnce(view.render);
      });

      it("should render on notification removed from the collection",
        function() {
          coll.add(testNotif);
          coll.remove(testNotif);

          sinon.assert.calledTwice(view.render);
        });

      it("should render on collection reset",
        function() {
          coll.reset();

          sinon.assert.calledOnce(view.render);
        });
    });
  });

  describe("loop.panel.PanelView", function() {
    beforeEach(function() {
      $("#fixtures").append([
        '<div id="default-view" class="share generate-url">',
        '  <div class="description">',
        '    <p>Get a link to share with a friend to Video Chat.</p>',
        '  </div>',
        '  <div class="action">',
        '    <div class="messages"></div>',
        '    <p class="invite">',
        '      <input type="text" placeholder="Nickname of the future caller"',
        '             name="caller" value="foo"/>',
        '      <a class="get-url" href="">Get a call url</a>',
        '    </p>',
        '    <p class="result hide">',
        '      <input id="call-url" type="url" readonly>',
        '      <a class="get-url" href="">Renew</a>',
        '    </p>',
        '  </div>',
        '</div>'
      ].join(""));
    });

    describe("#getCallurl", function() {
      it("should request a call url to the server", function() {
        var requestCallUrl = sandbox.stub(loop.shared.Client.prototype,
                                          "requestCallUrl");
        var view = new loop.panel.PanelView();

        view.getCallUrl({preventDefault: sandbox.spy()});

        sinon.assert.calledOnce(requestCallUrl);
        sinon.assert.calledWith(requestCallUrl, "foo");
      });
    });

    describe("#onCallUrlReceived", function() {
      it("should update the text field with the call url", function() {
        var view = new loop.panel.PanelView();
        view.render();

        view.onCallUrlReceived("http://call.me/");

        expect(view.$("#call-url").val()).eql("http://call.me/");
      });
    });
  });
});
