/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop, sinon, it, beforeEach, afterEach, describe*/

var expect = chai.expect;

describe("loop.standaloneMedia._MultiplexGum", function() {
  "use strict";

  var defaultGum =
    navigator.getUserMedia ||
    navigator.mozGetUserMedia ||
    navigator.webkitGetUserMedia ||
    (window["TBPlugin"] && TBPlugin.getUserMedia);

  var sandbox;
  var multiplexGum;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    multiplexGum = new loop.standaloneMedia._MultiplexGum();
    loop.standaloneMedia.setSingleton(multiplexGum);
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#constructor", function() {
    it("pending should default to false", function() {
      expect(multiplexGum.userMedia.pending).to.equal(false);
    });
  });

  describe("default getUserMedia", function() {
    it("should call getPermsAndCacheMedia", function() {
      var fakeOptions = {audio: true, video: true};
      var successCB = function() {};
      var errorCB = function() {};
      sandbox.stub(navigator, "originalGum");
      sandbox.stub(loop.standaloneMedia._MultiplexGum.prototype,
        "getPermsAndCacheMedia");
      multiplexGum = new loop.standaloneMedia._MultiplexGum();

      defaultGum(fakeOptions, successCB, errorCB);

      sinon.assert.calledOnce(multiplexGum.getPermsAndCacheMedia);
      sinon.assert.calledWithExactly(multiplexGum.getPermsAndCacheMedia,
        fakeOptions, successCB, errorCB);
    });
  });

  describe("#getPermsAndCacheMedia", function() {
    beforeEach(function() {
      sandbox.stub(navigator, "originalGum");
    });

    it("should change pending to true", function() {
      multiplexGum.getPermsAndCacheMedia();

      expect(multiplexGum.userMedia.pending).to.equal(true);
    });

    it("should call originalGum", function() {
      multiplexGum.getPermsAndCacheMedia();

      sinon.assert.calledOnce(navigator.originalGum);
    });

    it("should reset the pending state when the error callback is called",
      function(done) {
        var fakeError = new Error();
        navigator.originalGum.callsArgWith(2, fakeError);

        multiplexGum.getPermsAndCacheMedia(null, null, function onError(error) {
          expect(multiplexGum.userMedia.pending).to.equal(false);
          done();
        });
      });

    it("should reset the pending state when the success callback is called",
      function(done) {
        var fakeLocalStream = {};
        navigator.originalGum.callsArgWith(1, fakeLocalStream);

        multiplexGum.getPermsAndCacheMedia(null,
          function onSuccess(localStream) {
            expect(multiplexGum.userMedia.pending).to.equal(false);
            done();
          }, null);
      });

    it("should call the error callback when originalGum calls back an error",
      function(done) {
        var fakeError = new Error();
        navigator.originalGum.callsArgWith(2, fakeError);

        multiplexGum.getPermsAndCacheMedia(null, null, function onError(error) {
          expect(error).to.eql(fakeError);
          done();
        });
      });

    it("should propagate the success callback when originalGum succeeds",
      function(done) {
        var fakeLocalStream = {};
        navigator.originalGum.callsArgWith(1, fakeLocalStream);

        multiplexGum.getPermsAndCacheMedia(null,
          function onSuccess(localStream) {
            expect(localStream).to.eql(fakeLocalStream);
            done();
          }, null);
      });

    it("should call the success callback when the stream is cached",
      function(done) {
        var fakeLocalStream = {};
        multiplexGum.userMedia.localStream = fakeLocalStream;
        sinon.assert.notCalled(navigator.originalGum);

        multiplexGum.getPermsAndCacheMedia(null,
          function onSuccess(localStream) {
            expect(localStream).to.eql(fakeLocalStream);
            done();
          }, null);
      });

    it("should call the error callback when an error is cached",
      function(done) {
        var fakeError = new Error();
        multiplexGum.userMedia.error = fakeError;
        sinon.assert.notCalled(navigator.originalGum);

        multiplexGum.getPermsAndCacheMedia(null, null, function onError(error) {
          expect(error).to.eql(fakeError);
          done();
        });
      });

    it("should clear the error when success is called back", function(done) {
      var fakeError = new Error();
      var fakeLocalStream = {};
      multiplexGum.userMedia.localStream = fakeLocalStream;
      multiplexGum.userMedia.error = fakeError;

      multiplexGum.getPermsAndCacheMedia(null, function onSuccess(localStream) {
        expect(multiplexGum.userMedia.error).to.not.eql(fakeError);
        expect(localStream).to.eql(fakeLocalStream);
        done();
      }, null);
    });

    it("should call all success callbacks when success is achieved",
      function(done) {
        var fakeLocalStream = {};
        var calls = 0;
        // Async is needed so that the callbacks can be queued up.
        navigator.originalGum.callsArgWithAsync(1, fakeLocalStream);

        multiplexGum.getPermsAndCacheMedia(null, function onSuccess(localStream) {
          calls += 1;
          expect(localStream).to.eql(fakeLocalStream);
        }, null);

        expect(multiplexGum.userMedia).to.have.property('pending', true);

        multiplexGum.getPermsAndCacheMedia(null, function onSuccess(localStream) {
          calls += 10;
          expect(localStream).to.eql(fakeLocalStream);
          expect(calls).to.equal(11);
          done();
        }, null);
      });

    it("should call all error callbacks when error is encountered",
      function(done) {
        var fakeError = new Error();
        var calls = 0;
        // Async is needed so that the callbacks can be queued up.
        navigator.originalGum.callsArgWithAsync(2, fakeError);

        multiplexGum.getPermsAndCacheMedia(null, null, function onError(error) {
          calls += 1;
          expect(error).to.eql(fakeError);
        });

        expect(multiplexGum.userMedia).to.have.property('pending', true);

        multiplexGum.getPermsAndCacheMedia(null, null, function onError(error) {
          calls += 10;
          expect(error).to.eql(fakeError);
          expect(calls).to.eql(11);
          done();
        });
      });

    it("should not call a getPermsAndCacheMedia success callback at the time" +
       " of gUM success callback fires",
      function(done) {
        var fakeLocalStream = {};
        multiplexGum.userMedia.localStream = fakeLocalStream;
        navigator.originalGum.callsArgWith(1, fakeLocalStream);
        var calledOnce = false;
        var promiseCalledOnce = new Promise(function(resolve, reject) {

          multiplexGum.getPermsAndCacheMedia(null,
            function gPACMSuccess(localStream) {
              expect(localStream).to.eql(fakeLocalStream);
              expect(multiplexGum.userMedia).to.have.property('pending', false);
              expect(multiplexGum.userMedia.successCallbacks.length).to.equal(0);
              if (calledOnce) {
                sinon.assert.fail("original callback was called twice");
              }
              calledOnce = true;
              resolve();
            }, function() {
              sinon.assert.fail("error callback should not have fired");
              reject();
              done();
            });
        });

        promiseCalledOnce.then(function() {
          defaultGum(null, function gUMSuccess(localStream2) {
            expect(localStream2).to.eql(fakeLocalStream);
            expect(multiplexGum.userMedia).to.have.property('pending', false);
            expect(multiplexGum.userMedia.successCallbacks.length).to.equal(0);
            done();
          });
        });
      });

    it("should not call a getPermsAndCacheMedia error callback when the " +
      " gUM error callback fires",
      function(done) {
        var fakeError = "monkeys ate the stream";
        multiplexGum.userMedia.error = fakeError;
        navigator.originalGum.callsArgWith(2, fakeError);
        var calledOnce = false;
        var promiseCalledOnce = new Promise(function(resolve, reject) {
          multiplexGum.getPermsAndCacheMedia(null, function() {
            sinon.assert.fail("success callback should not have fired");
            reject();
            done();
          }, function gPACMError(errString) {
            expect(errString).to.eql(fakeError);
            expect(multiplexGum.userMedia).to.have.property('pending', false);
            if (calledOnce) {
              sinon.assert.fail("original error callback was called twice");
            }
            calledOnce = true;
            resolve();
          });
        });

        promiseCalledOnce.then(function() {
          defaultGum(null, function() {},
            function gUMError(errString) {
              expect(errString).to.eql(fakeError);
              expect(multiplexGum.userMedia).to.have.property('pending', false);
              done();
            });
        });
      });

    it("should call the success callback with a new stream, " +
       " when a new stream is available",
      function(done) {
        var endedStream = {ended: true};
        var newStream = {};
        multiplexGum.userMedia.localStream = endedStream;
        navigator.originalGum.callsArgWith(1, newStream);

        multiplexGum.getPermsAndCacheMedia(null, function onSuccess(localStream) {
          expect(localStream).to.eql(newStream);
          done();
        }, null);
      });
  });

  describe("#reset", function () {
    it("should reset all userMedia state to default", function() {
      // If userMedia is defined, then it needs to have all of
      // the properties that multipleGum will depend on. It is
      // easier to simply delete the object than to setup a fake
      // state of the object.
      delete multiplexGum.userMedia;

      multiplexGum.reset();

      expect(multiplexGum.userMedia).to.deep.equal({
          error: null,
          localStream: null,
          pending: false,
          errorCallbacks: [],
          successCallbacks: [],
      });
    });

    it("should call all queued error callbacks with 'PERMISSION_DENIED'",
      function(done) {
        sandbox.stub(navigator, "originalGum");
        multiplexGum.getPermsAndCacheMedia(null, function(localStream) {
          sinon.assert.fail(
            "The success callback shouldn't be called due to reset");
        }, function(error) {
          expect(error).to.equal("PERMISSION_DENIED");
          done();
        });
        multiplexGum.reset();
      });

    it("should call MST.stop() on the stream tracks", function() {
      var stopStub = sandbox.stub();
      multiplexGum.userMedia.localStream = {stop: stopStub};

      multiplexGum.reset();

      sinon.assert.calledOnce(stopStub);
    });

    it("should not call MST.stop() on the stream tracks if .stop() doesn't exist",
      function() {
        multiplexGum.userMedia.localStream = {};

        try {
          multiplexGum.reset();
        } catch (ex) {
          sinon.assert.fail(
            "reset shouldn't throw when a stream doesn't implement stop(): "
            + ex);
        }
      });

    it("should not get stuck in recursion if the error callback calls 'reset'",
      function() {
        sandbox.stub(navigator, "originalGum");
        navigator.originalGum.callsArgWith(2, "PERMISSION_DENIED");

        var calledOnce = false;
        multiplexGum.getPermsAndCacheMedia(null, null, function() {
          if (calledOnce) {
            sinon.assert.fail("reset should only be called once");
          }
          calledOnce = true;
          multiplexGum.reset.bind(multiplexGum)();
        });
      });

    it("should not get stuck in recursion if the success callback calls 'reset'",
      function() {
        sandbox.stub(navigator, "originalGum");
        navigator.originalGum.callsArgWith(1, {});

        var calledOnce = false;
        multiplexGum.getPermsAndCacheMedia(null, function() {
          calledOnce = true;
          multiplexGum.reset.bind(multiplexGum)();
        }, function() {
          if (calledOnce) {
            sinon.assert.fail("reset should only be called once");
          }
          calledOnce = true;
        });
      });
  });
});
