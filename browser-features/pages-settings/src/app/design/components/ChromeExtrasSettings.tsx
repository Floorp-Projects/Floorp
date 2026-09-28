import { Button } from "../../../../../../libs/ui/button.tsx";
import { useEffect, useRef, useState } from "react";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { useTranslation } from "react-i18next";
import { useNavigate } from "react-router-dom";
import {
  Eye,
  EyeOff,
  LayoutGrid,
  Navigation,
  Settings,
  Sidebar,
} from "lucide-react";
import { getChromeExtras, saveChromeExtras } from "../dataManager.ts";
import {
  CHROME_EXTRAS_DEFAULTS,
  type ChromeExtrasKey,
  type ChromeExtrasSettings,
} from "#features-chrome/common/designs/chrome-extras.ts";

interface ChromeExtrasSettingsProps {
  onClose?: () => void;
}

export function ChromeExtrasSettings({ onClose }: ChromeExtrasSettingsProps) {
  const { t } = useTranslation();
  const navigate = useNavigate();
  const [settings, setSettings] = useState<ChromeExtrasSettings>(
    CHROME_EXTRAS_DEFAULTS,
  );
  const latestSettingsRef = useRef<ChromeExtrasSettings>(
    CHROME_EXTRAS_DEFAULTS,
  );
  const pendingSavesRef = useRef(0);
  const mountedRef = useRef(false);
  const [loaded, setLoaded] = useState(false);
  const [loadError, setLoadError] = useState(false);
  const [saveError, setSaveError] = useState(false);
  const [saving, setSaving] = useState(false);
  const [loadAttempt, setLoadAttempt] = useState(0);

  useEffect(() => {
    let active = true;
    mountedRef.current = true;
    setLoaded(false);
    setLoadError(false);
    const loadSettings = async () => {
      try {
        const chromeExtras = await getChromeExtras();
        if (!active) return;
        latestSettingsRef.current = chromeExtras;
        setSettings(chromeExtras);
        setLoaded(true);
      } catch (error) {
        if (!active) return;
        console.error("[ChromeExtras] Failed to load settings:", error);
        setLoadError(true);
      }
    };
    void loadSettings();
    return () => {
      active = false;
      mountedRef.current = false;
    };
  }, [loadAttempt]);

  const persistSettings = (snapshot: ChromeExtrasSettings) => {
    pendingSavesRef.current++;
    setSaving(true);
    setSaveError(false);
    void saveChromeExtras(snapshot).then(() => {
      if (mountedRef.current) setSaveError(false);
    }).catch((error) => {
      console.error("[ChromeExtras] Failed to save settings:", error);
      if (mountedRef.current) setSaveError(true);
    }).finally(() => {
      pendingSavesRef.current--;
      if (mountedRef.current) setSaving(pendingSavesRef.current > 0);
    });
  };

  const handleSettingChange = (
    key: ChromeExtrasKey,
    value: boolean,
  ) => {
    const newSettings = { ...latestSettingsRef.current, [key]: value };
    latestSettingsRef.current = newSettings;
    setSettings(newSettings);
    persistSettings(newSettings);
  };

  return (
    <div className="p-6 space-y-6">
      <div className="flex items-center justify-between">
        <div className="flex flex-col items-start">
          <h1 className="text-3xl font-bold mb-2">
            {t("design.chrome-extras.title")}
          </h1>
          <p className="text-sm text-muted-foreground">
            {t("design.chrome-extras.description")}
          </p>
        </div>
        <Button
          onClick={() => (onClose ? onClose() : navigate("/features/design"))}
          type="button"
          variant="primary"
        >
          {t("design.chrome-extras.back")}
        </Button>
      </div>

      {loadError && (
        <div className="space-y-2">
          <p role="alert">{t("ui.loadError")}</p>
          <Button
            type="button"
            variant="secondary"
            onClick={() => setLoadAttempt((attempt) => attempt + 1)}
          >
            {t("ui.retry")}
          </Button>
        </div>
      )}
      {!loaded && !loadError && <p role="status">{t("ui.loading")}</p>}
      {saveError && (
        <div className="space-y-2">
          <p role="alert">{t("ui.saveError")}</p>
          <Button
            type="button"
            variant="secondary"
            disabled={saving}
            onClick={() => persistSettings(latestSettingsRef.current)}
          >
            {t("ui.retry")}
          </Button>
        </div>
      )}

      {/* Experimental Warning */}
      <div className="bg-yellow-50 dark:bg-yellow-900/20 border border-yellow-200 dark:border-yellow-800 rounded-lg p-4">
        <div className="flex flex-col space-y-3">
          <h2 className="text-base font-semibold text-yellow-800 dark:text-yellow-200">
            {t("design.chrome-extras.experimentalWarning.title")}
          </h2>
          <p className="text-sm text-yellow-700 dark:text-yellow-300">
            {t("design.chrome-extras.experimentalWarning.description")}
          </p>
        </div>
      </div>

      <fieldset
        disabled={!loaded}
        aria-busy={!loaded || saving}
        className="space-y-6"
      >
        {/* Auto-hide Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <EyeOff className="size-5" />
              {t("design.chrome-extras.autohide.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="autohide-tab">
                {t("design.chrome-extras.autohide.tab")}
              </label>
              <Switch
                id="autohide-tab"
                checked={settings.autohideTab}
                onChange={(e) =>
                  handleSettingChange("autohideTab", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="autohide-navbar">
                {t("design.chrome-extras.autohide.navbar")}
              </label>
              <Switch
                id="autohide-navbar"
                checked={settings.autohideNavbar}
                onChange={(e) =>
                  handleSettingChange("autohideNavbar", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="autohide-sidebar">
                {t("design.chrome-extras.autohide.sidebar")}
              </label>
              <Switch
                id="autohide-sidebar"
                checked={settings.autohideSidebar}
                onChange={(e) =>
                  handleSettingChange("autohideSidebar", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="autohide-back-button">
                {t("design.chrome-extras.autohide.backButton")}
              </label>
              <Switch
                id="autohide-back-button"
                checked={settings.autohideBackButton}
                onChange={(e) =>
                  handleSettingChange("autohideBackButton", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="autohide-forward-button">
                {t("design.chrome-extras.autohide.forwardButton")}
              </label>
              <Switch
                id="autohide-forward-button"
                checked={settings.autohideForwardButton}
                onChange={(e) =>
                  handleSettingChange(
                    "autohideForwardButton",
                    e.target.checked,
                  )}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="autohide-page-action">
                {t("design.chrome-extras.autohide.pageAction")}
              </label>
              <Switch
                id="autohide-page-action"
                checked={settings.autohidePageAction}
                onChange={(e) =>
                  handleSettingChange("autohidePageAction", e.target.checked)}
              />
            </div>
          </CardContent>
        </Card>

        {/* Hide Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Eye className="size-5" />
              {t("design.chrome-extras.hidden.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-tab-icon">
                {t("design.chrome-extras.hidden.tabIcon")}
              </label>
              <Switch
                id="hidden-tab-icon"
                checked={settings.hiddenTabIcon}
                onChange={(e) =>
                  handleSettingChange("hiddenTabIcon", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-tabbar">
                {t("design.chrome-extras.hidden.tabbar")}
              </label>
              <Switch
                id="hidden-tabbar"
                checked={settings.hiddenTabbar}
                onChange={(e) =>
                  handleSettingChange("hiddenTabbar", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-navbar">
                {t("design.chrome-extras.hidden.navbar")}
              </label>
              <Switch
                id="hidden-navbar"
                checked={settings.hiddenNavbar}
                onChange={(e) =>
                  handleSettingChange("hiddenNavbar", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-sidebar-header">
                {t("design.chrome-extras.hidden.sidebarHeader")}
              </label>
              <Switch
                id="hidden-sidebar-header"
                checked={settings.hiddenSidebarHeader}
                onChange={(e) =>
                  handleSettingChange("hiddenSidebarHeader", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-urlbar-iconbox">
                {t("design.chrome-extras.hidden.urlbarIconbox")}
              </label>
              <Switch
                id="hidden-urlbar-iconbox"
                checked={settings.hiddenUrlbarIconbox}
                onChange={(e) =>
                  handleSettingChange("hiddenUrlbarIconbox", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-bookmarkbar-icon">
                {t("design.chrome-extras.hidden.bookmarkbarIcon")}
              </label>
              <Switch
                id="hidden-bookmarkbar-icon"
                checked={settings.hiddenBookmarkbarIcon}
                onChange={(e) =>
                  handleSettingChange(
                    "hiddenBookmarkbarIcon",
                    e.target.checked,
                  )}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-bookmarkbar-label">
                {t("design.chrome-extras.hidden.bookmarkbarLabel")}
              </label>
              <Switch
                id="hidden-bookmarkbar-label"
                checked={settings.hiddenBookmarkbarLabel}
                onChange={(e) =>
                  handleSettingChange(
                    "hiddenBookmarkbarLabel",
                    e.target.checked,
                  )}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="hidden-disabled-menu">
                {t("design.chrome-extras.hidden.disabledMenu")}
              </label>
              <Switch
                id="hidden-disabled-menu"
                checked={settings.hiddenDisabledMenu}
                onChange={(e) =>
                  handleSettingChange("hiddenDisabledMenu", e.target.checked)}
              />
            </div>
          </CardContent>
        </Card>

        {/* Icon Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Settings className="size-5" />
              {t("design.chrome-extras.icon.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="icon-disabled">
                {t("design.chrome-extras.icon.disabled")}
              </label>
              <Switch
                id="icon-disabled"
                checked={settings.iconDisabled}
                onChange={(e) =>
                  handleSettingChange("iconDisabled", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="icon-menu">
                {t("design.chrome-extras.icon.menu")}
              </label>
              <Switch
                id="icon-menu"
                checked={settings.iconMenu}
                onChange={(e) =>
                  handleSettingChange("iconMenu", e.target.checked)}
              />
            </div>
          </CardContent>
        </Card>

        {/* Centered Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Navigation className="size-5" />
              {t("design.chrome-extras.centered.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="centered-tab">
                {t("design.chrome-extras.centered.tab")}
              </label>
              <Switch
                id="centered-tab"
                checked={settings.centeredTab}
                onChange={(e) =>
                  handleSettingChange("centeredTab", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="centered-urlbar">
                {t("design.chrome-extras.centered.urlbar")}
              </label>
              <Switch
                id="centered-urlbar"
                checked={settings.centeredUrlbar}
                onChange={(e) =>
                  handleSettingChange("centeredUrlbar", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="centered-bookmarkbar">
                {t("design.chrome-extras.centered.bookmarkbar")}
              </label>
              <Switch
                id="centered-bookmarkbar"
                checked={settings.centeredBookmarkbar}
                onChange={(e) =>
                  handleSettingChange("centeredBookmarkbar", e.target.checked)}
              />
            </div>
          </CardContent>
        </Card>

        {/* URL View Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Navigation className="size-5" />
              {t("design.chrome-extras.urlView.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="url-view-move-icon-to-left">
                {t("design.chrome-extras.urlView.moveIconToLeft")}
              </label>
              <Switch
                id="url-view-move-icon-to-left"
                checked={settings.urlViewMoveIconToLeft}
                onChange={(e) =>
                  handleSettingChange(
                    "urlViewMoveIconToLeft",
                    e.target.checked,
                  )}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="url-view-go-button-when-typing">
                {t("design.chrome-extras.urlView.goButtonWhenTyping")}
              </label>
              <Switch
                id="url-view-go-button-when-typing"
                checked={settings.urlViewGoButtonWhenTyping}
                onChange={(e) =>
                  handleSettingChange(
                    "urlViewGoButtonWhenTyping",
                    e.target.checked,
                  )}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="url-view-always-show-page-actions">
                {t("design.chrome-extras.urlView.alwaysShowPageActions")}
              </label>
              <Switch
                id="url-view-always-show-page-actions"
                checked={settings.urlViewAlwaysShowPageActions}
                onChange={(e) =>
                  handleSettingChange(
                    "urlViewAlwaysShowPageActions",
                    e.target.checked,
                  )}
              />
            </div>
          </CardContent>
        </Card>

        {/* Tab Bar Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <LayoutGrid className="size-5" />
              {t("design.chrome-extras.tabbar.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="tabbar-as-titlebar">
                {t("design.chrome-extras.tabbar.asTitlebar")}
              </label>
              <Switch
                id="tabbar-as-titlebar"
                checked={settings.tabbarAsTitlebar}
                onChange={(e) =>
                  handleSettingChange("tabbarAsTitlebar", e.target.checked)}
              />
            </div>
            <div className="flex items-center justify-between">
              <label htmlFor="tabbar-one-liner">
                {t("design.chrome-extras.tabbar.oneLiner")}
              </label>
              <Switch
                id="tabbar-one-liner"
                checked={settings.tabbarOneLiner}
                onChange={(e) =>
                  handleSettingChange("tabbarOneLiner", e.target.checked)}
              />
            </div>
          </CardContent>
        </Card>

        {/* Sidebar Settings */}
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Sidebar className="size-5" />
              {t("design.chrome-extras.sidebar.title")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex items-center justify-between">
              <label htmlFor="sidebar-overlap">
                {t("design.chrome-extras.sidebar.overlap")}
              </label>
              <Switch
                id="sidebar-overlap"
                checked={settings.sidebarOverlap}
                onChange={(e) =>
                  handleSettingChange("sidebarOverlap", e.target.checked)}
              />
            </div>
          </CardContent>
        </Card>
      </fieldset>
    </div>
  );
}
