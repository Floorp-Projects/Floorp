import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { RestartModal } from "@/components/common/restart-modal.tsx";
import { useTranslation } from "react-i18next";
import { useFormContext } from "react-hook-form";
import { Command, Keyboard } from "lucide-react";
import { Link } from "react-router-dom";
import { useState } from "react";
import {
  shortcutToString,
  useKeyboardShortcutConfig,
} from "../../keyboard-shortcut/dataManager.ts";

export function BasicSettings() {
  const { t } = useTranslation();
  const { getValues, setValue } = useFormContext();
  const [showRestartModal, setShowRestartModal] = useState(false);
  const { config: shortcutConfig, loading: shortcutLoading } =
    useKeyboardShortcutConfig();

  const toggleShortcut = shortcutConfig?.shortcuts?.["floorp-toggle-command-palette"];
  const isShortcutFeatureEnabled = shortcutConfig?.enabled === true;
  const hasShortcut = !shortcutLoading
    && isShortcutFeatureEnabled
    && toggleShortcut !== undefined
    && typeof toggleShortcut.key === "string"
    && toggleShortcut.key.length > 0;
  const shortcutDisplay = toggleShortcut && hasShortcut
    ? shortcutToString(toggleShortcut)
    : "";

  return (
    <>
      {showRestartModal
        ? (
          <RestartModal
            onClose={() => setShowRestartModal(false)}
            label={t("commandPalette.needRestartDescriptionForEnable")}
          />
        )
        : null}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Command className="size-5" />
            {t("commandPalette.basicSettings")}
          </CardTitle>
          <CardDescription>
            {t("commandPalette.description")}
          </CardDescription>
        </CardHeader>
        <CardContent className="space-y-3">
          <div>
            <div className="mb-2 inline-flex items-center gap-2">
              <h3 className="text-base font-medium">
                {t("commandPalette.enableOrDisable")}
              </h3>
            </div>
            <div className="space-y-1">
              <div className="flex items-center justify-between gap-2">
                <label
                  htmlFor="enable-command-palette"
                  className="flex flex-col gap-1.5"
                >
                  <span>{t("commandPalette.enable")}</span>
                  <span className="font-normal text-sm text-base-content/70">
                    {t("commandPalette.enableDescription")}
                  </span>
                </label>
                <Switch
                  id="enable-command-palette"
                  checked={getValues("enabled")}
                  onChange={(e) => {
                    const wasEnabled = getValues("enabled");
                    setValue("enabled", e.target.checked);
                    if (!wasEnabled && e.target.checked) {
                      setShowRestartModal(true);
                    }
                  }}
                />
              </div>
            </div>
          </div>

          <div className="flex items-start gap-2 rounded-lg border border-base-300/40 p-3 bg-base-200/40 dark:bg-base-700/30">
            <Keyboard className="size-5 mt-0.5 shrink-0 text-base-content/70" />
            <div className="space-y-1">
              <p className="text-base font-medium">
                {t("commandPalette.launchHintTitle")}
              </p>
              <p className="text-sm text-base-content/70">
                {hasShortcut
                  ? t("commandPalette.launchHint", { key: shortcutDisplay })
                  : t("commandPalette.launchHintNoShortcut")}{" "}
                <Link
                  to="/features/shortcuts"
                  className="text-[var(--link-text-color)] hover:underline"
                >
                  {t("commandPalette.launchShortcutLink")}
                </Link>
              </p>
            </div>
          </div>
        </CardContent>
      </Card>
    </>
  );
}
