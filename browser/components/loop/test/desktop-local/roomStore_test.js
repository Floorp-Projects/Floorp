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
  var fakeRoomList;

  beforeEach(function() {
    sandbox = sinon.sandbox.create();
    dispatcher = new loop.Dispatcher();
    fakeRoomList = [{
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
  });

  afterEach(function() {
    sandbox.restore();
  });

  describe("#constructor", function() {
    it("should throw an error if mozLoop is missing", function() {
      expect(function() {
        new loop.store.RoomStore(dispatcher);
      }).to.Throw(/mozLoop/);
    });
  });

  describe("constructed", function() {
    var fakeMozLoop, fakeNotifications, store;

    var defaultStoreState = {
      error: undefined,
      pendingCreation: false,
      pendingInitialRetrieval: false,
      rooms: [],
      activeRoom: {}
    };

    beforeEach(function() {
      fakeMozLoop = {
        SHARING_ROOM_URL: {
          COPY_FROM_PANEL: 0,
          COPY_FROM_CONVERSATION: 1,
          EMAIL_FROM_PANEL: 2,
          EMAIL_FROM_CONVERSATION: 3
        },
        ROOM_CREATE: {
          CREATE_SUCCESS: 0,
          CREATE_FAIL: 1
        },
        ROOM_DELETE: {
          DELETE_SUCCESS: 0,
          DELETE_FAIL: 1
        },
        ROOM_CONTEXT_ADD: {
          ADD_FROM_PANEL: 0,
          ADD_FROM_CONVERSATION: 1
        },
        copyString: function() {},
        getLoopPref: function(pref) {
          return pref;
        },
        notifyUITour: function() {},
        rooms: {
          create: function() {},
          delete: function() {},
          getAll: function() {},
          open: function() {},
          rename: function() {},
          on: sandbox.stub()
        },
        telemetryAddValue: sinon.stub()
      };
      fakeNotifications = {
        set: sinon.stub(),
        remove: sinon.stub()
      };
      store = new loop.store.RoomStore(dispatcher, {
        mozLoop: fakeMozLoop,
        notifications: fakeNotifications
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

        it("should avoid adding a duplicate room", function() {
          var sampleRoom = fakeRoomList[0];

          fakeMozLoop.rooms.trigger("add", "add", sampleRoom);

          expect(store.getStoreState().rooms).to.have.length.of(3);
          expect(store.getStoreState().rooms.reduce(function(count, room) {
            return count += room.roomToken === sampleRoom.roomToken ? 1 : 0;
          }, 0)).eql(1);
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

      describe("refresh", function() {
        it("should clear the list of rooms", function() {
          fakeMozLoop.rooms.trigger("refresh", "refresh");

          expect(store.getStoreState().rooms).to.have.length.of(0);
        });
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
            rooms: [{decryptedContext: {roomName: "RoomWord 1"}}]
          });

          expect(store.findNextAvailableRoomNumber(fakeNameTemplate)).eql(2);
        });

      it("should not be sensitive to initial list order", function() {
        store.setStoreState({
          rooms: [{
            decryptedContext: {
              roomName: "RoomWord 99"
            }
          }, {
            decryptedContext: {
              roomName: "RoomWord 98"
            }
          }]
        });

        expect(store.findNextAvailableRoomNumber(fakeNameTemplate)).eql(100);
      });
    });

    describe("#createRoom", function() {
      var fakeNameTemplate = "Conversation {{conversationLabel}}";
      var fakeLocalRoomId = "777";
      var fakeOwner = "fake@invalid";
      var fakeRoomCreationData;

      beforeEach(function() {
        sandbox.stub(dispatcher, "dispatch");
        store.setStoreState({pendingCreation: false, rooms: []});
        fakeRoomCreationData = {
          nameTemplate: fakeNameTemplate,
          roomOwner: fakeOwner
        };
      });

      it("should clear any existing room errors", function() {
        sandbox.stub(fakeMozLoop.rooms, "create");

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledOnce(fakeNotifications.remove);
        sinon.assert.calledWithExactly(fakeNotifications.remove,
          "create-room-error");
      });

      it("should log a telemetry event when the operation with context is successful", function() {
        sandbox.stub(fakeMozLoop.rooms, "create", function(data, cb) {
          cb(null, {roomToken: "fakeToken"});
        });

        fakeRoomCreationData.urls = [{
          location: "http://invalid.com",
          description: "fakeSite",
          thumbnail: "fakeimage.png"
        }];

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledTwice(fakeMozLoop.telemetryAddValue);
        sinon.assert.calledWithExactly(fakeMozLoop.telemetryAddValue,
          "LOOP_ROOM_CONTEXT_ADD", 0);
      });

      it("should request creation of a new room", function() {
        sandbox.stub(fakeMozLoop.rooms, "create");

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledWith(fakeMozLoop.rooms.create, {
          decryptedContext: {
            roomName: "Conversation 1"
          },
          roomOwner: fakeOwner,
          maxSize: store.maxRoomCreationSize
        });
      });

      it("should request creation of a new room with context", function() {
        sandbox.stub(fakeMozLoop.rooms, "create");

        fakeRoomCreationData.urls = [{
          location: "http://invalid.com",
          description: "fakeSite",
          thumbnail: "fakeimage.png"
        }];

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledWith(fakeMozLoop.rooms.create, {
          decryptedContext: {
            roomName: "Conversation 1",
            urls: [{
              location: "http://invalid.com",
              description: "fakeSite",
              thumbnail: "fakeimage.png"
            }]
          },
          roomOwner: fakeOwner,
          maxSize: store.maxRoomCreationSize
        });
      });

      it("should switch the pendingCreation state flag to true", function() {
        sandbox.stub(fakeMozLoop.rooms, "create");

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        expect(store.getStoreState().pendingCreation).eql(true);
      });

      it("should dispatch a CreatedRoom action once the operation is done",
        function() {
          sandbox.stub(fakeMozLoop.rooms, "create", function(data, cb) {
            cb(null, {roomToken: "fakeToken"});
          });

          store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.CreatedRoom({
              roomToken: "fakeToken"
            }));
        });

      it("should dispatch a CreateRoomError action if the operation fails",
        function() {
          var err = new Error("fake");
          sandbox.stub(fakeMozLoop.rooms, "create", function(data, cb) {
            cb(err);
          });

          store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.CreateRoomError({
              error: err
            }));
        });

      it("should log a telemetry event when the operation is successful", function() {
        sandbox.stub(fakeMozLoop.rooms, "create", function(data, cb) {
          cb(null, {roomToken: "fakeToken"});
        });

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledOnce(fakeMozLoop.telemetryAddValue);
        sinon.assert.calledWithExactly(fakeMozLoop.telemetryAddValue,
          "LOOP_ROOM_CREATE", 0);
      });

      it("should log a telemetry event when the operation fails", function() {
        var err = new Error("fake");
        sandbox.stub(fakeMozLoop.rooms, "create", function(data, cb) {
          cb(err);
        });

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledOnce(fakeMozLoop.telemetryAddValue);
        sinon.assert.calledWithExactly(fakeMozLoop.telemetryAddValue,
          "LOOP_ROOM_CREATE", 1);
      });
   });

   describe("#createdRoom", function() {
      beforeEach(function() {
        sandbox.stub(dispatcher, "dispatch");
      });

      it("should switch the pendingCreation state flag to false", function() {
        store.setStoreState({pendingCreation: true});

        store.createdRoom(new sharedActions.CreatedRoom({
          roomToken: "fakeToken"
        }));

        expect(store.getStoreState().pendingCreation).eql(false);
      });

      it("should dispatch an OpenRoom action once the operation is done",
        function() {
          store.createdRoom(new sharedActions.CreatedRoom({
            roomToken: "fakeToken"
          }));

          sinon.assert.calledOnce(dispatcher.dispatch);
          sinon.assert.calledWithExactly(dispatcher.dispatch,
            new sharedActions.OpenRoom({
              roomToken: "fakeToken"
            }));
        });
    });

    describe("#createRoomError", function() {
      it("should switch the pendingCreation state flag to false", function() {
        store.setStoreState({pendingCreation: true});

        store.createRoomError({
          error: new Error("fake")
        });

        expect(store.getStoreState().pendingCreation).eql(false);
      });

      it("should set a notification", function() {
        store.createRoomError({
          error: new Error("fake")
        });

        sinon.assert.calledOnce(fakeNotifications.set);
        sinon.assert.calledWithMatch(fakeNotifications.set, {
          id: "create-room-error",
          level: "error"
        });
      });
    });

    describe("#deleteRoom", function() {
      var fakeRoomToken = "42abc";

      beforeEach(function() {
        sandbox.stub(dispatcher, "dispatch");
      });

      it("should request deletion of a room", function() {
        sandbox.stub(fakeMozLoop.rooms, "delete");

        store.deleteRoom(new sharedActions.DeleteRoom({
          roomToken: fakeRoomToken
        }));

        sinon.assert.calledWith(fakeMozLoop.rooms.delete, fakeRoomToken);
      });

      it("should dispatch a DeleteRoomError action if the operation fails", function() {
        var err = new Error("fake");
        sandbox.stub(fakeMozLoop.rooms, "delete", function(roomToken, cb) {
          cb(err);
        });

        store.deleteRoom(new sharedActions.DeleteRoom({
          roomToken: fakeRoomToken
        }));

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch,
          new sharedActions.DeleteRoomError({
            error: err
          }));
      });

      it("should log a telemetry event when the operation is successful", function() {
        sandbox.stub(fakeMozLoop.rooms, "delete", function(roomToken, cb) {
          cb();
        });

        store.deleteRoom(new sharedActions.DeleteRoom({
          roomToken: fakeRoomToken
        }));

        sinon.assert.calledOnce(fakeMozLoop.telemetryAddValue);
        sinon.assert.calledWithExactly(fakeMozLoop.telemetryAddValue,
          "LOOP_ROOM_DELETE", 0);
      });

      it("should log a telemetry event when the operation fails", function() {
        var err = new Error("fake");
        sandbox.stub(fakeMozLoop.rooms, "delete", function(roomToken, cb) {
          cb(err);
        });

        store.deleteRoom(new sharedActions.DeleteRoom({
          roomToken: fakeRoomToken
        }));

        sinon.assert.calledOnce(fakeMozLoop.telemetryAddValue);
        sinon.assert.calledWithExactly(fakeMozLoop.telemetryAddValue,
          "LOOP_ROOM_DELETE", 1);
      });
    });

    describe("#copyRoomUrl", function() {
      it("should copy the room URL", function() {
        var copyString = sandbox.stub(fakeMozLoop, "copyString");

        store.copyRoomUrl(new sharedActions.CopyRoomUrl({
          roomUrl: "http://invalid",
          from: "conversation"
        }));

        sinon.assert.calledOnce(copyString);
        sinon.assert.calledWithExactly(copyString, "http://invalid");
      });

      it("should send a telemetry event for copy from panel", function() {
        store.copyRoomUrl(new sharedActions.CopyRoomUrl({
          roomUrl: "http://invalid",
          from: "panel"
        }));

        sinon.assert.calledOnce(fakeMozLoop.telemetryAddValue);
        sinon.assert.calledWithExactly(fakeMozLoop.telemetryAddValue,
          "LOOP_SHARING_ROOM_URL", 0);
      });

      it("should send a telemetry event for copy from conversation", function() {
        store.copyRoomUrl(new sharedActions.CopyRoomUrl({
          roomUrl: "http://invalid",
          from: "conversation"
        }));

        sinon.assert.calledOnce(fakeMozLoop.telemetryAddValue);
        sinon.assert.calledWithExactly(fakeMozLoop.telemetryAddValue,
          "LOOP_SHARING_ROOM_URL", 1);
      });
    });

    describe("#emailRoomUrl", function() {
      it("should call composeCallUrlEmail to email the url", function() {
        sandbox.stub(sharedUtils, "composeCallUrlEmail");

        store.emailRoomUrl(new sharedActions.EmailRoomUrl({
          roomUrl: "http://invalid",
          from: "conversation"
        }));

        sinon.assert.calledOnce(sharedUtils.composeCallUrlEmail);
        sinon.assert.calledWith(sharedUtils.composeCallUrlEmail,
          "http://invalid", null, undefined, "conversation");
      });

      it("should call composeUrlEmail differently with context", function() {
        sandbox.stub(sharedUtils, "composeCallUrlEmail");

        var url = "http://invalid";
        var description = "Hello, is it me you're looking for?";
        store.emailRoomUrl(new sharedActions.EmailRoomUrl({
          roomUrl: url,
          roomDescription: description,
          from: "conversation"
        }));

        sinon.assert.calledOnce(sharedUtils.composeCallUrlEmail);
        sinon.assert.calledWithExactly(sharedUtils.composeCallUrlEmail,
          url, null, description, "conversation");
      });
    });

    describe("#shareRoomUrl", function() {
      beforeEach(function() {
        fakeMozLoop.socialShareRoom = sinon.stub();
      });

      it("should pass the correct data for GMail sharing", function() {
        var roomUrl = "http://invalid";
        var origin = "https://mail.google.com/v1";
        store.shareRoomUrl(new sharedActions.ShareRoomUrl({
          roomUrl: roomUrl,
          provider: {
            origin: origin
          }
        }));

        sinon.assert.calledOnce(fakeMozLoop.socialShareRoom);
        sinon.assert.calledWithExactly(fakeMozLoop.socialShareRoom, origin,
          roomUrl, "share_email_subject5", "share_email_body5");
      });

      it("should pass the correct data for all other Social Providers", function() {
        var roomUrl = "http://invalid2";
        var origin = "https://twitter.com/share";
        store.shareRoomUrl(new sharedActions.ShareRoomUrl({
          roomUrl: roomUrl,
          provider: {
            origin: origin
          }
        }));

        sinon.assert.calledOnce(fakeMozLoop.socialShareRoom);
        sinon.assert.calledWithExactly(fakeMozLoop.socialShareRoom, origin,
          roomUrl, "share_tweet", null);
      });
    });

    describe("#addSocialShareProvider", function() {
      it("should invoke to the correct mozLoop function", function() {
        fakeMozLoop.addSocialShareProvider = sinon.stub();

        store.addSocialShareProvider(new sharedActions.AddSocialShareProvider());

        sinon.assert.calledOnce(fakeMozLoop.addSocialShareProvider);
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
      var fakeStore, activeRoomStore;

      beforeEach(function() {
        activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, {
          mozLoop: fakeMozLoop,
          sdkDriver: {}
        });
        fakeStore = new loop.store.RoomStore(dispatcher, {
          mozLoop: fakeMozLoop,
          activeRoomStore: activeRoomStore
        });
      });

      it("should subscribe to substore changes", function() {
        var fakeServerData = {fake: true};

        activeRoomStore.setStoreState({serverData: fakeServerData});

        expect(fakeStore.getStoreState().activeRoom.serverData)
          .eql(fakeServerData);
      });

      it("should trigger a change event when the substore is updated",
        function(done) {
          fakeStore.once("change:activeRoom", function() {
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
      store = new loop.store.RoomStore(dispatcher, {mozLoop: fakeMozLoop});
    });

    it("should open the room via mozLoop", function() {
      dispatcher.dispatch(new sharedActions.OpenRoom({roomToken: "42abc"}));

      sinon.assert.calledOnce(fakeMozLoop.rooms.open);
      sinon.assert.calledWithExactly(fakeMozLoop.rooms.open, "42abc");
    });
  });

  describe("#updateRoomContext", function() {
    var store, fakeMozLoop, clock;

    beforeEach(function() {
      clock = sinon.useFakeTimers();
      fakeMozLoop = {
        rooms: {
          get: sinon.stub().callsArgWith(1, null, {
            roomToken: "42abc",
            decryptedContext: {
              roomName: "sillier name"
            }
          }),
          update: null
        }
      };
      store = new loop.store.RoomStore(dispatcher, {mozLoop: fakeMozLoop});
    });

    afterEach(function() {
      clock.restore();
    });

    it("should rename the room via mozLoop", function() {
      fakeMozLoop.rooms.update = sinon.spy();
      dispatcher.dispatch(new sharedActions.UpdateRoomContext({
        roomToken: "42abc",
        newRoomName: "silly name"
      }));

      sinon.assert.calledOnce(fakeMozLoop.rooms.get);
      sinon.assert.calledOnce(fakeMozLoop.rooms.update);
      sinon.assert.calledWith(fakeMozLoop.rooms.update, "42abc", {
        roomName: "silly name"
      });
    });

    it("should flag the the store as saving context", function() {
      expect(store.getStoreState().savingContext).to.eql(false);

      sandbox.stub(fakeMozLoop.rooms, "update", function(roomToken, roomData, cb) {
        expect(store.getStoreState().savingContext).to.eql(true);
        cb();
      });

      dispatcher.dispatch(new sharedActions.UpdateRoomContext({
        roomToken: "42abc",
        newRoomName: "silly name"
      }));

      expect(store.getStoreState().savingContext).to.eql(false);
    });

    it("should store any update-encountered error", function() {
      var err = new Error("fake");
      sandbox.stub(fakeMozLoop.rooms, "update", function(roomToken, roomData, cb) {
        expect(store.getStoreState().savingContext).to.eql(true);
        cb(err);
      });

      dispatcher.dispatch(new sharedActions.UpdateRoomContext({
        roomToken: "42abc",
        newRoomName: "silly name"
      }));

      var state = store.getStoreState();
      expect(state.error).eql(err);
      expect(state.savingContext).to.eql(false);
    });

    it("should ensure only submitting a non-empty room name", function() {
      fakeMozLoop.rooms.update = sinon.spy();

      dispatcher.dispatch(new sharedActions.UpdateRoomContext({
        roomToken: "42abc",
        newRoomName: " \t  \t "
      }));
      clock.tick(1);

      sinon.assert.notCalled(fakeMozLoop.rooms.update);
      expect(store.getStoreState().savingContext).to.eql(false);
    });

    it("should save updated context information", function() {
      fakeMozLoop.rooms.update = sinon.spy();

      dispatcher.dispatch(new sharedActions.UpdateRoomContext({
        roomToken: "42abc",
        // Room name doesn't need to change.
        newRoomName: "sillier name",
        newRoomDescription: "Hello, is it me you're looking for?",
        newRoomThumbnail: "http://example.com/empty.gif",
        newRoomURL: "http://example.com"
      }));

      sinon.assert.calledOnce(fakeMozLoop.rooms.update);
      sinon.assert.calledWith(fakeMozLoop.rooms.update, "42abc", {
        urls: [{
          description: "Hello, is it me you're looking for?",
          location: "http://example.com",
          thumbnail: "http://example.com/empty.gif"
        }]
      });
    });

    it("should not save context information with an invalid URL", function() {
      fakeMozLoop.rooms.update = sinon.spy();

      dispatcher.dispatch(new sharedActions.UpdateRoomContext({
        roomToken: "42abc",
        // Room name doesn't need to change.
        newRoomName: "sillier name",
        newRoomDescription: "Hello, is it me you're looking for?",
        newRoomThumbnail: "http://example.com/empty.gif",
        // NOTE: there are many variation we could test here, but the URL object
        // constructor also fails on empty strings and is using the Gecko URL
        // parser. Therefore we ought to rely on it working properly.
        newRoomURL: "http/example.com"
      }));

      sinon.assert.notCalled(fakeMozLoop.rooms.update);
    });

    it("should not save context information when no context information is provided",
      function() {
        fakeMozLoop.rooms.update = sinon.spy();

        dispatcher.dispatch(new sharedActions.UpdateRoomContext({
          roomToken: "42abc",
          // Room name doesn't need to change.
          newRoomName: "sillier name",
          newRoomDescription: "",
          newRoomThumbnail: "",
          newRoomURL: ""
        }));
        clock.tick(1);

        sinon.assert.notCalled(fakeMozLoop.rooms.update);
        expect(store.getStoreState().savingContext).to.eql(false);
      });
  });
});
