import { rpc } from "@/lib/rpc/rpc.ts";
import type { DesignFormData } from "@/types/pref.ts";
import {
  CHROME_EXTRAS_DEFAULTS,
  type ChromeExtrasKey,
  type ChromeExtrasSettings,
} from "#features-chrome/common/designs/chrome-extras.ts";


const SPLIT_VIEW_DND_CREATE_PREF = "floorp.splitView.dragToSplitCreate.enabled";
const DEFAULT_SPLIT_VIEW_DND_CREATE = false;

/** The single pref every design setting lives in. */
const DESIGN_CONFIGS_PREF = "floorp.design.configs";

/**
 * The 25 chrome-extras toggles ("UI 拡張設定").
 *
 * These used to be stored as individual `userChrome.*` bool prefs, which is why
 * they only worked on the Lepton-family designs. They now live in
 * `floorp.design.configs` → `uiCustomization.chromeExtras`, so `style-manager.ts`
 * can apply them on every design. `getOldChromeExtrasConfig()` seeds this from
 * the old prefs on the first run after the upgrade.
 */
const CHROME_EXTRAS_KEYS = Object.keys(
  CHROME_EXTRAS_DEFAULTS,
) as ChromeExtrasKey[];

type StoredConfig = {
  uiCustomization?: {
    chromeExtras?: Partial<Record<ChromeExtrasKey, unknown>>;
  };
};

async function readDesignConfigs(): Promise<StoredConfig> {
  const raw = await rpc.getStringPref(DESIGN_CONFIGS_PREF);
  if (!raw) {
    return {};
  }
  try {
    return JSON.parse(raw) as StoredConfig;
  } catch (error) {
    console.error("[ChromeExtras] Failed to parse design configs:", error);
    return {};
  }
}

/**
 * Read the chrome-extras toggles, filling in the defaults for anything the
 * stored config does not have.
 */
export async function getChromeExtras(): Promise<ChromeExtrasSettings> {
  const config = await readDesignConfigs();
  const stored = config.uiCustomization?.chromeExtras;
  const result: ChromeExtrasSettings = { ...CHROME_EXTRAS_DEFAULTS };
  if (stored) {
    for (const key of CHROME_EXTRAS_KEYS) {
      const value = stored[key];
      if (typeof value === "boolean") {
        result[key] = value;
      }
    }
  }
  return result;
}

/**
 * Merge the chrome-extras toggles back into `floorp.design.configs`.
 *
 * A read-modify-write of just this category: the rest of the design config is
 * owned by `saveDesignSettings()` below, which spreads `oldData.uiCustomization`
 * and therefore preserves `chromeExtras` untouched.
 */
export async function saveChromeExtras(
  settings: ChromeExtrasSettings,
): Promise<void> {
  const config = await readDesignConfigs();
  const newData = {
    ...config,
    uiCustomization: {
      ...config.uiCustomization,
      chromeExtras: { ...CHROME_EXTRAS_DEFAULTS, ...settings },
    },
  };
  await rpc.setStringPref(DESIGN_CONFIGS_PREF, JSON.stringify(newData));
}

interface SaveDesignSettingsOptions {
  hasTabStyleChanged?: boolean;
}

export async function saveDesignSettings(
  settings: DesignFormData,
  options: SaveDesignSettingsOptions = {},
): Promise<null | void> {
  if (Object.keys(settings).length === 0) {
    return;
  }

  const result = await rpc.getStringPref(DESIGN_CONFIGS_PREF);
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
      tabDoubleClickToClose: settings.tabDoubleClickToClose,
    },
    uiCustomization: {
      ...oldData.uiCustomization,
      navbar: {
        ...oldData.uiCustomization.navbar,
        position: settings.navbarPosition,
        searchBarTop: settings.searchBarTop,
      },
      display: {
        ...oldData.uiCustomization.display,
        disableFullscreenNotification: settings.disableFullscreenNotification,
        deleteBrowserBorder: settings.deleteBrowserBorder,
      },
      special: {
        ...oldData.uiCustomization.special,
        optimizeForTreeStyleTab: settings.optimizeForTreeStyleTab,
        hideForwardBackwardButton: settings.hideForwardBackwardButton,
        stgLikeWorkspaces: settings.stgLikeWorkspaces,
      },
      multirowTab: {
        ...oldData.uiCustomization.multirowTab,
        newtabInsideEnabled: settings.multirowTabNewtabInside,
      },
      bookmarkBar: {
        ...oldData.uiCustomization.bookmarkBar,
        focusExpand: settings.bookmarkBarFocusExpand,
        position: settings.bookmarkBarPosition,
      },
      qrCode: {
        ...oldData.uiCustomization.qrCode,
        disableButton: settings.disableQRCodeButton,
      },
      disableFloorpStart: settings.disableFloorpStart,
    },
  };
  delete newData.tab.tabDubleClickToClose;
  await rpc.setStringPref(DESIGN_CONFIGS_PREF, JSON.stringify(newData));
  await rpc.setBoolPref(
    SPLIT_VIEW_DND_CREATE_PREF,
    settings.tabDragToSplitCreate,
  );

  const { hasTabStyleChanged = false } = options;

  if (hasTabStyleChanged && settings.style !== undefined) {
    const isVertical = settings.style === "vertical";
    await rpc.setBoolPref("sidebar.verticalTabs", isVertical);

    if (!isVertical) {
      await rpc.setBoolPref("sidebar.revamp", false);
    }
  }
}

export async function getDesignSettings(): Promise<DesignFormData | null> {
  const result = await rpc.getStringPref(DESIGN_CONFIGS_PREF);
  if (!result) {
    return null;
  }
  const data = JSON.parse(result);
  const splitViewDndCreateEnabled = await rpc.getBoolPref(
    SPLIT_VIEW_DND_CREATE_PREF,
  );
  const formData: DesignFormData = {
    design: data.globalConfigs.userInterface,
    position: data.tabbar.tabbarPosition,
    style: data.tabbar.tabbarStyle,
    tabOpenPosition: data.tab.tabOpenPosition,
    tabMinHeight: data.tab.tabMinHeight,
    tabMinWidth: data.tab.tabMinWidth,
    tabPinTitle: data.tab.tabPinTitle,
    tabDragToSplitCreate: splitViewDndCreateEnabled ??
      DEFAULT_SPLIT_VIEW_DND_CREATE,
    tabScrollReverse: data.tab.tabScroll.reverse,
    tabScrollWrap: data.tab.tabScroll.wrap,
    tabDoubleClickToClose: typeof data.tab.tabDoubleClickToClose === "boolean"
      ? data.tab.tabDoubleClickToClose
      : data.tab.tabDubleClickToClose === true,
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
    bookmarkBarPosition: data.uiCustomization.bookmarkBar?.position ?? "top",
    disableQRCodeButton: data.uiCustomization.qrCode?.disableButton ?? false,
    disableFloorpStart: data.uiCustomization.disableFloorpStart,
  };
  return formData;
}

// Tab Sleep Exclusion Settings
const TAB_SLEEP_EXCLUSION_PREF = "floorp.tabs.sleep.exclusion";

export interface TabSleepExclusionSettings {
  enabled: boolean;
  patterns: string[];
}

const DEFAULT_TAB_SLEEP_EXCLUSION_SETTINGS: TabSleepExclusionSettings = {
  enabled: true,
  patterns: [],
};

export async function getTabSleepExclusionSettings(): Promise<
  TabSleepExclusionSettings
> {
  try {
    const result = await rpc.getStringPref(TAB_SLEEP_EXCLUSION_PREF);
    if (!result) {
      return { ...DEFAULT_TAB_SLEEP_EXCLUSION_SETTINGS };
    }
    const data = JSON.parse(result);
    return {
      enabled: data.enabled ?? DEFAULT_TAB_SLEEP_EXCLUSION_SETTINGS.enabled,
      patterns: Array.isArray(data.patterns)
        ? data.patterns
        : DEFAULT_TAB_SLEEP_EXCLUSION_SETTINGS.patterns,
    };
  } catch {
    return { ...DEFAULT_TAB_SLEEP_EXCLUSION_SETTINGS };
  }
}

export async function saveTabSleepExclusionSettings(
  settings: TabSleepExclusionSettings,
): Promise<void> {
  await rpc.setStringPref(TAB_SLEEP_EXCLUSION_PREF, JSON.stringify(settings));
}
