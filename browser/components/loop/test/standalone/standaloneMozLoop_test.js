/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

describe("loop.StandaloneMozLoop", function() {
  "use strict";

  var expect = chai.expect;
  var sandbox, fakeXHR, requests, callback, mozLoop;
  var fakeToken, fakeBaseServerUrl, fakeServerErrorDescription;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    fakeXHR = sandbox.useFakeXMLHttpRequest();
    requests = [];
    // https://github.com/cjohansen/Sinon.JS/issues/393
    fakeXHR.xhr.onCreate = function(xhr) {
      requests.push(xhr);
    };
    fakeBaseServerUrl = "http://fake.api";
    fakeServerErrorDescription = {
      code: 401,
      errno: 101,
      error: "error",
      message: "invalid token",
      info: "error info"
    };

    callback = sinon.spy();

    mozLoop = new loop.StandaloneMozLoop({
      baseServerUrl: fakeBaseServerUrl
    });
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#constructor", function() {
    it("should require a baseServerUrl setting", function() {
      expect(function() {
        new loop.StandaloneMozLoop();
      }).to.Throw(Error, /required/);
    });
  });

  describe("#setLoopPref", function() {
    afterEach(function() {
      localStorage.removeItem("fakePref");
    });

    it("should store the value of the preference", function() {
      loop.request("SetLoopPref", "fakePref", "fakeValue");

      expect(localStorage.getItem("fakePref")).eql("fakeValue");
    });
  });

  describe("#getLoopPref", function() {
    afterEach(function() {
      localStorage.removeItem("fakePref");
    });

    it("should return the value of the preference", function() {
      localStorage.setItem("fakePref", "fakeValue");

      return loop.request("GetLoopPref", "fakePref").then(function(result) {
        expect(result).eql("fakeValue");
      });
    });
  });

  describe("#rooms.get", function() {
    it("should GET to the server", function() {
      loop.request("Rooms:Get", "fakeToken");

      expect(requests).to.have.length.of(1);
      expect(requests[0].url).eql(fakeBaseServerUrl + "/rooms/fakeToken");
      expect(requests[0].method).eql("GET");
    });

    it("should call the callback with success parameters", function() {
      var promise = loop.request("Rooms:Get", "fakeToken").then(function(result) {
        expect(result).eql(roomDetails);
      });

      var roomDetails = {
        roomName: "fakeName",
        roomUrl: "http://invalid"
      };

      requests[0].respond(200, { "Content-Type": "application/json" },
        JSON.stringify(roomDetails));

      return promise;
    });

    it("should call the callback with failure parameters", function() {
      var promise = loop.request("Rooms:Get", "fakeToken").then(function(result) {
        expect(result.isError).eql(true);
        expect(/HTTP 401 Unauthorized/.test(result.message)).eql(true);
      });

      requests[0].respond(401, { "Content-Type": "application/json" },
                          JSON.stringify(fakeServerErrorDescription));

      return promise;
    });
  });

  describe("#rooms.join", function() {
    it("should POST to the server", function() {
      loop.request("Rooms:Join", "fakeToken");

      expect(requests).to.have.length.of(1);
      expect(requests[0].url).eql(fakeBaseServerUrl + "/rooms/fakeToken");
      expect(requests[0].method).eql("POST");

      var requestData = JSON.parse(requests[0].requestBody);
      expect(requestData.action).eql("join");
    });

    it("should call the callback with success parameters", function() {
      var promise = loop.request("Rooms:Join", "fakeToken").then(function(result) {
        expect(result).eql(sessionData);
      });

      var sessionData = {
        apiKey: "12345",
        sessionId: "54321",
        sessionToken: "another token",
        expires: 20
      };

      requests[0].respond(200, { "Content-Type": "application/json" },
        JSON.stringify(sessionData));

      return promise;
    });

    it("should call the callback with failure parameters", function() {
      var promise = loop.request("Rooms:Join", "fakeToken").then(function(result) {
        expect(result.isError).eql(true);
        expect(/HTTP 401 Unauthorized/.test(result.message)).eql(true);
      });

      requests[0].respond(401, { "Content-Type": "application/json" },
                          JSON.stringify(fakeServerErrorDescription));

      return promise;
    });
  });

  describe("#rooms.refreshMembership", function() {
    var standaloneMozLoop, fakeServerErrDescription;

    beforeEach(function() {
      standaloneMozLoop = new loop.StandaloneMozLoop({
        baseServerUrl: fakeBaseServerUrl
      });

      fakeServerErrDescription = {
        code: 401,
        errno: 101,
        error: "error",
        message: "invalid token",
        info: "error info"
      };
    });

    it("should POST to the server", function() {
      loop.request("Rooms:RefreshMembership", "fakeToken", "fakeSessionToken");

      expect(requests).to.have.length.of(1);
      expect(requests[0].url).eql(fakeBaseServerUrl + "/rooms/fakeToken");
      expect(requests[0].method).eql("POST");
      expect(requests[0].requestHeaders.Authorization)
        .eql("Basic " + btoa("fakeSessionToken"));

      var requestData = JSON.parse(requests[0].requestBody);
      expect(requestData.action).eql("refresh");
      expect(requestData.sessionToken).eql("fakeSessionToken");
    });

    it("should call the callback with success parameters", function() {
      var promise = loop.request("Rooms:RefreshMembership", "fakeToken",
        "fakeSessionToken").then(function(result) {
          expect(result).eql(responseData);
        });

      var responseData = {
        expires: 20
      };

      requests[0].respond(200, { "Content-Type": "application/json" },
        JSON.stringify(responseData));

      return promise;
    });

    it("should call the callback with failure parameters", function() {
      var promise = loop.request("Rooms:RefreshMembership", "fakeToken",
        "fakeSessionToken").then(function(result) {
          expect(result.isError).eql(true);
          expect(/HTTP 401 Unauthorized/.test(result.message)).eql(true);
        });

      requests[0].respond(401, { "Content-Type": "application/json" },
                          JSON.stringify(fakeServerErrDescription));

      return promise;
    });
  });

  describe("#rooms.leave", function() {
    it("should POST to the server", function() {
      loop.request("Rooms:Leave", "fakeToken", "fakeSessionToken");

      expect(requests).to.have.length.of(1);
      expect(requests[0].async).eql(false);
      expect(requests[0].url).eql(fakeBaseServerUrl + "/rooms/fakeToken");
      expect(requests[0].method).eql("POST");
      expect(requests[0].requestHeaders.Authorization)
        .eql("Basic " + btoa("fakeSessionToken"));

      var requestData = JSON.parse(requests[0].requestBody);
      expect(requestData.action).eql("leave");
    });

    it("should call the callback with success parameters", function() {
      var promise = loop.request("Rooms:Leave", "fakeToken", "fakeSessionToken")
        .then(function(result) {
          expect(result).eql({});
        });

      requests[0].respond(204);

      return promise;
    });

    it("should call the callback with failure parameters", function() {
      var promise = loop.request("Rooms:Leave", "fakeToken", "fakeSessionToken")
        .then(function(result) {
          expect(result.isError).eql(true);
          expect(/HTTP 401 Unauthorized/.test(result.message)).eql(true);
        });

      requests[0].respond(401, { "Content-Type": "application/json" },
                          JSON.stringify(fakeServerErrorDescription));

      return promise;
    });
  });
});
