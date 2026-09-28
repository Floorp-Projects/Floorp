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
      return Promise.resolve("failed");
    }
    management.apps[id].name = name;
    calls.push({ method: "NRRenameSsb", args: [id, name] });
    return Promise.resolve("ok");
  });
  expose("NRSetSsbContainer", (id: string, container: number) => {
    if (management.fail === "native-context") {
      return Promise.resolve("native-context-fixed");
    }
    management.apps[id].userContextId = container;
    calls.push({ method: "NRSetSsbContainer", args: [id, container] });
    return Promise.resolve("ok");
  });
  expose("NRUninstallSsb", (id: string) => {
    if (management.fail === "uninstall") return Promise.resolve("failed");
    delete management.apps[id];
    calls.push({ method: "NRUninstallSsb", args: [id] });
    return Promise.resolve("ok");
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
