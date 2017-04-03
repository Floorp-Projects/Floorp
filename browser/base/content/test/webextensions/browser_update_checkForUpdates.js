// Invoke the "Check for Updates" menu item
function checkAll(win) {
  win.gViewController.doCommand("cmd_findAllUpdates");
  return new Promise(resolve => {
    let observer = {
      observe(subject, topic, data) {
        Services.obs.removeObserver(observer, "EM-update-check-finished");
        resolve();
      },
    };
    Services.obs.addObserver(observer, "EM-update-check-finished", false);
  });
}

// Test "Check for Updates" with both auto-update settings
add_task(() => interactiveUpdateTest(true, checkAll));
add_task(() => interactiveUpdateTest(false, checkAll));
