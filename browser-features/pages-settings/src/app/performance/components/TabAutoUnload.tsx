/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import styles from "@/components/common/settings-sections.module.css";
import { Button } from "@/components/common/button.tsx";
import { Input } from "@/components/common/input.tsx";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import {
  type ChangeEvent,
  type KeyboardEvent,
  useCallback,
  useEffect,
  useState,
} from "react";
import { useTranslation } from "react-i18next";
import { Archive, ChevronDown, Globe, Plus, X } from "lucide-react";
import { NumberField } from "./NumberField.tsx";
import {
  DEFAULT_TAB_AUTO_UNLOAD_SETTINGS,
  getTabAutoUnloadSettings,
  MEMORY_THRESHOLD_MB_MIN,
  MIN_INACTIVE_MINUTES_MIN,
  saveTabAutoUnloadSettings,
  type TabAutoUnloadSettings,
} from "@/app/performance/dataManager.ts";

export function TabAutoUnload() {
  const { t } = useTranslation();
  const [settings, setSettings] = useState<TabAutoUnloadSettings>(
    DEFAULT_TAB_AUTO_UNLOAD_SETTINGS,
  );
  const [newPattern, setNewPattern] = useState("");
  const [isLoading, setIsLoading] = useState(true);
  const [saveFailed, setSaveFailed] = useState(false);
  const [showExamples, setShowExamples] = useState(true);

  useEffect(() => {
    let cancelled = false;
    (async () => {
      const loaded = await getTabAutoUnloadSettings();
      if (!cancelled) {
        setSettings(loaded);
        setIsLoading(false);
      }
    })();
    return () => {
      cancelled = true;
    };
  }, []);

  const save = useCallback(
    async (next: TabAutoUnloadSettings) => {
      setSettings(next);
      try {
        await saveTabAutoUnloadSettings(next);
        setSaveFailed(false);
      } catch (error) {
        console.error(
          "[Performance] Failed to save tab auto unload settings:",
          error,
        );
        setSaveFailed(true);
        setSettings(await getTabAutoUnloadSettings());
      }
    },
    [],
  );

  const addPattern = useCallback(() => {
    const trimmed = newPattern.trim();
    if (!trimmed || settings.excludePatterns.includes(trimmed)) {
      return;
    }
    setNewPattern("");
    void save({
      ...settings,
      excludePatterns: [...settings.excludePatterns, trimmed],
    });
  }, [newPattern, save, settings]);

  const removePattern = useCallback(
    (pattern: string) => {
      void save({
        ...settings,
        excludePatterns: settings.excludePatterns.filter((p) => p !== pattern),
      });
    },
    [save, settings],
  );

  const handleKeyDown = (event: KeyboardEvent<HTMLInputElement>) => {
    if (event.key === "Enter") {
      event.preventDefault();
      addPattern();
    }
  };

  const handleInputChange = (event: ChangeEvent<HTMLInputElement>) => {
    setNewPattern(event.currentTarget.value);
  };

  if (isLoading) {
    return (
      <Card className={styles.section}>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Archive className="size-5" />
            {t("performance.tabAutoUnload.title")}
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
    <Card className={styles.section}>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Archive className="size-5" />
          {t("performance.tabAutoUnload.title")}
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-2">
        <p className="text-sm text-muted-foreground leading-relaxed">
          {t("performance.tabAutoUnload.description")}
        </p>

        <div className={styles.row}>
          <label htmlFor="tab-auto-unload-enabled">
            {t("performance.tabAutoUnload.enable")}
          </label>
          <Switch
            id="tab-auto-unload-enabled"
            checked={settings.enabled}
            onChange={(e) =>
              void save({ ...settings, enabled: e.currentTarget.checked })}
          />
        </div>

        <div className="space-y-4 md:space-y-0">
          <NumberField
            id="tab-auto-unload-memory-threshold"
            label={t("performance.tabAutoUnload.memoryThreshold")}
            value={settings.memoryThresholdMB}
            min={MEMORY_THRESHOLD_MB_MIN}
            disabled={!settings.enabled}
            onCommit={(memoryThresholdMB) =>
              void save({ ...settings, memoryThresholdMB })}
          />
          <NumberField
            id="tab-auto-unload-min-inactive"
            label={t("performance.tabAutoUnload.minInactive")}
            value={settings.minInactiveMinutes}
            min={MIN_INACTIVE_MINUTES_MIN}
            disabled={!settings.enabled}
            onCommit={(minInactiveMinutes) =>
              void save({ ...settings, minInactiveMinutes })}
          />
        </div>

        <div className="space-y-4">
          <div>
            <h3 className="text-base font-medium mb-1">
              {t("performance.tabAutoUnload.exclusionsTitle")}
            </h3>
            <p className="text-sm text-muted-foreground">
              {t("performance.tabAutoUnload.exclusionsDescription")}
            </p>
          </div>

          <div className="flex gap-2">
            <Input
              id="tab-auto-unload-exclude-input"
              type="text"
              aria-label={t("performance.tabAutoUnload.exclusionsTitle")}
              placeholder={t("performance.tabAutoUnload.patternPlaceholder")}
              value={newPattern}
              onChange={handleInputChange}
              onKeyDown={handleKeyDown}
              className="flex-1"
            />
            <Button
              type="button"
              onClick={addPattern}
              disabled={!newPattern.trim()}
              variant="secondary"
              className="px-4 py-2 bg-primary text-primary-foreground rounded-md hover:bg-primary/90 disabled:opacity-50 disabled:cursor-not-allowed flex items-center gap-2 transition-colors"
            >
              <Plus className="size-4" />
              {t("performance.tabAutoUnload.add")}
            </Button>
          </div>

          <details
            className="group"
            open={showExamples}
            onToggle={(e) =>
              setShowExamples((e.target as HTMLDetailsElement).open)}
          >
            <summary className="flex items-center gap-2 text-sm text-muted-foreground cursor-pointer hover:text-foreground transition-colors select-none">
              <ChevronDown className="size-4 transition-transform group-open:rotate-180" />
              {t("performance.tabAutoUnload.examples")}
            </summary>
            <div className="mt-3 bg-muted/30 rounded-lg p-4 space-y-3">
              <div className="space-y-1">
                <code className="block bg-background/80 px-3 py-1.5 rounded border border-border/50 text-sm font-mono">
                  example.com
                </code>
                <p className="text-xs text-muted-foreground pl-1">
                  {t("performance.tabAutoUnload.exampleDomain")}
                </p>
              </div>
              <div className="space-y-1">
                <code className="block bg-background/80 px-3 py-1.5 rounded border border-border/50 text-sm font-mono">
                  *.google.com
                </code>
                <p className="text-xs text-muted-foreground pl-1">
                  {t("performance.tabAutoUnload.exampleWildcard")}
                </p>
              </div>
              <div className="space-y-1">
                <code className="block bg-background/80 px-3 py-1.5 rounded border border-border/50 text-sm font-mono">
                  https://mail.google.com/*
                </code>
                <p className="text-xs text-muted-foreground pl-1">
                  {t("performance.tabAutoUnload.exampleFullUrl")}
                </p>
              </div>
            </div>
          </details>

          {settings.excludePatterns.length > 0
            ? (
              <div className="space-y-2">
                {settings.excludePatterns.map((pattern) => (
                  <div
                    key={pattern}
                    className="group flex items-center justify-between bg-muted/40 hover:bg-muted/60 px-3 py-2.5 rounded-md transition-colors"
                  >
                    <div className="flex items-center gap-2.5">
                      <Globe className="size-4 text-muted-foreground shrink-0" />
                      <code className="text-sm font-mono">{pattern}</code>
                    </div>
                    <Button
                      type="button"
                      onClick={() => removePattern(pattern)}
                      variant="secondary"
                      className="opacity-60 hover:opacity-100 text-destructive p-1 rounded hover:bg-destructive/10 transition-all"
                      aria-label={t("performance.tabAutoUnload.remove")}
                    >
                      <X className="size-4" />
                    </Button>
                  </div>
                ))}
              </div>
            )
            : (
              <div className="flex items-center gap-3 py-4 px-3 border border-dashed border-muted-foreground/30 rounded-md">
                <Globe className="size-5 text-muted-foreground/50 shrink-0" />
                <p className="text-sm text-muted-foreground">
                  {t("performance.tabAutoUnload.noPatterns")}
                </p>
              </div>
            )}
        </div>

        <p className="text-xs text-muted-foreground">
          {t("performance.tabAutoUnload.cost")}
        </p>

        {saveFailed && (
          <p className="text-sm text-destructive">
            {t("performance.tabAutoUnload.saveFailed")}
          </p>
        )}
      </CardContent>
    </Card>
  );
}
