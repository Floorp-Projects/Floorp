import { type ChangeEvent, useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { MonitorCog } from "lucide-react";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import {
  getTabWindowBehaviorSettings,
  type OpenNewWindowValue,
  setOpenNewWindow,
  setTaskbarPreviews,
  type TabWindowBehaviorSettings,
} from "@/app/design/tabWindowBehavior.ts";

export function TabWindowBehavior() {
  const { t } = useTranslation();
  const [settings, setSettings] = useState<
    TabWindowBehaviorSettings | null
  >(null);

  useEffect(() => {
    let mounted = true;

    const loadSettings = async () => {
      try {
        const values = await getTabWindowBehaviorSettings();
        if (mounted) {
          setSettings(values);
        }
      } catch (error) {
        console.error("Failed to load tab and window behavior settings", error);
      }
    };

    void loadSettings();
    globalThis.addEventListener("focus", loadSettings);

    return () => {
      mounted = false;
      globalThis.removeEventListener("focus", loadSettings);
    };
  }, []);

  const handleOpenNewWindowChange = async (
    event: ChangeEvent<HTMLSelectElement>,
  ) => {
    const previous = settings?.openNewWindow;
    const value = Number(event.target.value) as OpenNewWindowValue;
    setSettings((current) =>
      current ? { ...current, openNewWindow: value } : current
    );
    try {
      await setOpenNewWindow(value);
    } catch (error) {
      console.error("Failed to save link opening behavior", error);
      setSettings((current) =>
        current ? { ...current, openNewWindow: previous } : current
      );
    }
  };

  const handleTaskbarPreviewsChange = async (
    event: ChangeEvent<HTMLInputElement>,
  ) => {
    const previous = settings?.taskbarPreviews;
    const enabled = event.target.checked;
    setSettings((current) =>
      current ? { ...current, taskbarPreviews: enabled } : current
    );
    try {
      await setTaskbarPreviews(enabled);
    } catch (error) {
      console.error("Failed to save taskbar preview behavior", error);
      setSettings((current) =>
        current ? { ...current, taskbarPreviews: previous } : current
      );
    }
  };

  const taskbarPreviewsAvailable = settings !== null &&
    settings.taskbarPreviews !== null;

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <MonitorCog className="size-5" />
          {t("design.tabWindowBehavior.title")}
        </CardTitle>
        <CardDescription>
          {t("design.tabWindowBehavior.description")}
        </CardDescription>
      </CardHeader>
      <CardContent className="space-y-5">
        <div className="space-y-2">
          <label
            className="text-base font-medium"
            htmlFor="open-new-window-behavior"
          >
            {t("design.tabWindowBehavior.openLinks")}
          </label>
          <p className="text-sm text-muted-foreground">
            {t("design.tabWindowBehavior.openLinksDescription")}
          </p>
          <select
            id="open-new-window-behavior"
            className="select select-bordered w-full"
            value={settings?.openNewWindow ?? ""}
            onChange={handleOpenNewWindowChange}
            disabled={settings === null}
          >
            <option value="1">
              {t("design.tabWindowBehavior.openCurrentWindowOrTab")}
            </option>
            <option value="2">
              {t("design.tabWindowBehavior.openNewWindow")}
            </option>
            <option value="3">
              {t("design.tabWindowBehavior.openNewTab")}
            </option>
          </select>
        </div>

        <div className="flex items-start justify-between gap-4">
          <div>
            <label
              className="text-base font-medium"
              htmlFor="taskbar-tab-previews"
            >
              {t("design.tabWindowBehavior.taskbarPreviews")}
            </label>
            <p className="text-sm text-muted-foreground">
              {taskbarPreviewsAvailable
                ? t("design.tabWindowBehavior.taskbarPreviewsDescription")
                : t("design.tabWindowBehavior.taskbarPreviewsUnavailable")}
            </p>
          </div>
          <Switch
            id="taskbar-tab-previews"
            checked={settings?.taskbarPreviews ?? false}
            onChange={handleTaskbarPreviewsChange}
            disabled={!taskbarPreviewsAvailable}
          />
        </div>
      </CardContent>
    </Card>
  );
}
