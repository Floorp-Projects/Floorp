var expect = chai.expect;

describe("loop.roomViews", function () {
  "use strict";

  var store, fakeWindow, sandbox, fakeAddCallback, fakeMozLoop,
    fakeRemoveCallback, fakeRoomId, fakeWindow;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    fakeRoomId = "fakeRoomId";
    fakeAddCallback =
      sandbox.stub().withArgs(fakeRoomId, "RoomCreationError");
    fakeRemoveCallback =
      sandbox.stub().withArgs(fakeRoomId, "RoomCreationError");
    fakeMozLoop = { rooms: { addCallback: fakeAddCallback,
                             removeCallback: fakeRemoveCallback } };

    fakeWindow = { document: {} };
    loop.roomViews.setRootObject(fakeWindow);

    store = new loop.store.LocalRoomStore({
      dispatcher: { register: function() {} },
      mozLoop: fakeMozLoop
    });
    store.setStoreState({localRoomId: fakeRoomId});
  });

  afterEach(function() {
    sinon.sandbox.restore();
    loop.roomViews.setRootObject(window);
  });

  describe("EmptyRoomView", function() {
    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        new loop.roomViews.EmptyRoomView({
          mozLoop: fakeMozLoop,
          localRoomStore: store
        }));
    }

    describe("#componentDidMount", function() {
       it("should add #onCreationError using mozLoop.rooms.addCallback",
         function() {

           var testComponent = mountTestComponent();

           sinon.assert.calledOnce(fakeMozLoop.rooms.addCallback);
           sinon.assert.calledWithExactly(fakeMozLoop.rooms.addCallback,
             fakeRoomId, "RoomCreationError", testComponent.onCreationError);
         });
    });

    describe("#componentWillUnmount", function () {
      it("should remove #onCreationError using mozLoop.rooms.addCallback",
        function () {
          var testComponent = mountTestComponent();

          testComponent.componentWillUnmount();

          sinon.assert.calledOnce(fakeMozLoop.rooms.removeCallback);
          sinon.assert.calledWithExactly(fakeMozLoop.rooms.removeCallback,
            fakeRoomId, "RoomCreationError", testComponent.onCreationError);
        });
      });

    describe("#onCreationError", function() {
      it("should log an error using console.error", function() {
        fakeWindow.console = { error: sandbox.stub() };
        var testComponent = mountTestComponent();

        testComponent.onCreationError(new Error("fake error"));

        sinon.assert.calledOnce(fakeWindow.console.error);
      });
    });

    describe("#render", function() {
      it("should set document.title to store.serverData.roomName",
        function() {
          var fakeRoomName = "Monkey";
          store.setStoreState({serverData: {roomName: fakeRoomName},
                               localRoomId: fakeRoomId});

          mountTestComponent();

          expect(fakeWindow.document.title).to.equal(fakeRoomName);
        })
    });

  });
});
