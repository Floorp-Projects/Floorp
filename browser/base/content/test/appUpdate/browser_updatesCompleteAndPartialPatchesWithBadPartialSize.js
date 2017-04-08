add_task(function* testCompleteAndPartialPatchesWithBadPartialSize() {
  let updateParams = "invalidPartialSize=1&promptWaitTime=0";

  yield runUpdateTest(updateParams, 1, [
    {
      notificationId: "update-restart",
      button: "secondarybutton",
      cleanup() {
        PanelUI.removeNotification(/.*/);
      }
    },
  ]);
});
