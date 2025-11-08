import { rpc } from "@/lib/rpc/rpc.ts";
import type { DesignFormData } from "@/types/pref.ts";

// Lepton Settings Interface
export interface LeptonFormData {
  // Auto-hide settings
  autohideTab: boolean;
  autohideNavbar: boolean;
  autohideSidebar: boolean;
  autohideBackButton: boolean;
  autohideForwardButton: boolean;
  autohidePageAction: boolean;

  // Hide settings
  hiddenTabIcon: boolean;
  hiddenTabbar: boolean;
  hiddenNavbar: boolean;
  hiddenSidebarHeader: boolean;
  hiddenUrlbarIconbox: boolean;
  hiddenBookmarkbarIcon: boolean;
  hiddenBookmarkbarLabel: boolean;
  hiddenDisabledMenu: boolean;

  // Icon settings
  iconDisabled: boolean;
  iconMenu: boolean;

  // Centered settings
  centeredTab: boolean;
  centeredUrlbar: boolean;
  centeredBookmarkbar: boolean;

  // URL View settings
  urlViewMoveIconToLeft: boolean;
  urlViewGoButtonWhenTyping: boolean;
  urlViewAlwaysShowPageActions: boolean;

  // Tab Bar settings
  tabbarAsTitlebar: boolean;
  tabbarOneLiner: boolean;

  // Sidebar settings
  sidebarOverlap: boolean;
}

// Lepton preference keys mapping
const LEPTON_PREFS = {
  autohideTab: "userChrome.autohide.tab",
  autohideNavbar: "userChrome.autohide.navbar",
  autohideSidebar: "userChrome.autohide.sidebar",
  autohideBackButton: "userChrome.autohide.back_button",
  autohideForwardButton: "userChrome.autohide.forward_button",
  autohidePageAction: "userChrome.autohide.page_action",
  hiddenTabIcon: "userChrome.hidden.tab_icon",
  hiddenTabbar: "userChrome.hidden.tabbar",
  hiddenNavbar: "userChrome.hidden.navbar",
  hiddenSidebarHeader: "userChrome.hidden.sidebar_header",
  hiddenUrlbarIconbox: "userChrome.hidden.urlbar_iconbox",
  hiddenBookmarkbarIcon: "userChrome.hidden.bookmarkbar_icon",
  hiddenBookmarkbarLabel: "userChrome.hidden.bookmarkbar_label",
  hiddenDisabledMenu: "userChrome.hidden.disabled_menu",
  iconDisabled: "userChrome.icon.disabled",
  iconMenu: "userChrome.icon.menu",
  centeredTab: "userChrome.centered.tab",
  centeredUrlbar: "userChrome.centered.urlbar",
  centeredBookmarkbar: "userChrome.centered.bookmarkbar",
  urlViewMoveIconToLeft: "userChrome.urlView.move_icon_to_left",
  urlViewGoButtonWhenTyping: "userChrome.urlView.go_button_when_typing",
  urlViewAlwaysShowPageActions: "userChrome.urlView.always_show_page_actions",
  tabbarAsTitlebar: "userChrome.tabbar.as_titlebar",
  tabbarOneLiner: "userChrome.tabbar.one_liner",
  sidebarOverlap: "userChrome.sidebar.overlap",
} as const;

export async function saveLeptonSettings(
  settings: LeptonFormData,
): Promise<void> {
  const promises = Object.entries(settings).map(([key, value]) => {
    const prefKey = LEPTON_PREFS[key as keyof LeptonFormData];
    if (prefKey) {
      return rpc.setBoolPref(prefKey, value);
    }
    return Promise.resolve();
  });

  await Promise.all(promises);
}

export async function getLeptonSettings(): Promise<LeptonFormData> {
  const defaultSettings: LeptonFormData = {
    autohideTab: false,
    autohideNavbar: false,
    autohideSidebar: false,
    autohideBackButton: false,
    autohideForwardButton: false,
    autohidePageAction: false,
    hiddenTabIcon: false,
    hiddenTabbar: false,
    hiddenNavbar: false,
    hiddenSidebarHeader: false,
    hiddenUrlbarIconbox: false,
    hiddenBookmarkbarIcon: false,
    hiddenBookmarkbarLabel: false,
    hiddenDisabledMenu: false,
    iconDisabled: false,
    iconMenu: false,
    centeredTab: false,
    centeredUrlbar: false,
    centeredBookmarkbar: false,
    urlViewMoveIconToLeft: false,
    urlViewGoButtonWhenTyping: false,
    urlViewAlwaysShowPageActions: false,
    tabbarAsTitlebar: false,
    tabbarOneLiner: false,
    sidebarOverlap: false,
  };

  const results = await Promise.all(
    Object.entries(LEPTON_PREFS).map(async ([key, prefKey]) => {
      const value = await rpc.getBoolPref(prefKey);
      return [key, value ?? defaultSettings[key as keyof LeptonFormData]] as [
        string,
        boolean,
      ];
    }),
  );

  const settings: LeptonFormData = { ...defaultSettings };
  results.forEach(([key, value]) => {
    settings[key as keyof LeptonFormData] = value;
  });

  return settings;
}

export async function saveDesignSettings(
  settings: DesignFormData,
): Promise<null | void> {
  if (Object.keys(settings).length === 0) {
    return;
  }

  const result = await rpc.getStringPref("floorp.design.configs");
  if (!result) {
    return null;
  }
  const oldData = JSON.parse(result);

  const newData = {
    ...oldData,
    globalConfigs: {
      ...oldData.globalConfigs,
      userInterface: settings.design,
      faviconColor: settings.faviconColor,
    },
    tabbar: {
      ...oldData.tabbar,
      tabbarPosition: settings.position,
      tabbarStyle: settings.style,
      multiRowTabBar: {
        ...oldData.tabbar.multiRowTabBar,
        maxRowEnabled: settings.maxRowEnabled,
        maxRow: settings.maxRow,
      },
    },
    tab: {
      ...oldData.tab,
      tabScroll: {
        ...oldData.tab.tabScroll,
        reverse: settings.tabScrollReverse,
        wrap: settings.tabScrollWrap,
        enabled: settings.tabScroll,
      },
      tabOpenPosition: settings.tabOpenPosition,
      tabMinHeight: settings.tabMinHeight,
      tabMinWidth: settings.tabMinWidth,
      tabPinTitle: settings.tabPinTitle,
      tabDubleClickToClose: settings.tabDubleClickToClose,
    },
    uiCustomization: {
      ...oldData.uiCustomization,
      navbar: {
        position: settings.navbarPosition,
        searchBarTop: settings.searchBarTop,
      },
      display: {
        disableFullscreenNotification: settings.disableFullscreenNotification,
        deleteBrowserBorder: settings.deleteBrowserBorder,
      },
      special: {
        optimizeForTreeStyleTab: settings.optimizeForTreeStyleTab,
        hideForwardBackwardButton: settings.hideForwardBackwardButton,
        stgLikeWorkspaces: settings.stgLikeWorkspaces,
      },
      multirowTab: {
        newtabInsideEnabled: settings.multirowTabNewtabInside,
      },
      bookmarkBar: {
        focusExpand: settings.bookmarkBarFocusExpand,
      },
      qrCode: {
        disableButton: settings.disableQRCodeButton,
      },
      disableFloorpStart: settings.disableFloorpStart,
    },
  };
  rpc.setStringPref("floorp.design.configs", JSON.stringify(newData));

  // Set sidebar.verticalTabs preference based on tab bar style
  if (settings.style !== undefined) {
    const isVertical = settings.style === "vertical";
    await rpc.setBoolPref("sidebar.verticalTabs", isVertical);
    
    // Set sidebar.revamp to false when vertical tabs are disabled
    if (!isVertical) {
      await rpc.setBoolPref("sidebar.revamp", false);
    }
  }
}

export async function getDesignSettings(): Promise<DesignFormData | null> {
  const result = await rpc.getStringPref("floorp.design.configs");
  if (!result) {
    return null;
  }
  const data = JSON.parse(result);
  const formData: DesignFormData = {
    design: data.globalConfigs.userInterface,
    position: data.tabbar.tabbarPosition,
    style: data.tabbar.tabbarStyle,
    tabOpenPosition: data.tab.tabOpenPosition,
    tabMinHeight: data.tab.tabMinHeight,
    tabMinWidth: data.tab.tabMinWidth,
    tabPinTitle: data.tab.tabPinTitle,
    tabScrollReverse: data.tab.tabScroll.reverse,
    tabScrollWrap: data.tab.tabScroll.wrap,
    tabDubleClickToClose: data.tab.tabDubleClickToClose,
    tabScroll: data.tab.tabScroll.enabled,
    faviconColor: data.globalConfigs.faviconColor,
    maxRowEnabled: data.tabbar.multiRowTabBar.maxRowEnabled,
    maxRow: data.tabbar.multiRowTabBar.maxRow,
    navbarPosition: data.uiCustomization.navbar.position,
    searchBarTop: data.uiCustomization.navbar.searchBarTop,
    disableFullscreenNotification:
      data.uiCustomization.display.disableFullscreenNotification,
    deleteBrowserBorder: data.uiCustomization.display.deleteBrowserBorder,
    optimizeForTreeStyleTab:
      data.uiCustomization.special.optimizeForTreeStyleTab,
    hideForwardBackwardButton:
      data.uiCustomization.special.hideForwardBackwardButton,
    stgLikeWorkspaces: data.uiCustomization.special.stgLikeWorkspaces,
    multirowTabNewtabInside:
      data.uiCustomization.multirowTab.newtabInsideEnabled,
    bookmarkBarFocusExpand: data.uiCustomization.bookmarkBar?.focusExpand ??
      false,
    disableQRCodeButton: data.uiCustomization.qrCode?.disableButton ?? false,
    disableFloorpStart: data.uiCustomization.disableFloorpStart,
  };
  return formData;
}
