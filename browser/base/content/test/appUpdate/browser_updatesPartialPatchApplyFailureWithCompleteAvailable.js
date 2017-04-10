add_task(function* testPartialPatchApplyFailureWithCompleteAvailable() {
  let patches = getLocalPatchString("partial", null, null, null, null, null,
                                    STATE_PENDING) +
                getLocalPatchString("complete", null, null, null,
                                    null, "false");

  let promptWaitTime = "0";
  let updates = getLocalUpdateString(patches, null, null, null,
                                     Services.appinfo.version, null,
                                     null, null, null, null, "false",
                                     null, null, null, null, promptWaitTime);

  yield runUpdateProcessingTest(updates, [
    {
      notificationId: "update-restart",
      button: "secondarybutton",
      cleanup() {
        PanelUI.removeNotification(/.*/);
      }
    },
  ]);
});
