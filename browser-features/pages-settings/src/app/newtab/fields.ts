// SPDX-License-Identifier: MPL-2.0
import type { NewTabSettingField, NewTabTab } from "./types.ts";
export const NEWTAB_SETTINGS_ROUTE = "/features/newtab";
export const NEWTAB_FIELDS: readonly NewTabSettingField[] = [
  { id: "newtab-search", tab: "general", titleKey: "newtab.search" },
  { id: "newtab-clock", tab: "general", titleKey: "newtab.clock" },
  { id: "newtab-history", tab: "general", titleKey: "newtab.history" },
  { id: "newtab-dials", tab: "general", titleKey: "newtab.dials" },
  { id: "newtab-feed", tab: "general", titleKey: "newtab.feed" },
  { id: "newtab-placement", tab: "general", titleKey: "newtab.placement" },
  { id: "newtab-layout", tab: "general", titleKey: "newtab.layout" },
  {
    id: "newtab-background-type",
    tab: "general",
    titleKey: "newtab.background.title",
  },
  {
    id: "newtab-transparency",
    tab: "general",
    titleKey: "newtab.transparency",
  },
  {
    id: "newtab-slideshow",
    tab: "general",
    titleKey: "newtab.background.slideshow",
  },
  {
    id: "newtab-interval",
    tab: "general",
    titleKey: "newtab.background.interval",
  },
  {
    id: "disable-floorp-start",
    tab: "general",
    titleKey: "design.uiCustomization.newtab.disableFloorpStart",
  },
  { id: "newtab-columns", tab: "dial", titleKey: "newtab.columns" },
  { id: "newtab-tile-size", tab: "dial", titleKey: "newtab.tileSize" },
  {
    id: "newtab-workspace-groups",
    tab: "dial",
    titleKey: "newtab.workspaceGroups",
  },
  { id: "newtab-refresh", tab: "feed", titleKey: "newtab.refresh" },
  { id: "newtab-sponsors", tab: "sponsors", titleKey: "newtab.sponsors.show" },
  {
    id: "newtab-restore-sponsors",
    tab: "sponsors",
    titleKey: "newtab.sponsors.restore",
  },
];
export function tabForSetting(setting: string | null): NewTabTab | null {
  return NEWTAB_FIELDS.find((field) => field.id === setting)?.tab ?? null;
}
export function resolveNewTabTab(search: string): NewTabTab {
  const params = new URLSearchParams(search);
  const destination = tabForSetting(params.get("setting"));
  if (destination) return destination;
  const tab = params.get("tab");
  return tab === "dial" || tab === "feed" || tab === "sponsors"
    ? tab
    : "general";
}
