"use strict"; /* This Source Code Form is subject to the terms of the Mozilla Public
               * License, v. 2.0. If a copy of the MPL was not distributed with this file,
               * You can obtain one at http://mozilla.org/MPL/2.0/. */

var loop = loop || {};
loop.store = loop.store || {};

/**
 * Manages the conversation window app controller view. Used to get
 * the window data and store the window type.
 */
loop.store.ConversationAppStore = function () {
  "use strict";

  /**
   * Constructor
   *
   * @param {Object} options Options for the store. Should contain the
   *                         activeRoomStore and dispatcher objects.
   */
  var ConversationAppStore = loop.store.createStore({ 
    initialize: function initialize(options) {
      if (!options.activeRoomStore) {
        throw new Error("Missing option activeRoomStore");}

      if (!("feedbackPeriod" in options)) {
        throw new Error("Missing option feedbackPeriod");}

      if (!("feedbackTimestamp" in options)) {
        throw new Error("Missing option feedbackTimestamp");}

      this._activeRoomStore = options.activeRoomStore;
      this._facebookEnabled = options.facebookEnabled;
      this._feedbackPeriod = options.feedbackPeriod;
      this._feedbackTimestamp = options.feedbackTimestamp;
      this._rootObj = "rootObject" in options ? options.rootObject : window;
      this._storeState = this.getInitialStoreState();

      // Start listening for specific events, coming from the window object.
      this._eventHandlers = {};
      ["unload", "LoopHangupNow", "socialFrameAttached", "socialFrameDetached", "ToggleBrowserSharing"].
      forEach(function (eventName) {
        var handlerName = eventName + "Handler";
        this._eventHandlers[eventName] = this[handlerName].bind(this);
        this._rootObj.addEventListener(eventName, this._eventHandlers[eventName]);}.
      bind(this));

      this.dispatcher.register(this, [
      "getWindowData", 
      "showFeedbackForm", 
      "leaveConversation"]);}, 



    getInitialStoreState: function getInitialStoreState() {
      return { 
        chatWindowDetached: false, 
        facebookEnabled: this._facebookEnabled, 
        // How often to display the form. Convert seconds to ms.
        feedbackPeriod: this._feedbackPeriod * 1000, 
        // Date when the feedback form was last presented. Convert to ms.
        feedbackTimestamp: this._feedbackTimestamp * 1000, 
        showFeedbackForm: false };}, 



    /**
     * Retrieves current store state.
     *
     * @return {Object}
     */
    getStoreState: function getStoreState() {
      return this._storeState;}, 


    /**
     * Updates store states and trigger a "change" event.
     *
     * @param {Object} state The new store state.
     */
    setStoreState: function setStoreState(state) {
      this._storeState = _.extend({}, this._storeState, state);
      this.trigger("change");}, 


    /**
     * Sets store state which will result in the feedback form rendered.
     * Saves a timestamp of when the feedback was last rendered.
     */
    showFeedbackForm: function showFeedbackForm() {
      var timestamp = Math.floor(new Date().getTime() / 1000);

      loop.request("SetLoopPref", "feedback.dateLastSeenSec", timestamp);

      this.setStoreState({ 
        showFeedbackForm: true });}, 



    /**
     * Handles the get window data action - obtains the window data,
     * updates the store and notifies interested components.
     *
     * @param {sharedActions.GetWindowData} actionData The action data
     */
    getWindowData: function getWindowData(actionData) {
      var windowData = loop.getStoredRequest(["GetConversationWindowData", 
      actionData.windowId]);

      if (!windowData) {
        console.error("Failed to get the window data");
        this.setStoreState({ windowType: "failed" });
        return;}


      this.setStoreState({ windowType: windowData.type });

      this.dispatcher.dispatch(new loop.shared.actions.SetupWindowData(_.extend({ 
        windowId: actionData.windowId }, windowData)));}, 


    /**
     * Event handler; invoked when the 'unload' event is dispatched from the
     * window object.
     * It will dispatch a 'WindowUnload' action that other stores may listen to
     * and will remove all event handlers attached to the window object.
     */
    unloadHandler: function unloadHandler() {
      this.dispatcher.dispatch(new loop.shared.actions.WindowUnload());

      // Unregister event handlers.
      var eventNames = Object.getOwnPropertyNames(this._eventHandlers);
      eventNames.forEach(function (eventName) {
        this._rootObj.removeEventListener(eventName, this._eventHandlers[eventName]);}.
      bind(this));
      this._eventHandlers = null;}, 


    /**
     * Event handler; invoked when the 'LoopHangupNow' event is dispatched from
     * the window object.
     * It'll attempt to gracefully disconnect from an active session, or close
     * the window when no session is currently active.
     */
    LoopHangupNowHandler: function LoopHangupNowHandler() {
      this.dispatcher.dispatch(new loop.shared.actions.LeaveConversation());}, 


    /**
     * Handles leaving the conversation, displaying the feedback form if it
     * is time to.
     */
    leaveConversation: function leaveConversation() {
      if (this.getStoreState().windowType !== "room" || 
      !this._activeRoomStore.getStoreState().used || 
      this.getStoreState().showFeedbackForm) {
        loop.shared.mixins.WindowCloseMixin.closeWindow();
        return;}


      var delta = new Date() - new Date(this.getStoreState().feedbackTimestamp);

      // Show timestamp if feedback period (6 months) passed.
      // 0 is default value for pref. Always show feedback form on first use.
      if (this.getStoreState().feedbackTimestamp === 0 || 
      delta >= this.getStoreState().feedbackPeriod) {
        this.dispatcher.dispatch(new loop.shared.actions.LeaveRoom({ 
          windowStayingOpen: true }));

        this.dispatcher.dispatch(new loop.shared.actions.ShowFeedbackForm());
        return;}


      loop.shared.mixins.WindowCloseMixin.closeWindow();}, 


    /**
     * Event handler; invoked when the 'PauseScreenShare' event is dispatched from
     * the window object.
     * It'll attempt to pause or resume the screen share as appropriate.
     */
    ToggleBrowserSharingHandler: function ToggleBrowserSharingHandler(actionData) {
      this.dispatcher.dispatch(new loop.shared.actions.ToggleBrowserSharing({ 
        enabled: !actionData.detail }));}, 



    /**
     * Event handler; invoked when the 'socialFrameAttached' event is dispatched
     * from the window object.
     */
    socialFrameAttachedHandler: function socialFrameAttachedHandler() {
      this.setStoreState({ chatWindowDetached: false });}, 


    /**
     * Event handler; invoked when the 'socialFrameDetached' event is dispatched
     * from the window object.
     */
    socialFrameDetachedHandler: function socialFrameDetachedHandler() {
      this.setStoreState({ chatWindowDetached: true });} });



  return ConversationAppStore;}();
