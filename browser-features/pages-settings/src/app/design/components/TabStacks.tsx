import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Layers } from "lucide-react";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { InfoTip } from "@/components/common/infotip.tsx";
import { RestartModal } from "@/components/common/restart-modal.tsx";
import {
  getTabStacksSettings,
  setTabStacksEnabled,
  type TabStacksSettings,
} from "@/app/design/tabStacks.ts";

export function TabStacks() {
  const { t } = useTranslation();
  const [settings, setSettings] = useState<TabStacksSettings>({
    enabled: false,
  });
  const [isLoading, setIsLoading] = useState(true);
  const [showRestartModal, setShowRestartModal] = useState(false);

  useEffect(() => {
    let mounted = true;

    const loadSettings = async () => {
      try {
        const loaded = await getTabStacksSettings();
        if (mounted) {
          setSettings(loaded);
        }
      } catch (error) {
        console.error("[tab-stacks] Failed to load tab stacks settings:", error);
      } finally {
        if (mounted) {
          setIsLoading(false);
        }
      }
    };

    void loadSettings();
    globalThis.addEventListener("focus", loadSettings);

    return () => {
      mounted = false;
      globalThis.removeEventListener("focus", loadSettings);
    };
  }, []);

  const handleEnabledChange = async (enabled: boolean) => {
    const previous = settings.enabled;
    setSettings((current) => ({ ...current, enabled }));
    try {
      await setTabStacksEnabled(enabled);
      setShowRestartModal(true);
    } catch (error) {
      console.error("[tab-stacks] Failed to save tab stacks settings:", error);
      setSettings((current) => ({ ...current, enabled: previous }));
    }
  };

  return (
    <>
      {showRestartModal
        ? (
          <RestartModal
            onClose={() => setShowRestartModal(false)}
            label={t("design.tabStacks.needRestartDescriptionForEnableAndDisable")}
          />
        )
        : null}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Layers className="size-5" />
            {t("design.tabStacks.title")}
          </CardTitle>
          <CardDescription>
            {t("design.tabStacks.description")}
          </CardDescription>
        </CardHeader>
        <CardContent className="space-y-3">
          <div>
            <div className="mb-2 inline-flex items-center gap-2">
              <h3 className="text-base font-medium">
                {t("design.tabStacks.enableOrDisable")}
              </h3>
              <InfoTip
                description={t("design.tabStacks.enableTabStacksDescription")}
              />
            </div>
            <div className="space-y-1">
              <div className="flex items-center justify-between gap-2">
                <div className="space-y-1">
                  <label htmlFor="enable-tab-stacks">
                    {t("design.tabStacks.enableTabStacks")}
                  </label>
                </div>
                <Switch
                  id="enable-tab-stacks"
                  checked={settings.enabled}
                  disabled={isLoading}
                  onChange={(e) => handleEnabledChange(e.target.checked)}
                />
              </div>
              <div className="text-sm text-base-content/70">
                {t("design.tabStacks.needRestartDescriptionForEnableAndDisable")}
              </div>
            </div>
          </div>
        </CardContent>
      </Card>
    </>
  );
}
