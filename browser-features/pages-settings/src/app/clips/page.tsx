import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { Clipboard, Sliders } from "lucide-react";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Input } from "@/components/common/input.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { InfoTip } from "@/components/common/infotip.tsx";
import { ConfirmModal } from "@/components/common/ConfirmModal.tsx";
import {
  type ClipsMode,
  type ClipsSettings,
  DEFAULT_SETTINGS,
  getClipsSettings,
  saveClipsSettings,
} from "./dataManager.ts";

const MODES: ClipsMode[] = ["local", "sync", "clipboard"];

export default function Page() {
  const { t } = useTranslation();
  const [settings, setSettings] = useState<ClipsSettings>(DEFAULT_SETTINGS);
  /** A mode the user picked but has not confirmed yet. */
  const [modeToConfirm, setModeToConfirm] = useState<ClipsMode | null>(null);

  /**
   * The controls wait for the read.
   *
   * What is on screen until then is the defaults, not what is stored, and a
   * change made against them would save the defaults over everything the user
   * did not touch.
   */
  const [isLoading, setIsLoading] = useState(true);

  useEffect(() => {
    let mounted = true;
    void getClipsSettings().then((loaded) => {
      if (!mounted) return;
      setSettings(loaded);
      setIsLoading(false);
    });
    return () => {
      mounted = false;
    };
  }, []);

  const update = (patch: Partial<ClipsSettings>) => {
    const next = { ...settings, ...patch };
    setSettings(next);
    void saveClipsSettings(next);
  };

  return (
    <div className="p-6 space-y-6">
      <div className="flex flex-col items-start pl-6">
        <h1 className="text-3xl font-bold mb-2">{t("clips.title")}</h1>
        <p className="text-sm mb-8">{t("clips.description")}</p>
      </div>

      <div className="space-y-8 pl-6">
        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Clipboard className="size-5" />
              {t("clips.mode")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-3">
            {MODES.map((mode) => (
              <label key={mode} className="flex items-start gap-2">
                <input
                  type="radio"
                  name="clips-mode"
                  className="radio mt-1"
                  value={mode}
                  checked={settings.mode === mode}
                  disabled={isLoading}
                  onChange={() => setModeToConfirm(mode)}
                />
                <span>
                  <span className="block text-sm font-medium">
                    {t(`clips.modes.${mode}`)}
                  </span>
                  <span className="block text-sm opacity-70">
                    {t(`clips.modeDescriptions.${mode}`)}
                  </span>
                </span>
              </label>
            ))}
            {settings.mode === "sync" && (
              <p className="text-sm opacity-70">{t("clips.syncPathWarning")}</p>
            )}
          </CardContent>
        </Card>

        <Card>
          <CardHeader>
            <CardTitle className="flex items-center gap-2">
              <Sliders className="size-5" />
              {t("clips.otherSettings")}
            </CardTitle>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="space-y-2">
              <div className="inline-flex items-center gap-2">
                <label
                  htmlFor="clips-max-items"
                  className="text-sm font-medium leading-none"
                >
                  {t("clips.maxItems")}
                </label>
                <InfoTip description={t("clips.maxItemsDescription")} />
              </div>
              <Input
                id="clips-max-items"
                type="number"
                min={1}
                disabled={isLoading}
                value={settings.maxItems || ""}
                onChange={(e) => {
                  const value = Number(e.target.value);
                  if (value > 0) update({ maxItems: value });
                }}
                className="w-full"
              />
            </div>

            <div className="flex items-center justify-between gap-2">
              <label
                htmlFor="clips-clear-on-exit"
                className="text-sm font-medium leading-none"
              >
                {t("clips.clearOnExit")}
              </label>
              <Switch
                id="clips-clear-on-exit"
                disabled={isLoading}
                checked={settings.clearOnExit}
                onChange={(e) => update({ clearOnExit: e.target.checked })}
              />
            </div>

            <div className="space-y-2">
              <label className="text-sm font-medium leading-none">
                {t("clips.fileAction")}
              </label>
              <div className="flex space-x-4">
                <label className="flex items-center space-x-2">
                  <input
                    type="radio"
                    name="clips-file-action"
                    className="radio"
                    disabled={isLoading}
                    checked={settings.fileAction === "reveal"}
                    onChange={() => update({ fileAction: "reveal" })}
                  />
                  <span>{t("clips.fileActionReveal")}</span>
                </label>
                <label className="flex items-center space-x-2">
                  <input
                    type="radio"
                    name="clips-file-action"
                    className="radio"
                    disabled={isLoading}
                    checked={settings.fileAction === "launch"}
                    onChange={() => update({ fileAction: "launch" })}
                  />
                  <span>{t("clips.fileActionLaunch")}</span>
                </label>
              </div>
            </div>
          </CardContent>
        </Card>
      </div>

      <ConfirmModal
        isOpen={modeToConfirm !== null}
        onClose={() => setModeToConfirm(null)}
        onConfirm={() => {
          if (modeToConfirm) update({ mode: modeToConfirm });
        }}
        title={t("clips.modeSwitchTitle")}
        confirmText={t("clips.modeSwitchConfirm")}
        cancelText={t("clips.modeSwitchCancel")}
      >
        {t("clips.modeSwitchWarning")}
      </ConfirmModal>
    </div>
  );
}
