import { actionCreators as ac, actionTypes as at } from "common/Actions.jsm";

/**
 * List of functions that return items that can be included as menu options in a
 * SectionMenu. All functions take the section as the only parameter.
 */
export const SectionMenuOptions = {
  Separator: () => ({ type: "separator" }),
  MoveUp: section => ({
    id: "newtab-section-menu-move-up",
    icon: "arrowhead-up",
    action: ac.OnlyToMain({
      type: at.SECTION_MOVE,
      data: { id: section.id, direction: -1 },
    }),
    userEvent: "MENU_MOVE_UP",
    disabled: !!section.isFirst,
  }),
  MoveDown: section => ({
    id: "newtab-section-menu-move-down",
    icon: "arrowhead-down",
    action: ac.OnlyToMain({
      type: at.SECTION_MOVE,
      data: { id: section.id, direction: +1 },
    }),
    userEvent: "MENU_MOVE_DOWN",
    disabled: !!section.isLast,
  }),
  RemoveSection: section => ({
    id: "newtab-section-menu-remove-section",
    icon: "dismiss",
    action: ac.SetPref(section.showPrefName, false),
    userEvent: "MENU_REMOVE",
  }),
  CollapseSection: section => ({
    id: "newtab-section-menu-collapse-section",
    icon: "minimize",
    action: ac.OnlyToMain({
      type: at.UPDATE_SECTION_PREFS,
      data: { id: section.id, value: { collapsed: true } },
    }),
    userEvent: "MENU_COLLAPSE",
  }),
  ExpandSection: section => ({
    id: "newtab-section-menu-expand-section",
    icon: "maximize",
    action: ac.OnlyToMain({
      type: at.UPDATE_SECTION_PREFS,
      data: { id: section.id, value: { collapsed: false } },
    }),
    userEvent: "MENU_EXPAND",
  }),
  ManageSection: section => ({
    id: "newtab-section-menu-manage-section",
    icon: "settings",
    action: ac.OnlyToMain({ type: at.SETTINGS_OPEN }),
    userEvent: "MENU_MANAGE",
  }),
  ManageWebExtension: section => ({
    id: "newtab-section-menu-manage-webext",
    icon: "settings",
    action: ac.OnlyToMain({ type: at.OPEN_WEBEXT_SETTINGS, data: section.id }),
  }),
  AddTopSite: section => ({
    id: "newtab-section-menu-add-topsite",
    icon: "add",
    action: { type: at.TOP_SITES_EDIT, data: { index: -1 } },
    userEvent: "MENU_ADD_TOPSITE",
  }),
  AddSearchShortcut: section => ({
    id: "newtab-section-menu-add-search-engine",
    icon: "search",
    action: { type: at.TOP_SITES_OPEN_SEARCH_SHORTCUTS_MODAL },
    userEvent: "MENU_ADD_SEARCH",
  }),
  PrivacyNotice: section => ({
    id: "newtab-section-menu-privacy-notice",
    icon: "info",
    action: ac.OnlyToMain({
      type: at.OPEN_LINK,
      data: { url: section.privacyNoticeURL },
    }),
    userEvent: "MENU_PRIVACY_NOTICE",
  }),
  CheckCollapsed: section =>
    section.collapsed
      ? SectionMenuOptions.ExpandSection(section)
      : SectionMenuOptions.CollapseSection(section),
};
