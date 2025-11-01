import { rpc } from "../../lib/rpc/rpc.ts";
import { type WorkspacesFormData } from "../../types/pref.ts";

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
    rpc.setBoolPref(
      "floorp.workspaces.enabled",
      Boolean(settings.enabled),
    ),
  ]);
}

export async function getWorkspaceSettings(): Promise<
  WorkspacesFormData | null
> {
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

async function getWorkspacesConfigsExcludeEnabled(): Promise<
  Omit<WorkspacesFormData, "enabled"> | null
> {
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

  return { ...defaultConfigs, ...JSON.parse(result) };
}
