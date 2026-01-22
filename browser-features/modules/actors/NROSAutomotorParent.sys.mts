/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { osAutomotorManager } = ChromeUtils.importESModule(
  "resource://noraneko/modules/os-automotor/OSAutomotor-manager.sys.mjs",
);

const { osFrontEndManager } = ChromeUtils.importESModule(
  "resource://noraneko/modules/os-automotor/OSFrontEndManager.sys.mjs",
);

export class NROSAutomotorParent extends JSWindowActorParent {
  receiveMessage(message: ReceiveMessageArgument) {
    try {
      const { name, data } = message;

      switch (name) {
        case "OSAutomotor:IsPlatformSupported":
          return String(osAutomotorManager.isPlatformSupported());

        case "OSAutomotor:IsEnabled":
          return String(osAutomotorManager.isEnabled());

        case "OSAutomotor:GetInstalledVersion":
          return String(osAutomotorManager.getInstalledVersion());

        case "OSAutomotor:Enable":
          return osAutomotorManager
            .enableFloorpOS()
            .then(() =>
              JSON.stringify({
                success: true,
              }),
            )
            .catch((error: Error) =>
              JSON.stringify({
                success: false,
                error: error.message,
              }),
            );

        case "OSAutomotor:Disable":
          return osAutomotorManager
            .disableFloorpOS()
            .then(() => JSON.stringify({ success: true }))
            .catch((error: Error) =>
              JSON.stringify({
                success: false,
                error: error.message,
              }),
            );

        case "OSAutomotor:GetStatus":
          return JSON.stringify({
            enabled: osAutomotorManager.isEnabled(),
            platformSupported: osAutomotorManager.isPlatformSupported(),
            installedVersion: osAutomotorManager.getInstalledVersion(),
          });

        case "OSAutomotor:GetPlatformDebugInfo":
          return JSON.stringify(osAutomotorManager.getPlatformDebugInfo());

        // Workflow Progress Window messages
        case "OSAutomotor:WorkflowProgress":
          try {
            osFrontEndManager.handleProgressMessage(data);
            return JSON.stringify({ success: true });
          } catch (error) {
            console.error("[OSAutomotor Actor] Progress error:", error);
            return JSON.stringify({
              success: false,
              error: error instanceof Error ? error.message : String(error),
            });
          }

        case "OSAutomotor:GetWorkflowProgress":
          try {
            const progress = osFrontEndManager.getCurrentProgress();
            return JSON.stringify({ success: true, progress });
          } catch (error) {
            console.error("[OSAutomotor Actor] GetProgress error:", error);
            return JSON.stringify({
              success: false,
              error: error instanceof Error ? error.message : String(error),
            });
          }

        case "OSAutomotor:CloseProgressWindow":
          try {
            osFrontEndManager.closeProgressWindow();
            return JSON.stringify({ success: true });
          } catch (error) {
            console.error("[OSAutomotor Actor] CloseWindow error:", error);
            return JSON.stringify({
              success: false,
              error: error instanceof Error ? error.message : String(error),
            });
          }

        default:
          console.warn(`[OSAutomotor Actor] Unknown message: ${name}`);
          return null;
      }
    } catch (error) {
      console.error("[OSAutomotor Actor] Error in receiveMessage:", error);
      return JSON.stringify({
        success: false,
        error: error instanceof Error ? error.message : String(error),
      });
    }
  }
}

