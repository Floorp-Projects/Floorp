import { calls, expose } from "../../../../libs/ui/test/page-fixture.ts";
import type { InstalledApp } from "../../src/types/pref.ts";
export const management = {
  fail: "",
  osEnabled: false,
  apps: {} as Record<string, InstalledApp>,
};
export function setupManagement() {
  expose("NROpenCurrentProfileDirectory", (callback: (ok: boolean) => void) => {
    calls.push({ method: "NROpenCurrentProfileDirectory", args: [] });
    callback(management.fail !== "profile");
  });
  management.apps = {
    test: {
      id: "test",
      name: "Test app",
      start_url: "https://example.invalid/",
      icon: "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg'/%3E",
      userContextId: 0,
    } as InstalledApp,
  };
  for (
    const [name, value] of Object.entries({
      NRGetContainers: [{
        userContextId: 1,
        name: "Test container",
        color: "#123456",
      }],
      NRGetContainerContexts: [{
        id: "1",
        name: "Test container",
        color: "blue",
      }],
      NRGetStaticPanels: [{ id: "floorp//bookmarks", title: "Bookmarks" }],
      NRGetExtensionPanels: [{
        value: "test@example.invalid",
        label: "Test extension",
      }],
    })
  ) {
    expose(
      name,
      (callback: (data: string) => void) => callback(JSON.stringify(value)),
    );
  }
  expose(
    "NRGetInstalledApps",
    (callback: (data: string) => void) =>
      callback(JSON.stringify(management.apps)),
  );
  expose("NRRenameSsb", (id: string, name: string) => {
    if (management.fail === "rename") {
      throw new Error("Injected rename failure");
    }
    management.apps[id].name = name;
    calls.push({ method: "NRRenameSsb", args: [id, name] });
  });
  expose("NRSetSsbContainer", (id: string, container: number) => {
    management.apps[id].userContextId = container;
    calls.push({ method: "NRSetSsbContainer", args: [id, container] });
  });
  expose("NRUninstallSsb", (id: string) => {
    delete management.apps[id];
    calls.push({ method: "NRUninstallSsb", args: [id] });
  });
  expose("OSAutomotor", {
    getStatus: () =>
      Promise.resolve({
        enabled: management.osEnabled,
        platformSupported: true,
        installedVersion: "test",
        serverToken: "test-token",
      }),
    enable: () => {
      if (management.fail === "os") {
        return Promise.resolve({
          success: false,
          error: "Injected enable failure",
        });
      }
      management.osEnabled = true;
      return Promise.resolve({ success: true });
    },
    disable: () => {
      management.osEnabled = false;
      return Promise.resolve({ success: true });
    },
  });
}
