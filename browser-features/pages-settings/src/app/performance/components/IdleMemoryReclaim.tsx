/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import styles from "@/components/common/settings-sections.module.css";
import {
  type ComponentProps,
  forwardRef,
  useCallback,
  useEffect,
  useState,
} from "react";
import { useTranslation } from "react-i18next";
import { MemoryStick } from "lucide-react";
import {
  Card,
  CardContent,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { NumberInput } from "@chakra-ui/react";
import type { NumberFieldProps } from "../types.ts";
import { Switch } from "@/components/common/switch.tsx";
import {
  DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS,
  getIdleMemoryReclaimSettings,
  IDLE_THRESHOLD_SEC_MIN,
  type IdleMemoryReclaimSettings,
  MIN_INTERVAL_SEC_MIN,
  MIN_RESIDENT_MB_MIN,
  saveIdleMemoryReclaimSettings,
} from "@/app/performance/dataManager.ts";

// Let React synchronize the native value in Gecko, including stepper changes.
// Remove Ark's defaultValue when supplying a controlled value through asChild.
const ControlledNumberInput = forwardRef<
  HTMLInputElement,
  ComponentProps<"input">
>(function ControlledNumberInput(
  { defaultValue: _defaultValue, ...props },
  ref,
) {
  return <input {...props} ref={ref} />;
});

/**
 * Number input that keeps the raw text while typing, so a half-typed or
 * cleared field does not immediately write a clamped value to the pref.
 */
function NumberField({
  id,
  label,
  value,
  min,
  disabled,
  onCommit,
}: NumberFieldProps) {
  const [text, setText] = useState(String(value));

  useEffect(() => {
    setText(String(value));
  }, [value]);

  const commit = (rawValue: string) => {
    const parsed = Number(rawValue);
    if (!Number.isFinite(parsed)) {
      setText(String(value));
      return;
    }

    const next = Math.max(min, Math.round(parsed));
    setText(String(next));
    if (next !== value) {
      onCommit(next);
    }
  };

  return (
    <div className={styles.field}>
      <label htmlFor={id} className="text-sm">
        {label}
      </label>
      <NumberInput.Root
        ids={{ input: id }}
        min={min}
        value={text}
        disabled={disabled}
        colorPalette="purple"
        onValueChange={({ value }) => setText(value)}
        onValueCommit={({ value }) => commit(value)}
      >
        <NumberInput.Input asChild>
          <ControlledNumberInput
            value={text}
            onChange={(event) => setText(event.currentTarget.value)}
          />
        </NumberInput.Input>
        <NumberInput.Control />
      </NumberInput.Root>
    </div>
  );
}

export function IdleMemoryReclaim() {
  const { t } = useTranslation();
  const [settings, setSettings] = useState<IdleMemoryReclaimSettings>(
    DEFAULT_IDLE_MEMORY_RECLAIM_SETTINGS,
  );
  const [isLoading, setIsLoading] = useState(true);
  const [saveFailed, setSaveFailed] = useState(false);

  useEffect(() => {
    let cancelled = false;
    (async () => {
      const loaded = await getIdleMemoryReclaimSettings();
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
    async (next: IdleMemoryReclaimSettings) => {
      setSettings(next);
      try {
        await saveIdleMemoryReclaimSettings(next);
        setSaveFailed(false);
      } catch (error) {
        console.error(
          "[Performance] Failed to save idle reclaim settings:",
          error,
        );
        setSaveFailed(true);
        setSettings(await getIdleMemoryReclaimSettings());
      }
    },
    [],
  );

  if (isLoading) {
    return (
      <Card className={styles.section}>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <MemoryStick className="size-5" />
            {t("performance.idleReclaim.title")}
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
          <MemoryStick className="size-5" />
          {t("performance.idleReclaim.title")}
        </CardTitle>
      </CardHeader>
      <CardContent className="space-y-6">
        <p className="text-sm text-muted-foreground leading-relaxed">
          {t("performance.idleReclaim.description")}
        </p>

        <div className={styles.row}>
          <label htmlFor="idle-memory-reclaim-enabled">
            {t("performance.idleReclaim.enable")}
          </label>
          <Switch
            id="idle-memory-reclaim-enabled"
            checked={settings.enabled}
            onChange={(e) =>
              save({ ...settings, enabled: e.currentTarget.checked })}
          />
        </div>

        <div className="space-y-4">
          <NumberField
            id="idle-memory-reclaim-idle-threshold"
            label={t("performance.idleReclaim.idleThreshold")}
            value={settings.idleThresholdSec}
            min={IDLE_THRESHOLD_SEC_MIN}
            disabled={!settings.enabled}
            onCommit={(idleThresholdSec) =>
              save({ ...settings, idleThresholdSec })}
          />
          <NumberField
            id="idle-memory-reclaim-min-interval"
            label={t("performance.idleReclaim.minInterval")}
            value={settings.minIntervalSec}
            min={MIN_INTERVAL_SEC_MIN}
            disabled={!settings.enabled}
            onCommit={(minIntervalSec) => save({ ...settings, minIntervalSec })}
          />
          <NumberField
            id="idle-memory-reclaim-min-resident"
            label={t("performance.idleReclaim.minResident")}
            value={settings.minResidentMB}
            min={MIN_RESIDENT_MB_MIN}
            disabled={!settings.enabled}
            onCommit={(minResidentMB) => save({ ...settings, minResidentMB })}
          />
        </div>

        <p className="text-xs text-muted-foreground">
          {t("performance.idleReclaim.cost")}
        </p>

        {saveFailed && (
          <p className="text-sm text-destructive">
            {t("performance.idleReclaim.saveFailed")}
          </p>
        )}
      </CardContent>
    </Card>
  );
}
