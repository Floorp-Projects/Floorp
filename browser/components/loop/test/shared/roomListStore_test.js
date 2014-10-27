/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var expect = chai.expect;

describe("loop.store.Room", function () {
  "use strict";
  describe("#constructor", function() {
    it("should validate room values", function() {
      expect(function() {
        new loop.store.Room();
      }).to.Throw(Error, /missing required/);
    });
  });
});

describe("loop.store.RoomListStore", function () {
  "use strict";

  var sharedActions = loop.shared.actions;
  var sandbox, dispatcher;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    dispatcher = new loop.Dispatcher();
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#constructor", function() {
    it("should throw an error if the dispatcher is missing", function() {
      expect(function() {
        new loop.store.RoomListStore({mozLoop: {}});
      }).to.Throw(/dispatcher/);
    });

    it("should throw an error if mozLoop is missing", function() {
      expect(function() {
        new loop.store.RoomListStore({dispatcher: dispatcher});
      }).to.Throw(/mozLoop/);
    });
  });

  describe("#getAllRooms", function() {
    var store, fakeMozLoop;
    var fakeRoomList = [{
      roomToken: "_nxD4V4FflQ",
      roomUrl: "http://sample/_nxD4V4FflQ",
      roomName: "First Room Name",
      maxSize: 2,
      participants: [],
      ctime: 1405517546
    }, {
      roomToken: "QzBbvGmIZWU",
      roomUrl: "http://sample/QzBbvGmIZWU",
      roomName: "Second Room Name",
      maxSize: 2,
      participants: [],
      ctime: 1405517418
    }, {
      roomToken: "3jKS_Els9IU",
      roomUrl: "http://sample/3jKS_Els9IU",
      roomName: "Third Room Name",
      maxSize: 3,
      clientMaxSize: 2,
      participants: [],
      ctime: 1405518241
    }];

    beforeEach(function() {
      fakeMozLoop = {
        rooms: {
          getAll: function(cb) {
            cb(null, fakeRoomList);
          }
        }
      };
      store = new loop.store.RoomListStore({
        dispatcher: dispatcher,
        mozLoop: fakeMozLoop
      });
    });

    it("should trigger a list:changed event", function(done) {
      store.on("change", function() {
        done();
      });

      dispatcher.dispatch(new sharedActions.GetAllRooms());
    });

    it("should fetch the room list from the mozLoop API", function(done) {
      store.once("change", function() {
        expect(store.getStoreState().error).to.be.a.undefined;
        expect(store.getStoreState().rooms).to.have.length.of(3);
        done();
      });

      dispatcher.dispatch(new sharedActions.GetAllRooms());
    });

    it("should order the room list using ctime desc", function(done) {
      store.once("change", function() {
        var storeState = store.getStoreState();
        expect(storeState.error).to.be.a.undefined;
        expect(storeState.rooms[0].ctime).eql(1405518241);
        expect(storeState.rooms[1].ctime).eql(1405517546);
        expect(storeState.rooms[2].ctime).eql(1405517418);
        done();
      });

      dispatcher.dispatch(new sharedActions.GetAllRooms());
    });

    it("should report an error", function() {
      fakeMozLoop.rooms.getAll = function(cb) {
        cb("fakeError");
      };

      store.once("change", function() {
        var storeState = store.getStoreState();
        expect(storeState.error).eql("fakeError");
      });

      dispatcher.dispatch(new sharedActions.GetAllRooms());
    });
  });
});
