/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global loop, sinon */

var expect = chai.expect;

describe("loop.Client", function() {
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
    sandbox.restore();
  });

  describe("loop.Client", function() {
    describe("#constructor", function() {
      it("should require a baseApiUrl setting", function() {
        expect(function() {
          new loop.Client();
        }).to.Throw(Error, /required/);
      });
    });

    describe("#requestCallUrl", function() {
      var client;

      beforeEach(function() {
        client = new loop.Client({baseApiUrl: "http://fake.api"});
      });

      it("should request for a call url", function() {
        var callback = sinon.spy();
        client.requestCallUrl("fakeSimplepushUrl", callback);

        expect(requests).to.have.length.of(1);

        requests[0].respond(200, {"Content-Type": "application/json"},
                                 '{"call_url": "fakeCallUrl"}');
        sinon.assert.calledWithExactly(callback, null, "fakeCallUrl");
      });

      it("should send an error when the request fails", function() {
        var callback = sinon.spy();
        client.requestCallUrl("fakeSimplepushUrl", callback);

        expect(requests).to.have.length.of(1);

        requests[0].respond(400, {"Content-Type": "application/json"},
                                 '{"error": "my error"}');
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /HTTP error 400: Bad Request; my error/.test(err.message);
        }));
      });
    });

    describe("#requestCallsInfo", function() {
      var client;

      beforeEach(function() {
        client = new loop.Client({baseApiUrl: "http://fake.api"});
      });

      it("should request data for all calls", function() {
        var callback = sinon.spy();
        client.requestCallsInfo(callback);

        expect(requests).to.have.length.of(1);
        expect(requests[0].url).to.be.equal("http://fake.api/calls?version=0");
        expect(requests[0].method).to.be.equal("GET");

        requests[0].respond(200, {"Content-Type": "application/json"},
                                 '{"calls": [{"apiKey": "fake"}]}');
        sinon.assert.calledWithExactly(callback, null, [{apiKey: "fake"}]);
      });

      it("should send an error when the request fails", function() {
        var callback = sinon.spy();
        client.requestCallsInfo(callback);

        requests[0].respond(400, {"Content-Type": "application/json"},
                                 '{"error": "my error"}');
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /HTTP error 400: Bad Request; my error/.test(err.message);
        }));
      });

      it("should send an error if the data is not valid", function() {
        var callback = sinon.spy();
        client.requestCallsInfo(callback);

        requests[0].respond(200, {"Content-Type": "application/json"},
                                 '{"bad": {}}');
        sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
          return /Invalid calls data received/.test(err.message);
        }));
      });
    });
  });
});
