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
  useMemo,
  useRef,
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
// Ark's input handler also writes the value and restores a captured caret in a
// later frame, which can race React's controlled onChange. Keep its validation,
// keyboard and stepper handlers, but let React alone handle text input updates.
// Drop Ark's defaultValue because this input receives a controlled value.
const ControlledNumberInput = forwardRef<
  HTMLInputElement,
  ComponentProps<"input">
>(function ControlledNumberInput(
  { defaultValue: _defaultValue, onInput: _onInput, ...props },
  ref,
) {
  return <input {...props} ref={ref} />;
});

// Convert a draft back to a number with the same locale rules as the
// formatter: localized digits, then the localized decimal separator.
const DIGIT_ZEROES = [
  0x0030, // ASCII
  0x0660, // Arabic-Indic
  0x06f0, // Extended Arabic-Indic
  0x0966, // Devanagari
  0x0e50, // Thai
  0xff10, // Fullwidth
];
function parseLocalizedNumber(
  text: string,
  formatter: Intl.NumberFormat,
): number {
  const parts = formatter.formatToParts(1111.5);
  const group = parts.find((part) => part.type === "group")?.value;
  const decimals = new Set([
    parts.find((part) => part.type === "decimal")?.value ?? ".",
    ".",
    ",",
    "٫", // U+066B Arabic decimal separator
  ]);
  let normalized = "";
  for (const char of text.trim().normalize("NFKC")) {
    const code = char.codePointAt(0)!;
    const zero = DIGIT_ZEROES.find((z) => code >= z && code <= z + 9);
    if (zero !== undefined) {
      normalized += String(code - zero);
    } else if (decimals.has(char)) {
      normalized += ".";
    } else if (char !== group) {
      normalized += char;
    }
  }
  return Number(normalized);
}

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
  const { i18n } = useTranslation();
  const locale = i18n.language || "en-US";
  const formatOptions = useMemo<Intl.NumberFormatOptions>(() => ({
    useGrouping: false,
    maximumFractionDigits: 20,
  }), []);
  const formatter = useMemo(
    () => new Intl.NumberFormat(locale, formatOptions),
    [locale, formatOptions],
  );
  const [text, setText] = useState(() => formatter.format(value));
  const composing = useRef(false);
  // Ark's controlled-value watcher reformats and rewrites the DOM value on any
  // change, which drops a separately typed decimal separator and races IME
  // composition. Feed it only the committed value and let React own the draft.
  const committed = useMemo(
    () => formatter.format(value),
    [formatter, value],
  );

  useEffect(() => {
    setText(formatter.format(value));
  }, [value, formatter]);

  const commit = (parsed: number) => {
    if (composing.current) return;
    if (!Number.isFinite(parsed)) {
      setText(formatter.format(value));
      return;
    }

    const next = Math.max(min, Math.round(parsed));
    setText(formatter.format(next));
    if (next !== value) {
      onCommit(next);
    }
  };

  // Zag derives steps from the controlled value, which is pinned to the saved
  // pref — stepping would restart from it and ignore the visible draft. Step
  // from the parsed draft instead so repeated and post-edit steps accumulate.
  const onStepKey = (event: React.KeyboardEvent<HTMLInputElement>) => {
    if (event.key !== "ArrowUp" && event.key !== "ArrowDown") return;
    event.preventDefault();
    if (composing.current) return;
    const parsed = text.trim() === "" ? 0 : parseLocalizedNumber(text, formatter);
    const base = Number.isFinite(parsed) ? parsed : value;
    const next = Math.max(
      min,
      Math.round(base + (event.key === "ArrowUp" ? 1 : -1)),
    );
    setText(formatter.format(next));
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
        locale={locale}
        // Use the same locale parser for validation and numeric conversion.
        // Without formatOptions, Ark accepts localized digits but then parses
        // them with parseFloat, which can erase or truncate the draft.
        formatOptions={formatOptions}
        value={committed}
        disabled={disabled}
        colorPalette="purple"
        onValueChange={({ value }) => {
          if (!composing.current) setText(value);
        }}
        onValueCommit={() =>
          commit(
            text.trim() === "" ? 0 : parseLocalizedNumber(text, formatter),
          )}
      >
        <NumberInput.Input asChild>
          <ControlledNumberInput
            value={text}
            onKeyDown={onStepKey}
            onChange={(event) => setText(event.currentTarget.value)}
            onCompositionStart={() => {
              composing.current = true;
            }}
            onCompositionEnd={(event) => {
              composing.current = false;
              setText(event.currentTarget.value);
            }}
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
      <CardContent className="space-y-2">
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

        <div className="space-y-4 md:space-y-0">
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
