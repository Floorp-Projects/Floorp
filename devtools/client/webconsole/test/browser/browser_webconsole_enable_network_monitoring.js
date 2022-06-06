// Test that the "Enable Network Monitoring" checkbox item is not available
// in the webconsole.

"use strict";

add_task(async function testEnableNetworkMonitoringInWebConsole() {
  const hud = await openNewTabAndConsole(
    `data:text/html,<!DOCTYPE html><script>foo;</script>`
  );

  const enableNetworkMonitoringItem = getConsoleSettingElement(
    hud,
    ".webconsole-console-settings-menu-item-enableNetworkMonitoring"
  );
  ok(
    !enableNetworkMonitoringItem,
    "The 'Enable Network Monitoring' setting item should not be avaliable in the webconsole"
  );

  await closeConsole();
});
