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
    loop.shared.mixins.setRootObject(fakeWindow);

    store = new loop.store.LocalRoomStore({
      dispatcher: { register: function() {} },
      mozLoop: fakeMozLoop
    });
    store.setStoreState({localRoomId: fakeRoomId});
  });

  afterEach(function() {
    sinon.sandbox.restore();
    loop.shared.mixins.setRootObject(window);
  });

  describe("DesktopRoomView", function() {
    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        new loop.roomViews.DesktopRoomView({
          mozLoop: fakeMozLoop,
          localRoomStore: store
        }));
    }

    describe("#render", function() {
      it("should set document.title to store.serverData.roomName",
        function() {
          var fakeRoomName = "Monkey";
          store.setStoreState({serverData: {roomName: fakeRoomName},
                               localRoomId: fakeRoomId});

          mountTestComponent();

          expect(fakeWindow.document.title).to.equal(fakeRoomName);
        });
    });

  });
});
