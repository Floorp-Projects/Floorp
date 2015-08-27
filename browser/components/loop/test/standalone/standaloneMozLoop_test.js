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
    fakeXHR.xhr.onCreate = function (xhr) {
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
      mozLoop.setLoopPref("fakePref", "fakeValue");

      expect(localStorage.getItem("fakePref")).eql("fakeValue");
    });
  });

  describe("#getLoopPref", function() {
    afterEach(function() {
      localStorage.removeItem("fakePref");
    });

    it("should return the value of the preference", function() {
      localStorage.setItem("fakePref", "fakeValue");

      expect(mozLoop.getLoopPref("fakePref")).eql("fakeValue");
    });
  });

  describe("#rooms.get", function() {
    it("should GET to the server", function() {
      mozLoop.rooms.get("fakeToken", callback);

      expect(requests).to.have.length.of(1);
      expect(requests[0].url).eql(fakeBaseServerUrl + "/rooms/fakeToken");
      expect(requests[0].method).eql("GET");
    });

    it("should call the callback with success parameters", function() {
      mozLoop.rooms.get("fakeToken", callback);

      var roomDetails = {
        roomName: "fakeName",
        roomUrl: "http://invalid"
      };

      requests[0].respond(200, {"Content-Type": "application/json"},
        JSON.stringify(roomDetails));

      sinon.assert.calledOnce(callback);
      sinon.assert.calledWithExactly(callback, null, roomDetails);
    });

    it("should call the callback with failure parameters", function() {
      mozLoop.rooms.get("fakeToken", callback);

      requests[0].respond(401, {"Content-Type": "application/json"},
                          JSON.stringify(fakeServerErrorDescription));
      sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
        return /HTTP 401 Unauthorized/.test(err.message);
      }));
    });
  });

  describe("#rooms.join", function() {
    it("should POST to the server", function() {
      mozLoop.rooms.join("fakeToken", callback);

      expect(requests).to.have.length.of(1);
      expect(requests[0].url).eql(fakeBaseServerUrl + "/rooms/fakeToken");
      expect(requests[0].method).eql("POST");

      var requestData = JSON.parse(requests[0].requestBody);
      expect(requestData.action).eql("join");
    });

    it("should call the callback with success parameters", function() {
      mozLoop.rooms.join("fakeToken", callback);

      var sessionData = {
        apiKey: "12345",
        sessionId: "54321",
        sessionToken: "another token",
        expires: 20
      };

      requests[0].respond(200, {"Content-Type": "application/json"},
        JSON.stringify(sessionData));

      sinon.assert.calledOnce(callback);
      sinon.assert.calledWithExactly(callback, null, sessionData);
    });

    it("should call the callback with failure parameters", function() {
      mozLoop.rooms.join("fakeToken", callback);

      requests[0].respond(401, {"Content-Type": "application/json"},
                          JSON.stringify(fakeServerErrorDescription));
      sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
        return /HTTP 401 Unauthorized/.test(err.message);
      }));
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
      standaloneMozLoop.rooms.refreshMembership("fakeToken", "fakeSessionToken",
                                                callback);

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
      standaloneMozLoop.rooms.refreshMembership("fakeToken", "fakeSessionToken",
                                                callback);

      var responseData = {
        expires: 20
      };

      requests[0].respond(200, {"Content-Type": "application/json"},
        JSON.stringify(responseData));

      sinon.assert.calledOnce(callback);
      sinon.assert.calledWithExactly(callback, null, responseData);
    });

    it("should call the callback with failure parameters", function() {
      standaloneMozLoop.rooms.refreshMembership("fakeToken", "fakeSessionToken",
                                                callback);

      requests[0].respond(401, {"Content-Type": "application/json"},
                          JSON.stringify(fakeServerErrDescription));
      sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
        return /HTTP 401 Unauthorized/.test(err.message);
      }));
    });
  });

  describe("#rooms.leave", function() {
    it("should POST to the server", function() {
      mozLoop.rooms.leave("fakeToken", "fakeSessionToken", callback);

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
      mozLoop.rooms.leave("fakeToken", "fakeSessionToken", callback);

      requests[0].respond(204);

      sinon.assert.calledOnce(callback);
      sinon.assert.calledWithExactly(callback, null, {});
    });

    it("should call the callback with failure parameters", function() {
      mozLoop.rooms.leave("fakeToken", "fakeSessionToken", callback);

      requests[0].respond(401, {"Content-Type": "application/json"},
                          JSON.stringify(fakeServerErrorDescription));
      sinon.assert.calledWithMatch(callback, sinon.match(function(err) {
        return /HTTP 401 Unauthorized/.test(err.message);
      }));
    });
  });
});
