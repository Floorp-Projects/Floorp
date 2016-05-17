"use strict"; /* Any copyright is dedicated to the Public Domain.
               * http://creativecommons.org/publicdomain/zero/1.0/ */

var expect = chai.expect;

describe("loop.store.Room", function () {
  "use strict";

  describe("#constructor", function () {
    it("should validate room values", function () {
      expect(function () {
        new loop.store.Room();}).
      to.Throw(Error, /missing required/);});});});




describe("loop.store.RoomStore", function () {
  "use strict";

  var sharedActions = loop.shared.actions;
  var sharedUtils = loop.shared.utils;
  var sandbox, dispatcher;
  var fakeRoomList, requestStubs;

  beforeEach(function () {
    sandbox = LoopMochaUtils.createSandbox();

    LoopMochaUtils.stubLoopRequest(requestStubs = { 
      GetAllConstants: function GetAllConstants() {
        return { 
          SHARING_ROOM_URL: { 
            COPY_FROM_PANEL: 0, 
            COPY_FROM_CONVERSATION: 1, 
            EMAIL_FROM_CALLFAILED: 2, 
            EMAIL_FROM_CONVERSATION: 3, 
            FACEBOOK_FROM_CONVERSATION: 4, 
            EMAIL_FROM_PANEL: 5 }, 

          ROOM_CREATE: { 
            CREATE_SUCCESS: 0, 
            CREATE_FAIL: 1 }, 

          ROOM_DELETE: { 
            DELETE_SUCCESS: 0, 
            DELETE_FAIL: 1 }, 

          LOOP_MAU_TYPE: { 
            OPEN_PANEL: 0, 
            OPEN_CONVERSATION: 1, 
            ROOM_OPEN: 2, 
            ROOM_SHARE: 3, 
            ROOM_DELETE: 4 } };}, 



      CopyString: sinon.stub(), 
      ComposeEmail: sinon.stub(), 
      GetLoopPref: function GetLoopPref(prefName) {
        if (prefName === "debug.dispatcher") {
          return false;}

        return true;}, 

      NotifyUITour: function NotifyUITour() {}, 
      OpenURL: sinon.stub(), 
      "Rooms:Create": sinon.stub().returns({ 
        decryptedContext: [], 
        roomToken: "fakeToken", 
        roomUrl: "fakeUrl" }), 

      "Rooms:Delete": sinon.stub(), 
      "Rooms:GetAll": sinon.stub(), 
      "Rooms:Open": sinon.stub(), 
      "Rooms:Rename": sinon.stub(), 
      "Rooms:PushSubscription": sinon.stub(), 
      "SetLoopPref": sinon.stub(), 
      "SetNameNewRoom": sinon.stub(), 
      TelemetryAddValue: sinon.stub() });


    dispatcher = new loop.Dispatcher();
    fakeRoomList = [{ 
      roomToken: "_nxD4V4FflQ", 
      roomUrl: "http://sample/_nxD4V4FflQ", 
      roomName: "First Room Name", 
      maxSize: 2, 
      participants: [], 
      ctime: 1405517546 }, 
    { 
      roomToken: "QzBbvGmIZWU", 
      roomUrl: "http://sample/QzBbvGmIZWU", 
      roomName: "Second Room Name", 
      maxSize: 2, 
      participants: [], 
      ctime: 1405517418 }, 
    { 
      roomToken: "3jKS_Els9IU", 
      roomUrl: "http://sample/3jKS_Els9IU", 
      roomName: "Third Room Name", 
      maxSize: 3, 
      clientMaxSize: 2, 
      participants: [], 
      ctime: 1405518241 }];


    document.mozL10n.get = function (str) {return str;};});


  afterEach(function () {
    sandbox.restore();
    LoopMochaUtils.restore();});


  describe("#constructor", function () {
    it("should throw an error if constants are missing", function () {
      expect(function () {
        new loop.store.RoomStore(dispatcher);}).
      to.Throw(/constants/);});});



  describe("constructed", function () {
    var fakeNotifications, store;

    var defaultStoreState = { 
      error: undefined, 
      pendingCreation: false, 
      pendingInitialRetrieval: true, 
      rooms: [], 
      activeRoom: {} };


    beforeEach(function () {
      fakeNotifications = { 
        set: sinon.stub(), 
        remove: sinon.stub() };

      store = new loop.store.RoomStore(dispatcher, { 
        constants: requestStubs.GetAllConstants(), 
        notifications: fakeNotifications });

      store.setStoreState(defaultStoreState);});


    describe("MozLoop rooms event listeners", function () {
      beforeEach(function () {
        LoopMochaUtils.stubLoopRequest({ 
          "Rooms:GetAll": function RoomsGetAll() {
            return fakeRoomList;} });



        store.getAllRooms(); // registers event listeners
      });

      describe("add", function () {
        it("should add the room entry to the list", function () {
          LoopMochaUtils.publish("Rooms:Add", { 
            roomToken: "newToken", 
            roomUrl: "http://sample/newToken", 
            roomName: "New room", 
            maxSize: 2, 
            participants: [], 
            ctime: 1405517546 });


          expect(store.getStoreState().rooms).to.have.length.of(4);});


        it("should avoid adding a duplicate room", function () {
          var sampleRoom = fakeRoomList[0];

          LoopMochaUtils.publish("Rooms:Add", sampleRoom);

          expect(store.getStoreState().rooms).to.have.length.of(3);
          expect(store.getStoreState().rooms.reduce(function (count, room) {
            return count + (room.roomToken === sampleRoom.roomToken ? 1 : 0);}, 
          0)).eql(1);});});



      describe("close", function () {
        it("should request rename if closing room is last created", function () {
          store.setStoreState({ 
            openedRoom: "fakeToken", 
            lastCreatedRoom: "fakeToken" });


          sinon.stub(store, "setStoreState");

          LoopMochaUtils.publish("Rooms:Close");

          sinon.assert.calledTwice(store.setStoreState);
          sinon.assert.calledWithExactly(store.setStoreState.getCall(0), 
          { closingNewRoom: true });
          sinon.assert.calledOnce(requestStubs.SetNameNewRoom);});


        it("should not request rename if last created and opened room are null", function () {
          store.setStoreState({ 
            lastCreatedRoom: null, 
            openedRoom: null });


          sinon.stub(store, "setStoreState");

          LoopMochaUtils.publish("Rooms:Close");

          sinon.assert.notCalled(requestStubs.SetNameNewRoom);});


        it("should end up updating closingNewRoom state to false", function () {
          store.setStoreState({ closingNewRoom: "true" });
          LoopMochaUtils.publish("Rooms:Close");

          expect(store.getStoreState().closingNewRoom).to.eql(false);});


        it("should update the lastCreatedRoom state to null", function () {
          store.setStoreState({ lastCreatedRoom: "fake1234" });
          LoopMochaUtils.publish("Rooms:Close");

          expect(store.getStoreState().lastCreatedRoom).to.eql(null);});


        it("should update the openedRoom state to null", function () {
          store.setStoreState({ openedRoom: "fake1234" });
          LoopMochaUtils.publish("Rooms:Close");

          expect(store.getStoreState().openedRoom).to.eql(null);});});



      describe("open", function () {
        it("should update the openedRoom state to the room token", function () {
          LoopMochaUtils.publish("Rooms:Open", "fake1234");

          expect(store.getStoreState().openedRoom).to.eql("fake1234");});});



      describe("update", function () {
        it("should update a room entry", function () {
          LoopMochaUtils.publish("Rooms:Update", { 
            roomToken: "_nxD4V4FflQ", 
            roomUrl: "http://sample/_nxD4V4FflQ", 
            roomName: "Changed First Room Name", 
            maxSize: 2, 
            participants: [], 
            ctime: 1405517546 });


          expect(store.getStoreState().rooms).to.have.length.of(3);
          expect(store.getStoreState().rooms.some(function (room) {
            return room.roomName === "Changed First Room Name";})).
          eql(true);});});



      describe("delete", function () {
        it("should delete a room from the list", function () {
          LoopMochaUtils.publish("Rooms:Delete", { 
            roomToken: "_nxD4V4FflQ" });


          expect(store.getStoreState().rooms).to.have.length.of(2);
          expect(store.getStoreState().rooms.some(function (room) {
            return room.roomToken === "_nxD4V4FflQ";})).
          eql(false);});});



      describe("refresh", function () {
        it("should clear the list of rooms", function () {
          LoopMochaUtils.publish("Rooms:Refresh");

          expect(store.getStoreState().rooms).to.have.length.of(0);});});});




    describe("#createRoom", function () {
      var fakeRoomCreationData;

      beforeEach(function () {
        sandbox.stub(dispatcher, "dispatch");
        store.setStoreState({ pendingCreation: false, rooms: [] });
        fakeRoomCreationData = {};});


      it("should clear any existing room errors", function () {
        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledOnce(fakeNotifications.remove);
        sinon.assert.calledWithExactly(fakeNotifications.remove, 
        "create-room-error");});


      it("should request creation of a new room", function () {
        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledWith(requestStubs["Rooms:Create"], { 
          decryptedContext: {}, 
          maxSize: store.maxRoomCreationSize });});



      it("should request creation of a new room with context", function () {
        fakeRoomCreationData.urls = [{ 
          location: "http://invalid.com", 
          description: "fakeSite", 
          thumbnail: "fakeimage.png" }];


        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledWith(requestStubs["Rooms:Create"], { 
          decryptedContext: { 
            urls: [{ 
              location: "http://invalid.com", 
              description: "fakeSite", 
              thumbnail: "fakeimage.png" }] }, 


          maxSize: store.maxRoomCreationSize });});



      it("should switch the pendingCreation state flag to true", function () {
        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        expect(store.getStoreState().pendingCreation).eql(true);});


      it("should store the room token as the last created", function () {
        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        expect(store.getStoreState().lastCreatedRoom).eql("fakeToken");});


      it("should dispatch a CreatedRoom action once the operation is done", 
      function () {
        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.CreatedRoom({ 
          decryptedContext: [], 
          roomToken: "fakeToken", 
          roomUrl: "fakeUrl" }));});



      it("should dispatch a CreateRoomError action if the operation fails", 
      function () {
        var err = new Error("fake");
        err.isError = true;
        requestStubs["Rooms:Create"].returns(err);

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.CreateRoomError({ 
          error: err }));});



      it("should dispatch a CreateRoomError action if the operation fails with no result", 
      function () {
        requestStubs["Rooms:Create"].returns();

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.CreateRoomError({ 
          error: new Error("no result") }));});



      it("should log a telemetry event when the operation is successful", function () {
        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledOnce(requestStubs.TelemetryAddValue);
        sinon.assert.calledWithExactly(requestStubs.TelemetryAddValue, 
        "LOOP_ROOM_CREATE", 0);});


      it("should log a telemetry event when the operation fails", function () {
        var err = new Error("fake");
        err.isError = true;
        requestStubs["Rooms:Create"].returns(err);

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledOnce(requestStubs.TelemetryAddValue);
        sinon.assert.calledWithExactly(requestStubs.TelemetryAddValue, 
        "LOOP_ROOM_CREATE", 1);});


      it("should log a telemetry event when the operation fails with no result", function () {
        requestStubs["Rooms:Create"].returns();

        store.createRoom(new sharedActions.CreateRoom(fakeRoomCreationData));

        sinon.assert.calledOnce(requestStubs.TelemetryAddValue);
        sinon.assert.calledWithExactly(requestStubs.TelemetryAddValue, 
        "LOOP_ROOM_CREATE", 1);});});



    describe("#createdRoom", function () {
      beforeEach(function () {
        sandbox.stub(dispatcher, "dispatch");});


      it("should switch the pendingCreation state flag to false", function () {
        store.setStoreState({ pendingCreation: true });

        store.createdRoom(new sharedActions.CreatedRoom({ 
          decryptedContext: [], 
          roomToken: "fakeToken", 
          roomUrl: "fakeUrl" }));


        expect(store.getStoreState().pendingCreation).eql(false);});


      it("should not dispatch an OpenRoom action once the operation is done", 
      function () {
        store.createdRoom(new sharedActions.CreatedRoom({ 
          decryptedContext: [], 
          roomToken: "fakeToken", 
          roomUrl: "fakeUrl" }));


        sinon.assert.notCalled(dispatcher.dispatch);});


      it("should save the state of the active room", function () {
        store.createdRoom(new sharedActions.CreatedRoom({ 
          decryptedContext: [], 
          roomToken: "fakeToken", 
          roomUrl: "fakeUrl" }));


        expect(store.getStoreState().activeRoom).eql({ 
          decryptedContext: [], 
          roomToken: "fakeToken", 
          roomUrl: "fakeUrl" });});});




    describe("#createRoomError", function () {
      it("should switch the pendingCreation state flag to false", function () {
        store.setStoreState({ pendingCreation: true });

        store.createRoomError({ 
          error: new Error("fake") });


        expect(store.getStoreState().pendingCreation).eql(false);});


      it("should set a notification", function () {
        store.createRoomError({ 
          error: new Error("fake") });


        sinon.assert.calledOnce(fakeNotifications.set);
        sinon.assert.calledWithMatch(fakeNotifications.set, { 
          id: "create-room-error", 
          level: "error" });});});




    describe("#deleteRoom", function () {
      var fakeRoomToken = "42abc";

      beforeEach(function () {
        sandbox.stub(dispatcher, "dispatch");});


      it("should request deletion of a room", function () {
        store.deleteRoom(new sharedActions.DeleteRoom({ 
          roomToken: fakeRoomToken }));


        sinon.assert.calledWith(requestStubs["Rooms:Delete"], fakeRoomToken);});


      it("should dispatch a DeleteRoomError action if the operation fails", function () {
        var err = new Error("fake");
        err.isError = true;
        requestStubs["Rooms:Delete"].returns(err);

        store.deleteRoom(new sharedActions.DeleteRoom({ 
          roomToken: fakeRoomToken }));


        sinon.assert.calledOnce(dispatcher.dispatch);
        sinon.assert.calledWithExactly(dispatcher.dispatch, 
        new sharedActions.DeleteRoomError({ 
          error: err }));});



      it("should log a telemetry event when the operation is successful", function () {
        store.deleteRoom(new sharedActions.DeleteRoom({ 
          roomToken: fakeRoomToken }));


        sinon.assert.calledTwice(requestStubs.TelemetryAddValue);
        sinon.assert.calledWithExactly(requestStubs.TelemetryAddValue.getCall(0), 
        "LOOP_ROOM_DELETE", 0);});


      it("should log a telemetry event when the operation fails", function () {
        var err = new Error("fake");
        err.isError = true;
        requestStubs["Rooms:Delete"].returns(err);

        store.deleteRoom(new sharedActions.DeleteRoom({ 
          roomToken: fakeRoomToken }));


        sinon.assert.calledTwice(requestStubs.TelemetryAddValue);
        sinon.assert.calledWithExactly(requestStubs.TelemetryAddValue.getCall(0), 
        "LOOP_ROOM_DELETE", 1);});});



    describe("#copyRoomUrl", function () {
      it("should copy the room URL", function () {
        store.copyRoomUrl(new sharedActions.CopyRoomUrl({ 
          roomUrl: "http://invalid", 
          from: "conversation" }));


        sinon.assert.calledOnce(requestStubs.CopyString);
        sinon.assert.calledWithExactly(requestStubs.CopyString, "http://invalid");});


      it("should send a telemetry event for copy from panel", function () {
        store.copyRoomUrl(new sharedActions.CopyRoomUrl({ 
          roomUrl: "http://invalid", 
          from: "panel" }));


        sinon.assert.calledTwice(requestStubs.TelemetryAddValue);
        sinon.assert.calledWithExactly(requestStubs.TelemetryAddValue.getCall(0), 
        "LOOP_SHARING_ROOM_URL", 0);});


      it("should send a telemetry event for copy from conversation", function () {
        store.copyRoomUrl(new sharedActions.CopyRoomUrl({ 
          roomUrl: "http://invalid", 
          from: "conversation" }));


        sinon.assert.calledTwice(requestStubs.TelemetryAddValue);
        sinon.assert.calledWithExactly(requestStubs.TelemetryAddValue.getCall(0), 
        "LOOP_SHARING_ROOM_URL", 1);});});



    describe("#emailRoomUrl", function () {
      it("should call composeCallUrlEmail to email the url", function () {
        sandbox.stub(sharedUtils, "composeCallUrlEmail");

        store.emailRoomUrl(new sharedActions.EmailRoomUrl({ 
          roomUrl: "http://invalid", 
          from: "conversation" }));


        sinon.assert.calledOnce(sharedUtils.composeCallUrlEmail);
        sinon.assert.calledWith(sharedUtils.composeCallUrlEmail, 
        "http://invalid", null, undefined);});


      it("should call composeUrlEmail differently with context", function () {
        sandbox.stub(sharedUtils, "composeCallUrlEmail");

        var url = "http://invalid";
        var description = "Hello, is it me you're looking for?";
        store.emailRoomUrl(new sharedActions.EmailRoomUrl({ 
          roomUrl: url, 
          roomDescription: description, 
          from: "conversation" }));


        sinon.assert.calledOnce(sharedUtils.composeCallUrlEmail);
        sinon.assert.calledWithExactly(sharedUtils.composeCallUrlEmail, 
        url, null, description);});});



    describe("#facebookShareRoomUrl", function () {
      var getLoopPrefStub;
      var sharingSite = "www.sharing-site.com", 
      shareURL = sharingSite + 
      "?app_id=%APP_ID%" + 
      "&link=%ROOM_URL%" + 
      "&redirect_uri=%REDIRECT_URI%", 
      appId = "1234567890", 
      fallback = "www.fallback.com";

      beforeEach(function () {
        getLoopPrefStub = sinon.stub();
        getLoopPrefStub.withArgs("facebook.appId").returns(appId);
        getLoopPrefStub.withArgs("facebook.shareUrl").returns(shareURL);
        getLoopPrefStub.withArgs("facebook.fallbackUrl").returns(fallback);

        LoopMochaUtils.stubLoopRequest({ 
          GetLoopPref: getLoopPrefStub });});



      it("should open the facebook share url with correct room and redirection", function () {
        var room = "invalid.room", 
        origin = "origin.url";

        store.facebookShareRoomUrl(new sharedActions.FacebookShareRoomUrl({ 
          from: "conversation", 
          originUrl: origin, 
          roomUrl: room }));


        sinon.assert.calledOnce(requestStubs.OpenURL);
        sinon.assert.calledWithMatch(requestStubs.OpenURL, sharingSite);
        sinon.assert.calledWithMatch(requestStubs.OpenURL, room);
        sinon.assert.calledWithMatch(requestStubs.OpenURL, origin);});


      it("if no origin URL, send fallback URL", function () {
        var room = "invalid.room";

        store.facebookShareRoomUrl(new sharedActions.FacebookShareRoomUrl({ 
          from: "conversation", 
          roomUrl: room }));


        sinon.assert.calledOnce(requestStubs.OpenURL);
        sinon.assert.calledWithMatch(requestStubs.OpenURL, sharingSite);
        sinon.assert.calledWithMatch(requestStubs.OpenURL, room);
        sinon.assert.calledWithMatch(requestStubs.OpenURL, fallback);});


      it("should send a telemetry event for facebook share from conversation", function () {
        store.facebookShareRoomUrl(new sharedActions.FacebookShareRoomUrl({ 
          from: "conversation", 
          roomUrl: "http://invalid" }));


        sinon.assert.calledTwice(requestStubs.TelemetryAddValue);
        sinon.assert.calledWithExactly(requestStubs.TelemetryAddValue.getCall(0), 
        "LOOP_SHARING_ROOM_URL", 4);});});



    describe("#shareRoomUrl", function () {
      var socialShareRoomStub;

      beforeEach(function () {
        socialShareRoomStub = sinon.stub();
        LoopMochaUtils.stubLoopRequest({ 
          SocialShareRoom: socialShareRoomStub });});



      it("should pass the correct data for GMail sharing", function () {
        var roomUrl = "http://invalid";
        var origin = "https://mail.google.com/v1";
        store.shareRoomUrl(new sharedActions.ShareRoomUrl({ 
          roomUrl: roomUrl, 
          provider: { 
            origin: origin } }));



        sinon.assert.calledOnce(socialShareRoomStub);
        sinon.assert.calledWithExactly(socialShareRoomStub, 
        origin, 
        roomUrl, 
        "share_email_subject7", 
        "share_email_body7" + 
        "share_email_footer2");});


      it("should pass the correct data for all other Social Providers", function () {
        var roomUrl = "http://invalid2";
        var origin = "https://twitter.com/share";
        store.shareRoomUrl(new sharedActions.ShareRoomUrl({ 
          roomUrl: roomUrl, 
          provider: { 
            origin: origin } }));



        sinon.assert.calledOnce(socialShareRoomStub);
        sinon.assert.calledWithExactly(socialShareRoomStub, origin, 
        roomUrl, "share_tweet", null);});});



    describe("#addSocialShareProvider", function () {
      it("should invoke to the correct mozLoop function", function () {
        var stub = sinon.stub();
        LoopMochaUtils.stubLoopRequest({ 
          AddSocialShareProvider: stub });


        store.addSocialShareProvider(new sharedActions.AddSocialShareProvider());

        sinon.assert.calledOnce(stub);});});



    describe("#setStoreState", function () {
      it("should update store state data", function () {
        store.setStoreState({ pendingCreation: true });

        expect(store.getStoreState().pendingCreation).eql(true);});


      it("should trigger a `change` event", function (done) {
        store.once("change", function () {
          done();});


        store.setStoreState({ pendingCreation: true });});


      it("should trigger a `change:<prop>` event", function (done) {
        store.once("change:pendingCreation", function () {
          done();});


        store.setStoreState({ pendingCreation: true });});});



    describe("#getAllRooms", function () {
      it("should fetch the room list from the Loop API", function () {
        requestStubs["Rooms:GetAll"].returns(fakeRoomList);

        store.getAllRooms(new sharedActions.GetAllRooms());

        expect(store.getStoreState().error).to.be.a.undefined;
        expect(store.getStoreState().rooms).to.have.length.of(3);});


      it("should not fetch the room list if called a second time", function () {
        requestStubs["Rooms:GetAll"].returns(fakeRoomList);

        store.getAllRooms(new sharedActions.GetAllRooms());
        store.getAllRooms(new sharedActions.GetAllRooms());

        sinon.assert.calledOnce(requestStubs["Rooms:GetAll"]);});


      it("should order the room list using ctime desc", function () {
        requestStubs["Rooms:GetAll"].returns(fakeRoomList);

        store.getAllRooms(new sharedActions.GetAllRooms());

        var storeState = store.getStoreState();
        expect(storeState.error).to.be.a.undefined;
        expect(storeState.rooms[0].ctime).eql(1405518241);
        expect(storeState.rooms[1].ctime).eql(1405517546);
        expect(storeState.rooms[2].ctime).eql(1405517418);});


      it("should report an error", function () {
        var err = new Error("fake");
        err.isError = true;
        requestStubs["Rooms:GetAll"].returns(err);

        dispatcher.dispatch(new sharedActions.GetAllRooms());

        expect(store.getStoreState().error).eql(err);});


      it("should register event listeners after the list is retrieved", 
      function () {
        sandbox.stub(store, "startListeningToRoomEvents");
        requestStubs["Rooms:GetAll"].returns(fakeRoomList);

        store.getAllRooms();

        sinon.assert.calledOnce(store.startListeningToRoomEvents);});


      it("should set the pendingInitialRetrieval flag to true", function () {
        expect(store.getStoreState().pendingInitialRetrieval).eql(true);

        requestStubs["Rooms:GetAll"].returns([]);
        store.getAllRooms();

        expect(store.getStoreState().pendingInitialRetrieval).eql(false);});


      it("should set pendingInitialRetrieval to false once the action is performed", 
      function () {
        requestStubs["Rooms:GetAll"].returns(fakeRoomList);

        store.getAllRooms();

        expect(store.getStoreState().pendingInitialRetrieval).eql(false);});});



    describe("ActiveRoomStore substore", function () {
      var fakeStore, activeRoomStore;

      beforeEach(function () {
        activeRoomStore = new loop.store.ActiveRoomStore(dispatcher, { 
          sdkDriver: {} });

        fakeStore = new loop.store.RoomStore(dispatcher, { 
          constants: requestStubs.GetAllConstants(), 
          activeRoomStore: activeRoomStore });});



      it("should subscribe to substore changes", function () {
        var fakeServerData = { fake: true };

        activeRoomStore.setStoreState({ serverData: fakeServerData });

        expect(fakeStore.getStoreState().activeRoom.serverData).
        eql(fakeServerData);});


      it("should trigger a change event when the substore is updated", 
      function (done) {
        fakeStore.once("change:activeRoom", function () {
          done();});


        activeRoomStore.setStoreState({ serverData: {} });});});});




  describe("#openRoom", function () {
    var store;

    beforeEach(function () {
      store = new loop.store.RoomStore(dispatcher, { 
        constants: requestStubs.GetAllConstants() });});



    it("should open the room via mozLoop", function () {
      store.openRoom(new sharedActions.OpenRoom({ roomToken: "42abc" }));

      sinon.assert.calledOnce(requestStubs["Rooms:Open"]);
      sinon.assert.calledWithExactly(requestStubs["Rooms:Open"], "42abc");});});



  describe("#updateRoomContext", function () {
    var store, getRoomStub, updateRoomStub, clock;

    beforeEach(function () {
      clock = sinon.useFakeTimers();
      getRoomStub = sinon.stub().returns({ 
        roomToken: "42abc", 
        decryptedContext: { 
          roomName: "sillier name" } });


      updateRoomStub = sinon.stub();
      LoopMochaUtils.stubLoopRequest({ 
        "Rooms:Get": getRoomStub, 
        "Rooms:Update": updateRoomStub });

      store = new loop.store.RoomStore(dispatcher, { 
        constants: requestStubs.GetAllConstants() });});



    afterEach(function () {
      clock.restore();});


    it("should rename the room via mozLoop", function () {
      dispatcher.dispatch(new sharedActions.UpdateRoomContext({ 
        roomToken: "42abc", 
        newRoomName: "silly name" }));


      sinon.assert.calledOnce(getRoomStub);
      sinon.assert.calledOnce(updateRoomStub);
      sinon.assert.calledWith(updateRoomStub, "42abc", { 
        roomName: "silly name" });});



    it("should flag the the store as saving context", function () {
      expect(store.getStoreState().savingContext).to.eql(false);

      LoopMochaUtils.stubLoopRequest({ 
        "Rooms:Update": function RoomsUpdate() {
          expect(store.getStoreState().savingContext).to.eql(true);} });



      dispatcher.dispatch(new sharedActions.UpdateRoomContext({ 
        roomToken: "42abc", 
        newRoomName: "silly name" }));


      expect(store.getStoreState().savingContext).to.eql(false);});


    it("should store any update-encountered error", function () {
      var err = new Error("fake");
      err.isError = true;

      LoopMochaUtils.stubLoopRequest({ 
        "Rooms:Update": function RoomsUpdate() {
          expect(store.getStoreState().savingContext).to.eql(true);
          return err;} });



      dispatcher.dispatch(new sharedActions.UpdateRoomContext({ 
        roomToken: "42abc", 
        newRoomName: "silly name" }));


      var state = store.getStoreState();
      expect(state.error).eql(err);
      expect(state.savingContext).to.eql(false);});


    it("should ensure only submitting a non-empty room name", function () {
      dispatcher.dispatch(new sharedActions.UpdateRoomContext({ 
        roomToken: "42abc", 
        newRoomName: " \t  \t " }));

      clock.tick(1);

      sinon.assert.notCalled(updateRoomStub);
      expect(store.getStoreState().savingContext).to.eql(false);});


    it("should save updated context information", function () {
      dispatcher.dispatch(new sharedActions.UpdateRoomContext({ 
        roomToken: "42abc", 
        // Room name doesn't need to change.
        newRoomName: "sillier name", 
        newRoomDescription: "Hello, is it me you're looking for?", 
        newRoomThumbnail: "http://example.com/empty.gif", 
        newRoomURL: "http://example.com" }));


      sinon.assert.calledOnce(updateRoomStub);
      sinon.assert.calledWith(updateRoomStub, "42abc", { 
        urls: [{ 
          description: "Hello, is it me you're looking for?", 
          location: "http://example.com", 
          thumbnail: "http://example.com/empty.gif" }] });});




    it("should not save context information with an invalid URL", function () {
      dispatcher.dispatch(new sharedActions.UpdateRoomContext({ 
        roomToken: "42abc", 
        // Room name doesn't need to change.
        newRoomName: "sillier name", 
        newRoomDescription: "Hello, is it me you're looking for?", 
        newRoomThumbnail: "http://example.com/empty.gif", 
        // NOTE: there are many variation we could test here, but the URL object
        // constructor also fails on empty strings and is using the Gecko URL
        // parser. Therefore we ought to rely on it working properly.
        newRoomURL: "http/example.com" }));


      sinon.assert.notCalled(updateRoomStub);});


    it("should not save context information when no context information is provided", 
    function () {
      dispatcher.dispatch(new sharedActions.UpdateRoomContext({ 
        roomToken: "42abc", 
        // Room name doesn't need to change.
        newRoomName: "sillier name", 
        newRoomDescription: "", 
        newRoomThumbnail: "", 
        newRoomURL: "" }));

      clock.tick(1);

      sinon.assert.notCalled(updateRoomStub);
      expect(store.getStoreState().savingContext).to.eql(false);});});



  describe("MAU telemetry events", function () {
    var getLoopPrefStub, store;

    beforeEach(function () {
      getLoopPrefStub = function getLoopPrefStub(pref) {
        if (pref === "facebook.shareUrl") {
          return "https://shared.site/?u=%ROOM_URL%";}

        return 0;};


      LoopMochaUtils.stubLoopRequest({ 
        GetLoopPref: getLoopPrefStub });


      store = new loop.store.RoomStore(dispatcher, { 
        constants: requestStubs.GetAllConstants() });});



    it("should log telemetry event when opening a room", function () {
      store.openRoom(new sharedActions.OpenRoom({ roomToken: "42abc" }));

      sinon.assert.calledOnce(requestStubs["TelemetryAddValue"]);
      sinon.assert.calledWithExactly(requestStubs["TelemetryAddValue"], 
      "LOOP_ACTIVITY_COUNTER", store._constants.LOOP_MAU_TYPE.ROOM_OPEN);});


    it("should log telemetry event when sharing a room (copy link)", function () {
      store.copyRoomUrl(new sharedActions.CopyRoomUrl({ 
        roomUrl: "http://invalid", 
        from: "conversation" }));


      sinon.assert.calledTwice(requestStubs["TelemetryAddValue"]);
      sinon.assert.calledWithExactly(requestStubs["TelemetryAddValue"].getCall(1), 
      "LOOP_ACTIVITY_COUNTER", store._constants.LOOP_MAU_TYPE.ROOM_SHARE);});


    it("should log telemetry event when sharing a room (email)", function () {
      store.emailRoomUrl(new sharedActions.EmailRoomUrl({ 
        roomUrl: "http://invalid", 
        from: "conversation" }));


      sinon.assert.calledTwice(requestStubs["TelemetryAddValue"]);
      sinon.assert.calledWithExactly(requestStubs["TelemetryAddValue"].getCall(1), 
      "LOOP_ACTIVITY_COUNTER", store._constants.LOOP_MAU_TYPE.ROOM_SHARE);});


    it("should log telemetry event when sharing a room (facebook)", function () {
      store.facebookShareRoomUrl(new sharedActions.FacebookShareRoomUrl({ 
        roomUrl: "http://invalid", 
        from: "conversation" }));


      sinon.assert.calledTwice(requestStubs["TelemetryAddValue"]);
      sinon.assert.calledWithExactly(requestStubs["TelemetryAddValue"].getCall(1), 
      "LOOP_ACTIVITY_COUNTER", store._constants.LOOP_MAU_TYPE.ROOM_SHARE);});


    it("should log telemetry event when deleting a room", function () {
      store.deleteRoom(new sharedActions.DeleteRoom({ 
        roomToken: "42abc" }));


      sinon.assert.calledTwice(requestStubs["TelemetryAddValue"]);
      sinon.assert.calledWithExactly(requestStubs["TelemetryAddValue"].getCall(1), 
      "LOOP_ACTIVITY_COUNTER", store._constants.LOOP_MAU_TYPE.ROOM_DELETE);});});});
