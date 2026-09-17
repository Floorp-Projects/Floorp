import { rpc } from "@/lib/rpc/rpc.ts";

export const TAB_STACKS_ENABLED_PREF = "floorp.tabstacks.enabled";

export interface TabStacksSettings {
  enabled: boolean;
}

export async function getTabStacksSettings(): Promise<TabStacksSettings> {
  const enabled = await rpc.getBoolPref(TAB_STACKS_ENABLED_PREF);
  return {
    enabled: enabled ?? false,
  };
}

export async function setTabStacksEnabled(enabled: boolean): Promise<void> {
  await rpc.setBoolPref(TAB_STACKS_ENABLED_PREF, enabled);
}
