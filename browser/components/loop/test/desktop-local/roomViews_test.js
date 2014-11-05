var expect = chai.expect;

describe("loop.roomViews", function () {
  "use strict";

  var sandbox, dispatcher, roomStore, activeRoomStore, fakeWindow, fakeMozLoop,
      fakeRoomId;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();

    dispatcher = new loop.Dispatcher();

    fakeWindow = { document: {} };
    loop.shared.mixins.setRootObject(fakeWindow);

    activeRoomStore = new loop.store.ActiveRoomStore({
      dispatcher: dispatcher,
      mozLoop: {}
    });
    roomStore = new loop.store.RoomStore({
      dispatcher: dispatcher,
      mozLoop: {},
      activeRoomStore: activeRoomStore
    });
  });

  afterEach(function() {
    sinon.sandbox.restore();
    loop.shared.mixins.setRootObject(window);
  });

  describe("DesktopRoomView", function() {
    function mountTestComponent() {
      return TestUtils.renderIntoDocument(
        new loop.roomViews.DesktopRoomView({
          mozLoop: {},
          roomStore: roomStore
        }));
    }

    describe("#render", function() {
      it("should set document.title to store.serverData.roomName", function() {
        mountTestComponent();

        activeRoomStore.setStoreState({serverData: {roomName: "fakeName"}});

        expect(fakeWindow.document.title).to.equal("fakeName");
      });
    });
  });
});
