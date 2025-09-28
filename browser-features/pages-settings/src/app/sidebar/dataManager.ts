import { rpc } from "@/lib/rpc/rpc.ts";
import type { PanelSidebarFormData } from "@/types/pref.ts";
import type {
  Panel,
  Panels,
} from "../../../../main/core/common/panel-sidebar/utils/type.ts";

type Container = {
  id: number | string;
  name: string;
  label: string;
  icon: string;
  color: string;
};

type StaticPanel = {
  value: string;
  label: string;
  icon: string;
};

type ExtensionPanel = {
  extensionId: string;
  title: string;
  iconUrl: string;
};

export function getStaticPanelDisplayName(
  value: string | null,
  t: Function,
): string {
  if (!value) return t("panelSidebar.untitled");

  const staticPanelKey = value.replace("floorp//", "");
  return t(`panelSidebar.staticPanels.${staticPanelKey}`) || value;
}

export async function savePanelSidebarSettings(
  data: PanelSidebarFormData,
): Promise<void> {
  const { enabled, ...configData } = data;

  await Promise.all([
    rpc.setBoolPref("floorp.panelSidebar.enabled", enabled),
    rpc.setStringPref("floorp.panelSidebar.config", JSON.stringify(configData)),
  ]);
}

export async function getPanelSidebarSettings(): Promise<
  PanelSidebarFormData | null
> {
  const [enabled, configResult] = await Promise.all([
    rpc.getBoolPref("floorp.panelSidebar.enabled"),
    rpc.getStringPref("floorp.panelSidebar.config"),
  ]);

  if (!configResult) {
    return null;
  }

  const config = JSON.parse(configResult);
  return {
    enabled: enabled ?? true,
    autoUnload: config.autoUnload,
    position_start: config.position_start,
    globalWidth: config.globalWidth,
    displayed: config.displayed ?? false,
    webExtensionRunningEnabled: config.webExtensionRunningEnabled ?? false,
  };
}

export async function getPanelsList(): Promise<Panels | null> {
  const result = await rpc.getStringPref("floorp.panelSidebar.data");
  if (!result) {
    return null;
  }

  try {
    const data = JSON.parse(result);
    return data.data || [];
  } catch (e) {
    console.error("Failed to parse panel sidebar data:", e);
    return null;
  }
}

export async function savePanelsList(panels: Panels): Promise<void> {
  await rpc.setStringPref(
    "floorp.panelSidebar.data",
    JSON.stringify({ data: panels }),
  );
}

export async function addPanel(panel: Panel): Promise<void> {
  const panels = await getPanelsList();
  if (!panels) {
    return;
  }

  await savePanelsList([...panels, panel]);
}

export async function updatePanel(updatedPanel: Panel): Promise<void> {
  const panels = await getPanelsList();
  if (!panels) {
    return;
  }

  const updatedPanels = panels.map((panel) =>
    panel.id === updatedPanel.id ? updatedPanel : panel
  );

  await savePanelsList(updatedPanels);
}

export async function deletePanel(panelId: string): Promise<void> {
  const panels = await getPanelsList();
  if (!panels) {
    return;
  }

  const filteredPanels = panels.filter((panel) => panel.id !== panelId);
  await savePanelsList(filteredPanels);
}

const containersCache: Container[] = [];
const staticPanelsCache: StaticPanel[] = [];
const extensionPanelsCache: ExtensionPanel[] = [];

const fetchState = {
  isGettingContainers: false,
  isGettingStaticPanels: false,
  isGettingExtensionPanels: false,
};

export async function getContainers(): Promise<Container[]> {
  if (containersCache.length > 0) {
    return containersCache;
  }

  if (fetchState.isGettingContainers) {
    return new Promise((resolve) => {
      const checkInterval = setInterval(() => {
        if (!fetchState.isGettingContainers && containersCache.length > 0) {
          clearInterval(checkInterval);
          resolve(containersCache);
        }
      }, 100);
    });
  }

  fetchState.isGettingContainers = true;

  if (typeof (window as any).NRGetContainerContexts !== "function") {
    console.error("NRGetContainerContexts is not available");
    fetchState.isGettingContainers = false;
    return [];
  }

  try {
    return new Promise<Container[]>((resolve) => {
      const timeoutId = setTimeout(() => {
        console.error("Containers fetch timed out");
        fetchState.isGettingContainers = false;
        resolve([]);
      }, 5000);

      const callback = (dataStr: string) => {
        clearTimeout(timeoutId);
        try {
          const parsedData = JSON.parse(dataStr);
          const formattedData = Array.isArray(parsedData)
            ? parsedData.map((container: any) => {
              let containerId: number | string = 0;

              if (container.id === "0") {
                containerId = 0;
              } else {
                try {
                  containerId = Number.parseInt(container.id, 10);
                  if (isNaN(containerId)) {
                    console.error(
                      `Invalid container ID: ${container.id}, using original string`,
                    );
                    containerId = container.id;
                  }
                } catch (error) {
                  console.error(
                    `Error parsing container ID: ${container.id}`,
                    error,
                  );
                  containerId = container.id;
                }
              }

              return {
                id: containerId,
                name: container.label,
                label: container.label,
                icon: container.icon || "",
                color: container.color || "",
              };
            })
            : [];

          containersCache.length = 0;
          containersCache.push(...formattedData);
          fetchState.isGettingContainers = false;

          resolve(formattedData);
        } catch (error) {
          console.error("Failed to parse containers data:", error);
          fetchState.isGettingContainers = false;
          resolve([]);
        }
      };

      (window as any).NRGetContainerContexts(callback);
    });
  } catch (error) {
    console.error("Failed to get containers:", error);
    fetchState.isGettingContainers = false;
    return [];
  }
}

export async function getStaticPanels(): Promise<StaticPanel[]> {
  if (staticPanelsCache.length > 0) {
    return staticPanelsCache;
  }

  if (fetchState.isGettingStaticPanels) {
    return new Promise((resolve) => {
      const checkInterval = setInterval(() => {
        if (!fetchState.isGettingStaticPanels && staticPanelsCache.length > 0) {
          clearInterval(checkInterval);
          resolve(staticPanelsCache);
        }
      }, 100);
    });
  }

  fetchState.isGettingStaticPanels = true;

  if (typeof (window as any).NRGetStaticPanels !== "function") {
    console.error("NRGetStaticPanels is not available");
    fetchState.isGettingStaticPanels = false;
    return [];
  }

  try {
    return new Promise<StaticPanel[]>((resolve) => {
      const timeoutId = setTimeout(() => {
        console.error("Static panels fetch timed out");
        fetchState.isGettingStaticPanels = false;
        resolve([]);
      }, 5000);

      const callback = (dataStr: string) => {
        clearTimeout(timeoutId);
        try {
          const parsedData = JSON.parse(dataStr);
          const formattedData = Array.isArray(parsedData)
            ? parsedData.map((panel: any) => ({
              value: panel.id,
              label: panel.title,
              icon: panel.icon || "",
            }))
            : [];

          staticPanelsCache.length = 0;
          staticPanelsCache.push(...formattedData);
          fetchState.isGettingStaticPanels = false;

          resolve(formattedData);
        } catch (error) {
          console.error("Failed to parse static panels data:", error);
          fetchState.isGettingStaticPanels = false;
          resolve([]);
        }
      };

      (window as any).NRGetStaticPanels(callback);
    });
  } catch (error) {
    console.error("Failed to get static panels:", error);
    fetchState.isGettingStaticPanels = false;
    return [];
  }
}

export async function getExtensionPanels(): Promise<ExtensionPanel[]> {
  if (extensionPanelsCache.length > 0) {
    return extensionPanelsCache;
  }

  if (fetchState.isGettingExtensionPanels) {
    return new Promise((resolve) => {
      const checkInterval = setInterval(() => {
        if (
          !fetchState.isGettingExtensionPanels &&
          extensionPanelsCache.length > 0
        ) {
          clearInterval(checkInterval);
          resolve(extensionPanelsCache);
        }
      }, 100);
    });
  }

  fetchState.isGettingExtensionPanels = true;

  if (typeof (window as any).NRGetExtensionPanels !== "function") {
    console.error("NRGetExtensionPanels is not available");
    fetchState.isGettingExtensionPanels = false;
    return [];
  }

  try {
    return new Promise<ExtensionPanel[]>((resolve) => {
      const timeoutId = setTimeout(() => {
        console.error("Extension panels fetch timed out");
        fetchState.isGettingExtensionPanels = false;
        resolve([]);
      }, 5000);

      const callback = (dataStr: string) => {
        clearTimeout(timeoutId);
        try {
          const parsedData = JSON.parse(dataStr);
          const formattedData = Array.isArray(parsedData)
            ? parsedData.map((ext: any) => ({
              extensionId: ext.value,
              title: ext.label,
              iconUrl: ext.icon || "",
            }))
            : [];

          extensionPanelsCache.length = 0;
          extensionPanelsCache.push(...formattedData);
          fetchState.isGettingExtensionPanels = false;

          resolve(formattedData);
        } catch (error) {
          console.error("Failed to parse extension panels data:", error);
          fetchState.isGettingExtensionPanels = false;
          resolve([]);
        }
      };

      (window as any).NRGetExtensionPanels(callback);
    });
  } catch (error) {
    console.error("Failed to get extension panels:", error);
    fetchState.isGettingExtensionPanels = false;
    return [];
  }
}
