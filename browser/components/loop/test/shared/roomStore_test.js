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

describe("loop.store.RoomStore", function () {
  "use strict";

  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;
  var sandbox, dispatcher;

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
    sandbox = sinon.sandbox.create();
    dispatcher = new loop.Dispatcher();
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#constructor", function() {
    it("should throw an error if the dispatcher is missing", function() {
      expect(function() {
        new loop.store.RoomStore({mozLoop: {}});
      }).to.Throw(/dispatcher/);
    });

    it("should throw an error if mozLoop is missing", function() {
      expect(function() {
        new loop.store.RoomStore({dispatcher: dispatcher});
      }).to.Throw(/mozLoop/);
    });
  });

  describe("constructed", function() {
    var fakeMozLoop, store;

    var defaultStoreState = {
      error: undefined,
      pendingCreation: false,
      pendingInitialRetrieval: false,
      rooms: [],
      activeRoom: {}
    };

    beforeEach(function() {
      fakeMozLoop = {
        copyString: function() {},
        rooms: {
          create: function() {},
          getAll: function() {},
          on: sandbox.stub()
        }
      };
      store = new loop.store.RoomStore({
        dispatcher: dispatcher,
        mozLoop: fakeMozLoop
      });
      store.setStoreState(defaultStoreState);
    });

    describe("MozLoop rooms event listeners", function() {
      beforeEach(function() {
        _.extend(fakeMozLoop.rooms, Backbone.Events);

        fakeMozLoop.rooms.getAll = function(version, cb) {
          cb(null, fakeRoomList);
        };

        store.getAllRooms(); // registers event listeners
      });

      describe("add", function() {
        it("should add the room entry to the list", function() {
          fakeMozLoop.rooms.trigger("add", "add", {
            roomToken: "newToken",
            roomUrl: "http://sample/newToken",
            roomName: "New room",
            maxSize: 2,
            participants: [],
            ctime: 1405517546
          });

          expect(store.getStoreState().rooms).to.have.length.of(4);
        });
      });

      describe("update", function() {
        it("should update a room entry", function() {
          fakeMozLoop.rooms.trigger("update", "update", {
            roomToken: "_nxD4V4FflQ",
            roomUrl: "http://sample/_nxD4V4FflQ",
            roomName: "Changed First Room Name",
            maxSize: 2,
            participants: [],
            ctime: 1405517546
          });

          expect(store.getStoreState().rooms).to.have.length.of(3);
          expect(store.getStoreState().rooms.some(function(room) {
            return room.roomName === "Changed First Room Name";
          })).eql(true);
        });
      });

      describe("delete", function() {
        it("should delete a room from the list", function() {
          fakeMozLoop.rooms.trigger("delete", "delete", {
            roomToken: "_nxD4V4FflQ"
          });

          expect(store.getStoreState().rooms).to.have.length.of(2);
          expect(store.getStoreState().rooms.some(function(room) {
            return room.roomToken === "_nxD4V4FflQ";
          })).eql(false);
        });
      });
    });

    describe("#getStoreState", function() {
      it("should retrieve the whole state by default", function() {
        expect(store.getStoreState()).eql(defaultStoreState);
      });

      it("should retrieve a given property state", function() {
        var fakeActiveRoom = {fake: true};
        store.setStoreState({activeRoom: fakeActiveRoom});

        expect(store.getStoreState().activeRoom).eql(fakeActiveRoom);
      });
    });

    describe("#findNextAvailableRoomNumber", function() {
      var fakeNameTemplate = "RoomWord {{conversationLabel}}";

      it("should find next available room number from an empty room list",
        function() {
          store.setStoreState({rooms: []});

          expect(store.findNextAvailableRoomNumber(fakeNameTemplate)).eql(1);
        });

      it("should find next available room number from a non empty room list",
        function() {
          store.setStoreState({
            rooms: [{roomName: "RoomWord 1"}]
          });

          expect(store.findNextAvailableRoomNumber(fakeNameTemplate)).eql(2);
        });

      it("should not be sensitive to initial list order", function() {
        store.setStoreState({
          rooms: [{roomName: "RoomWord 99"}, {roomName: "RoomWord 98"}]
        });

        expect(store.findNextAvailableRoomNumber(fakeNameTemplate)).eql(100);
      });
    });

    describe("#createRoom", function() {
      var fakeNameTemplate = "Conversation {{conversationLabel}}";
      var fakeLocalRoomId = "777";
      var fakeOwner = "fake@invalid";
      var fakeRoomCreationData = {
        nameTemplate: fakeNameTemplate,
        roomOwner: fakeOwner
      };

      var fakeCreatedRoom = {
        roomName: "Conversation 1",
        roomToken: "fake",
        roomUrl: "http://invalid",
        maxSize: 42,
        participants: [],
        ctime: 1234567890
      };

      beforeEach(function() {
        store.setStoreState({pendingCreation: false, rooms: []});
      });

      it("should request creation of a new room", function() {
        sandbox.stub(fakeMozLoop.rooms, "create");

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledWith(fakeMozLoop.rooms.create, {
          roomName: "Conversation 1",
          roomOwner: fakeOwner,
          maxSize: store.maxRoomCreationSize,
          expiresIn: store.defaultExpiresIn
        });
      });

      it("should store any creation encountered error", function() {
        var err = new Error("fake");
        sandbox.stub(fakeMozLoop.rooms, "create", function(data, cb) {
          cb(err);
        });

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        expect(store.getStoreState().error).eql(err);
      });

      it("should switch the pendingCreation state flag to true", function() {
        sandbox.stub(fakeMozLoop.rooms, "create");

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        expect(store.getStoreState().pendingCreation).eql(true);
      });

      it("should switch the pendingCreation state flag to false once the " +
         "operation is done", function() {
        sandbox.stub(fakeMozLoop.rooms, "create", function(data, cb) {
          cb();
        });

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        expect(store.getStoreState().pendingCreation).eql(false);
      });
    });

    describe("#copyRoomUrl", function() {
      it("should copy the room URL", function() {
        var copyString = sandbox.stub(fakeMozLoop, "copyString");

        store.copyRoomUrl(new sharedActions.CopyRoomUrl({
          roomUrl: "http://invalid"
        }));

        sinon.assert.calledOnce(copyString);
        sinon.assert.calledWithExactly(copyString, "http://invalid");
      });
    });

    describe("#emailRoomUrl", function() {
      it("should call composeCallUrlEmail to email the url", function() {
        sandbox.stub(sharedUtils, "composeCallUrlEmail");

        store.emailRoomUrl(new sharedActions.EmailRoomUrl({
          roomUrl: "http://invalid"
        }));

        sinon.assert.calledOnce(sharedUtils.composeCallUrlEmail);
        sinon.assert.calledWithExactly(sharedUtils.composeCallUrlEmail,
          "http://invalid");
      });
    });

    describe("#setStoreState", function() {
      it("should update store state data", function() {
        store.setStoreState({pendingCreation: true});

        expect(store.getStoreState().pendingCreation).eql(true);
      });

      it("should trigger a `change` event", function(done) {
        store.once("change", function() {
          done();
        });

        store.setStoreState({pendingCreation: true});
      });

      it("should trigger a `change:<prop>` event", function(done) {
        store.once("change:pendingCreation", function() {
          done();
        });

        store.setStoreState({pendingCreation: true});
      });
    });

    describe("#getAllRooms", function() {
      it("should fetch the room list from the MozLoop API", function() {
        fakeMozLoop.rooms.getAll = function(version, cb) {
          cb(null, fakeRoomList);
        };

        store.getAllRooms(new sharedActions.GetAllRooms());

        expect(store.getStoreState().error).to.be.a.undefined;
        expect(store.getStoreState().rooms).to.have.length.of(3);
      });

      it("should order the room list using ctime desc", function() {
        fakeMozLoop.rooms.getAll = function(version, cb) {
          cb(null, fakeRoomList);
        };

        store.getAllRooms(new sharedActions.GetAllRooms());

        var storeState = store.getStoreState();
        expect(storeState.error).to.be.a.undefined;
        expect(storeState.rooms[0].ctime).eql(1405518241);
        expect(storeState.rooms[1].ctime).eql(1405517546);
        expect(storeState.rooms[2].ctime).eql(1405517418);
      });

      it("should report an error", function() {
        var err = new Error("fake");
        fakeMozLoop.rooms.getAll = function(version, cb) {
          cb(err);
        };

        dispatcher.dispatch(new sharedActions.GetAllRooms());

        expect(store.getStoreState().error).eql(err);
      });

      it("should register event listeners after the list is retrieved",
        function() {
          sandbox.stub(store, "startListeningToRoomEvents");
          fakeMozLoop.rooms.getAll = function(version, cb) {
            cb(null, fakeRoomList);
          };

          store.getAllRooms();

          sinon.assert.calledOnce(store.startListeningToRoomEvents);
        });

      it("should set the pendingInitialRetrieval flag to true", function() {
        store.getAllRooms();

        expect(store.getStoreState().pendingInitialRetrieval).eql(true);
      });

      it("should set pendingInitialRetrieval to false once the action is " +
         "performed", function() {
        fakeMozLoop.rooms.getAll = function(version, cb) {
          cb(null, fakeRoomList);
        };

        store.getAllRooms();

        expect(store.getStoreState().pendingInitialRetrieval).eql(false);
      });
    });

    describe("ActiveRoomStore substore", function() {
      var store, activeRoomStore;

      beforeEach(function() {
        activeRoomStore = new loop.store.ActiveRoomStore({
          dispatcher: dispatcher,
          mozLoop: fakeMozLoop,
          sdkDriver: {}
        });
        store = new loop.store.RoomStore({
          dispatcher: dispatcher,
          mozLoop: fakeMozLoop,
          activeRoomStore: activeRoomStore
        });
      });

      it("should subscribe to substore changes", function() {
        var fakeServerData = {fake: true};

        activeRoomStore.setStoreState({serverData: fakeServerData});

        expect(store.getStoreState().activeRoom.serverData)
          .eql(fakeServerData);
      });

      it("should trigger a change event when the substore is updated",
        function(done) {
          store.once("change:activeRoom", function() {
            done();
          });

          activeRoomStore.setStoreState({serverData: {}});
        });
    });
  });

  describe("#openRoom", function() {
    var store, fakeMozLoop;

    beforeEach(function() {
      fakeMozLoop = {
        rooms: {
          open: sinon.spy()
        }
      };
      store = new loop.store.RoomStore({
        dispatcher: dispatcher,
        mozLoop: fakeMozLoop
      });
    });

    it("should open the room via mozLoop", function() {
      dispatcher.dispatch(new sharedActions.OpenRoom({roomToken: "42abc"}));

      sinon.assert.calledOnce(fakeMozLoop.rooms.open);
      sinon.assert.calledWithExactly(fakeMozLoop.rooms.open, "42abc");
    });
  });

  describe("#renameRoom", function() {
    var store, fakeMozLoop;

    beforeEach(function() {
      fakeMozLoop = {
        rooms: {
          rename: sinon.spy()
        }
      };
      store = new loop.store.RoomStore({
        dispatcher: dispatcher,
        mozLoop: fakeMozLoop
      });
    });

    it("should rename the room via mozLoop", function() {
      dispatcher.dispatch(new sharedActions.RenameRoom({
        roomToken: "42abc",
        newRoomName: "silly name"
      }));

      sinon.assert.calledOnce(fakeMozLoop.rooms.rename);
      sinon.assert.calledWith(fakeMozLoop.rooms.rename, "42abc",
        "silly name");
    });
  });
});
