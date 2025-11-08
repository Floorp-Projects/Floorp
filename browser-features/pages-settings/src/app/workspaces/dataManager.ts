import { rpc } from "../../lib/rpc/rpc.ts";
import { type WorkspacesFormData } from "../../types/pref.ts";

const WORKSPACES_INIT_TOPIC = "floorp.workspaces.initialize";

type InitializeResult = {
  success: boolean;
  error?: string | null;
};

export async function initializeWorkspaces(reason?: string): Promise<boolean> {
  const globalWindow = window as unknown as {
    NRInitializeWorkspaces?: (
      reason?: string | null,
    ) => Promise<InitializeResult> | InitializeResult;
    Services?: {
      obs?: {
        notifyObservers: (
          subject: unknown,
          topic: string,
          data?: string | null,
        ) => void;
      };
    };
  };

  if (typeof globalWindow.NRInitializeWorkspaces === "function") {
    try {
      const result = await globalWindow.NRInitializeWorkspaces(reason ?? null);
      if (result?.success) {
        return true;
      }

      console.error(
        "initializeWorkspaces: actor reported failure",
        result?.error,
      );
      return false;
    } catch (error) {
      console.error(
        "initializeWorkspaces: actor call threw an exception",
        error,
      );
      return false;
    }
  }

  try {
    globalWindow.Services?.obs?.notifyObservers(
      null,
      WORKSPACES_INIT_TOPIC,
      reason ?? null,
    );
    return true;
  } catch (error) {
    console.error("initializeWorkspaces: fallback notification failed", error);
  }

  console.error("initializeWorkspaces: no available transport to initialize");
  return false;
}

export async function saveWorkspaceSettings(
  settings: WorkspacesFormData,
): Promise<null | void> {
  if (Object.keys(settings).length === 0) {
    return;
  }

  const oldConfig = await getWorkspacesConfigsExcludeEnabled();

  const newConfigs = {
    ...oldConfig,
    manageOnBms: settings.manageOnBms,
    showWorkspaceNameOnToolbar: settings.showWorkspaceNameOnToolbar,
    closePopupAfterClick: settings.closePopupAfterClick,
    exitOnLastTabClose: settings.exitOnLastTabClose,
  };

  await Promise.all([
    rpc.setStringPref(
      "floorp.workspaces.v4.config",
      JSON.stringify(newConfigs),
    ),
    rpc.setBoolPref("floorp.workspaces.enabled", Boolean(settings.enabled)),
  ]);
}

export async function getWorkspaceSettings(): Promise<WorkspacesFormData | null> {
  const [enabled, configs] = await Promise.all([
    getWorkspacesEnabled(),
    getWorkspacesConfigsExcludeEnabled(),
  ]);

  if (enabled === null || configs === null) {
    return null;
  }

  return {
    enabled,
    ...configs,
  };
}

async function getWorkspacesEnabled(): Promise<boolean | null> {
  return await rpc.getBoolPref("floorp.workspaces.enabled");
}

async function getWorkspacesConfigsExcludeEnabled(): Promise<Omit<
  WorkspacesFormData,
  "enabled"
> | null> {
  const defaultConfigs = {
    manageOnBms: false,
    showWorkspaceNameOnToolbar: false,
    closePopupAfterClick: true,
    exitOnLastTabClose: false,
  };

  const result = await rpc.getStringPref("floorp.workspaces.v4.config");
  if (!result) {
    return defaultConfigs;
  }

  try {
    const userConfigs = JSON.parse(result);
    return { ...defaultConfigs, ...userConfigs };
  } catch (e) {
    console.error(
      "Failed to parse workspace configuration, falling back to defaults:",
      e,
    );
    return defaultConfigs;
  }
}
