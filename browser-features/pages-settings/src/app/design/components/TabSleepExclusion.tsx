import { useCallback, useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import { ChevronDown, Globe, Moon, Plus, X } from "lucide-react";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Input } from "@/components/common/input.tsx";
import { Switch } from "@/components/common/switch.tsx";
import {
  getTabSleepExclusionSettings,
  saveTabSleepExclusionSettings,
  type TabSleepExclusionSettings,
} from "@/app/design/dataManager.ts";

export function TabSleepExclusion() {
  const { t } = useTranslation();
  const [settings, setSettings] = useState<TabSleepExclusionSettings>({
    enabled: true,
    patterns: [],
  });
  const [newPattern, setNewPattern] = useState("");
  const [isLoading, setIsLoading] = useState(true);
  const [showExamples, setShowExamples] = useState(true);

  // Load settings on mount
  useEffect(() => {
    const loadSettings = async () => {
      try {
        const loaded = await getTabSleepExclusionSettings();
        setSettings(loaded);
      } catch (error) {
        console.error("Failed to load tab sleep exclusion settings:", error);
      } finally {
        setIsLoading(false);
      }
    };
    loadSettings();
  }, []);

  // Save settings when they change
  const saveSettings = useCallback(
    async (newSettings: TabSleepExclusionSettings) => {
      setSettings(newSettings);
      try {
        await saveTabSleepExclusionSettings(newSettings);
      } catch (error) {
        console.error("Failed to save tab sleep exclusion settings:", error);
      }
    },
    [],
  );

  const handleEnabledChange = (enabled: boolean) => {
    saveSettings({ ...settings, enabled });
  };

  const handleAddPattern = () => {
    const trimmed = newPattern.trim();
    if (trimmed && !settings.patterns.includes(trimmed)) {
      saveSettings({
        ...settings,
        patterns: [...settings.patterns, trimmed],
      });
      setNewPattern("");
    }
  };

  const handleRemovePattern = (pattern: string) => {
    saveSettings({
      ...settings,
      patterns: settings.patterns.filter((p) => p !== pattern),
    });
  };

  const handleKeyDown = (e: React.KeyboardEvent<HTMLInputElement>) => {
    if (e.key === "Enter") {
      e.preventDefault();
      handleAddPattern();
    }
  };

  if (isLoading) {
    return (
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Moon className="size-5" />
            {t("design.tabSleepExclusion.title")}
          </CardTitle>
        </CardHeader>
        <CardContent>
          <div className="flex items-center gap-2 text-muted-foreground">
            <div className="size-4 rounded-full bg-muted animate-pulse" />
            <span className="text-sm">{t("common.loading")}</span>
          </div>
        </CardContent>
      </Card>
    );
  }

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Moon className="size-5" />
          {t("design.tabSleepExclusion.title")}
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-6">
        {/* Description */}
        <p className="text-sm text-muted-foreground leading-relaxed">
          {t("design.tabSleepExclusion.description")}
        </p>

        {/* Enable/Disable */}
        <div className="flex items-center justify-between gap-2">
          <label htmlFor="tab-sleep-exclusion-enabled">
            {t("design.tabSleepExclusion.enable")}
          </label>
          <Switch
            id="tab-sleep-exclusion-enabled"
            checked={settings.enabled}
            onChange={(e) => handleEnabledChange(e.target.checked)}
          />
        </div>

        {/* Pattern Section */}
        <div
          className={`space-y-4 transition-opacity duration-200 ${
            settings.enabled ? "" : "opacity-50 pointer-events-none"
          }`}
        >
          <div>
            <h3 className="text-base font-medium mb-1">
              {t("design.tabSleepExclusion.patterns")}
            </h3>
            <p className="text-sm text-muted-foreground">
              {t("design.tabSleepExclusion.patternsDescription")}
            </p>
          </div>

          {/* Add Pattern Input */}
          <div className="flex gap-2">
            <Input
              type="text"
              placeholder={t("design.tabSleepExclusion.patternPlaceholder")}
              value={newPattern}
              onChange={(e: React.ChangeEvent<HTMLInputElement>) =>
                setNewPattern(e.target.value)}
              onKeyDown={handleKeyDown}
              className="flex-1"
            />
            <button
              type="button"
              onClick={handleAddPattern}
              disabled={!newPattern.trim()}
              className="px-4 py-2 bg-primary text-primary-foreground rounded-md hover:bg-primary/90 disabled:opacity-50 disabled:cursor-not-allowed flex items-center gap-2 transition-colors"
            >
              <Plus className="size-4" />
              {t("design.tabSleepExclusion.add")}
            </button>
          </div>

          {/* Pattern Examples - Collapsible */}
          <details
            className="group"
            open={showExamples}
            onToggle={(e) =>
              setShowExamples((e.target as HTMLDetailsElement).open)}
          >
            <summary className="flex items-center gap-2 text-sm text-muted-foreground cursor-pointer hover:text-foreground transition-colors select-none">
              <ChevronDown className="size-4 transition-transform group-open:rotate-180" />
              {t("design.tabSleepExclusion.examples")}
            </summary>
            <div className="mt-3 bg-muted/30 rounded-lg p-4 space-y-3">
              <div className="space-y-1">
                <code className="block bg-background/80 px-3 py-1.5 rounded border border-border/50 text-sm font-mono">
                  example.com
                </code>
                <p className="text-xs text-muted-foreground pl-1">
                  {t("design.tabSleepExclusion.exampleDomain")}
                </p>
              </div>
              <div className="space-y-1">
                <code className="block bg-background/80 px-3 py-1.5 rounded border border-border/50 text-sm font-mono">
                  *.google.com
                </code>
                <p className="text-xs text-muted-foreground pl-1">
                  {t("design.tabSleepExclusion.exampleWildcard")}
                </p>
              </div>
              <div className="space-y-1">
                <code className="block bg-background/80 px-3 py-1.5 rounded border border-border/50 text-sm font-mono">
                  https://mail.google.com/*
                </code>
                <p className="text-xs text-muted-foreground pl-1">
                  {t("design.tabSleepExclusion.exampleFullUrl")}
                </p>
              </div>
            </div>
          </details>

          {/* Pattern List */}
          {settings.patterns.length > 0
            ? (
              <div className="space-y-2">
                {settings.patterns.map((pattern) => (
                  <div
                    key={pattern}
                    className="group flex items-center justify-between bg-muted/40 hover:bg-muted/60 px-3 py-2.5 rounded-md transition-colors"
                  >
                    <div className="flex items-center gap-2.5">
                      <Globe className="size-4 text-muted-foreground shrink-0" />
                      <code className="text-sm font-mono">{pattern}</code>
                    </div>
                    <button
                      type="button"
                      onClick={() => handleRemovePattern(pattern)}
                      className="opacity-60 hover:opacity-100 text-destructive p-1 rounded hover:bg-destructive/10 transition-all"
                      aria-label={t("design.tabSleepExclusion.remove")}
                    >
                      <X className="size-4" />
                    </button>
                  </div>
                ))}
              </div>
            )
            : (
              <div className="flex items-center gap-3 py-4 px-3 border border-dashed border-muted-foreground/30 rounded-md">
                <Globe className="size-5 text-muted-foreground/50 shrink-0" />
                <p className="text-sm text-muted-foreground">
                  {t("design.tabSleepExclusion.noPatterns")}
                </p>
              </div>
            )}
        </div>
      </CardContent>
    </Card>
  );
}
