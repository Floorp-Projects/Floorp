/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*global loop, sinon */

var expect = chai.expect;
var TestUtils = React.addons.TestUtils;

describe("loop.panel", function() {
  "use strict";

  var sandbox, notifier, fakeXHR, requests = [];

  function createTestRouter(fakeDocument) {
    return new loop.panel.PanelRouter({
      notifier: notifier,
      document: fakeDocument
    });
  }

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    fakeXHR = sandbox.useFakeXMLHttpRequest();
    requests = [];
    // https://github.com/cjohansen/Sinon.JS/issues/393
    fakeXHR.xhr.onCreate = function (xhr) {
      requests.push(xhr);
    };
    notifier = {
      clear: sandbox.spy(),
      notify: sandbox.spy(),
      warn: sandbox.spy(),
      warnL10n: sandbox.spy(),
      error: sandbox.spy(),
      errorL10n: sandbox.spy()
    };

    navigator.mozLoop = {
      doNotDisturb: true,
      get serverUrl() {
        return "http://example.com";
      },
      getStrings: function() {
        return JSON.stringify({textContent: "fakeText"});
      },
      get locale() {
        return "en-US";
      },
      setLoopCharPref: sandbox.stub(),
      getLoopCharPref: sandbox.stub()
    };

    document.mozL10n.initialize(navigator.mozLoop);
  });

  afterEach(function() {
    delete navigator.mozLoop;
    sandbox.restore();
  });

  describe("loop.panel.PanelRouter", function() {
    describe("#constructor", function() {
      it("should require a notifier", function() {
        expect(function() {
          new loop.panel.PanelRouter();
        }).to.Throw(Error, /missing required notifier/);
      });

      it("should require a document", function() {
        expect(function() {
          new loop.panel.PanelRouter({notifier: notifier});
        }).to.Throw(Error, /missing required document/);
      });
    });

    describe("constructed", function() {
      var router;

      beforeEach(function() {
        router = createTestRouter({
          hidden: true,
          addEventListener: sandbox.spy()
        });

        sandbox.stub(router, "loadView");
        sandbox.stub(router, "loadReactComponent");
      });

      describe("#home", function() {
        it("should reset the PanelView", function() {
          sandbox.stub(router, "reset");

          router.home();

          sinon.assert.calledOnce(router.reset);
        });
      });

      describe("#reset", function() {
        it("should clear all pending notifications", function() {
          router.reset();

          sinon.assert.calledOnce(notifier.clear);
        });

        it("should load the home view", function() {
          router.reset();

          sinon.assert.calledOnce(router.loadReactComponent);
          sinon.assert.calledWithExactly(router.loadReactComponent,
            sinon.match(function(value) {
              return React.addons.TestUtils.isComponentOfType(
                value, loop.panel.PanelView);
            }));
        });
      });
    });

    describe("Events", function() {
      beforeEach(function() {
        sandbox.stub(loop.panel.PanelRouter.prototype, "trigger");
      });

      it("should listen to document visibility changes", function() {
        var fakeDocument = {
          hidden: true,
          addEventListener: sandbox.spy()
        };

        var router = createTestRouter(fakeDocument);

        sinon.assert.calledOnce(fakeDocument.addEventListener);
        sinon.assert.calledWith(fakeDocument.addEventListener,
                                "visibilitychange");
      });

      it("should trigger panel:open when the panel document is visible",
        function() {
          var router = createTestRouter({
            hidden: false,
            addEventListener: function(name, cb) {
              cb({currentTarget: {hidden: false}});
            }
          });

          sinon.assert.calledOnce(router.trigger);
          sinon.assert.calledWithExactly(router.trigger, "panel:open");
        });

      it("should trigger panel:closed when the panel document is hidden",
        function() {
          var router = createTestRouter({
            hidden: true,
            addEventListener: function(name, cb) {
              cb({currentTarget: {hidden: true}});
            }
          });

          sinon.assert.calledOnce(router.trigger);
          sinon.assert.calledWithExactly(router.trigger, "panel:closed");
        });
    });
  });

  describe("loop.panel.DoNotDisturb", function() {
    var view;

    beforeEach(function() {
      view = TestUtils.renderIntoDocument(loop.panel.DoNotDisturb());
    });

    describe("Checkbox change event", function() {
      beforeEach(function() {
        navigator.mozLoop.doNotDisturb = false;

        var checkbox = TestUtils.findRenderedDOMComponentWithTag(view, "input");
        TestUtils.Simulate.change(checkbox);
      });

      it("should toggle the value of mozLoop.doNotDisturb", function() {
        expect(navigator.mozLoop.doNotDisturb).eql(true);
      });

      it("should update the DnD checkbox value", function() {
        expect(view.getDOMNode().querySelector("input").checked).eql(true);
      });
    });
  });

  describe("loop.panel.CallUrlForm", function() {
    var fakeClient, callUrlData, view;

    beforeEach(function() {
      callUrlData = {
        call_url: "http://call.invalid/",
        expiresAt: 1000
      };

      fakeClient = {
        requestCallUrl: function(_, cb) {
          cb(null, callUrlData);
        }
      };

      view = TestUtils.renderIntoDocument(loop.panel.CallUrlForm({
        notifier: notifier,
        client: fakeClient
      }));
    });

    describe("#render", function() {
      it("should render a ToSView", function() {
        TestUtils.findRenderedComponentWithType(view, loop.panel.ToSView);
      });
    });

    describe("Form submit event", function() {

      function submitForm(callerValue) {
        // fill caller field
        TestUtils.Simulate.change(
          TestUtils.findRenderedDOMComponentWithTag(view, "input"), {
            target: {value: callerValue}
          });

        // submit form
        TestUtils.Simulate.submit(
          TestUtils.findRenderedDOMComponentWithTag(view, "form"));
      }

      it("should reset all pending notifications", function() {
        submitForm("foo");

        sinon.assert.calledOnce(notifier.clear, "clear");
      });

      it("should request a call url to the server", function() {
        fakeClient.requestCallUrl = sandbox.stub();

        submitForm("foo");

        sinon.assert.calledOnce(fakeClient.requestCallUrl);
        sinon.assert.calledWith(fakeClient.requestCallUrl, "foo");
      });

      it("should set the call url form in a pending state", function() {
        // Cancel requestCallUrl effect to keep the state pending
        fakeClient.requestCallUrl = sandbox.stub();

        submitForm("foo");

        expect(view.state.pending).eql(true);
      });

      it("should update state with the call url received", function() {
        submitForm("foo");

        expect(view.state.pending).eql(false);
        expect(view.state.callUrl).eql(callUrlData.call_url);
      });

      it("should clear the pending state when a response is received",
        function() {
          submitForm("foo");

          expect(view.state.pending).eql(false);
        });

      it("should update CallUrlResult with the call url", function() {
        submitForm("foo");

        var urlField = view.getDOMNode().querySelector("input[type='url']");

        expect(urlField.value).eql(callUrlData.call_url);
      });

      it("should reset all pending notifications", function() {
        submitForm("foo");

        sinon.assert.calledOnce(view.props.notifier.clear);
      });

      it("should notify the user when the operation failed", function() {
        fakeClient.requestCallUrl = function(_, cb) {
          cb("fake error");
        };

        submitForm("foo");

        sinon.assert.calledOnce(notifier.errorL10n);
        sinon.assert.calledWithExactly(notifier.errorL10n,
                                       "unable_retrieve_url");
      });
    });
  });

  describe('loop.panel.ToSView', function() {

    it("should set the value of the loop.seenToS preference to 'seen'",
      function() {
        TestUtils.renderIntoDocument(loop.panel.ToSView());

        sinon.assert.calledOnce(navigator.mozLoop.setLoopCharPref);
        sinon.assert.calledWithExactly(navigator.mozLoop.setLoopCharPref,
          'seenToS', 'seen');
      });

    it("should not set the value of loop.seenToS when it's already set",
      function() {
        navigator.mozLoop.getLoopCharPref = function() {
          return "seen";
        };

        TestUtils.renderIntoDocument(loop.panel.ToSView());

        sinon.assert.notCalled(navigator.mozLoop.setLoopCharPref);
      });

    it("should render when the value of loop.seenToS is not set", function() {
      var view = TestUtils.renderIntoDocument(loop.panel.ToSView());

      TestUtils.findRenderedDOMComponentWithClass(view, "tos");
    });

    it("should not render when the value of loop.seenToS is set to 'seen'",
      function(done) {
        navigator.mozLoop.getLoopCharPref = function() {
          return "seen";
        };

        try {
          TestUtils.findRenderedDOMComponentWithClass(view, "tos");
        } catch (err) {
          done();
        }
    });
  });
});
