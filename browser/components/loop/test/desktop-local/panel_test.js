/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop, sinon */

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

    describe("#getCallUrl", function() {
      it("should request a call url to the server", function() {
        var requestCallUrl = sandbox.stub(loop.shared.Client.prototype,
                                          "requestCallUrl");
        var view = new loop.panel.PanelView();

        view.getCallUrl({preventDefault: sandbox.spy()});

        sinon.assert.calledOnce(requestCallUrl);
        sinon.assert.calledWith(requestCallUrl, "foo");
      });

      it("should notify the user when the operation failed", function() {
        var requestCallUrl = sandbox.stub(
          loop.shared.Client.prototype, "requestCallUrl", function(_, cb) {
            cb("fake error");
          });
        var view = new loop.panel.PanelView();
        sandbox.stub(view.notifier, "notify");

        view.getCallUrl({preventDefault: sandbox.spy()});

        sinon.assert.calledOnce(view.notifier.notify);
        sinon.assert.calledWithMatch(view.notifier.notify, {level: "error"});
      });
    });

    describe("#onCallUrlReceived", function() {
      it("should update the text field with the call url", function() {
        var view = new loop.panel.PanelView();
        view.render();

        view.onCallUrlReceived("http://call.me/");

        expect(view.$("#call-url").val()).eql("http://call.me/");
      });

      it("should reset all pending notifications", function() {
        var view = new loop.panel.PanelView().render();
        sandbox.stub(view.notifier, "clear");

        view.onCallUrlReceived("http://call.me/");

        sinon.assert.calledOnce(view.notifier.clear, "clear");
      });
    });
  });
});
