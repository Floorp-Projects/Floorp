/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { Button } from "../../../../../../libs/ui/button.tsx";
import { Modal } from "../../../../../../libs/ui/modal.tsx";
import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import {
  codeToXulKey,
  comboToCanonicalCode,
  type Modifiers,
  modifiersToXulString,
  type XulCombo,
  xulKeyToCode,
  xulStringToModifiers,
} from "../../../../../modules/common/NRKeySyncCombo.ts";
import type { FirefoxKeyEntry } from "../../../../../modules/common/NRKeySyncTypes.ts";
import {
  getRecordedShortcutCode,
  type ShortcutConfig,
} from "../../../types/pref.ts";
import { Input } from "@/components/common/input.tsx";
import { keySync } from "../../../lib/rpc/keysync.ts";
import { useKeyboardShortcutConfig } from "../dataManager.ts";
import { useFirefoxKeys } from "../firefoxKeysManager.ts";
import {
  formatModifierLabel,
  formatModifierSymbol,
  isMac,
} from "../platform.ts";
import type { FirefoxKeyEditorProps } from "../types.ts";

/** Floorp shortcut that the edited combo would shadow at runtime. */
interface FloorpConflict {
  actionId: string;
  shortcutText: string;
}

/**
 * Editor dialog for one mirrored Firefox `<key>`. Records a combo the same
 * way as ShortcutEditor, warns about (non-blocking) conflicts with sibling
 * Firefox keys and with Floorp's own shortcuts (which keep runtime
 * priority), and persists the change through keySync.changeKey so it lands
 * in customKeys.json. The list refreshes via the push subscription.
 */
export const FirefoxKeyEditor = ({ entry, onClose }: FirefoxKeyEditorProps) => {
  const { t } = useTranslation();
  const [modifiers, setModifiers] = useState<Modifiers>(() =>
    xulStringToModifiers(entry.modifiers, isMac())
  );
  const [code, setCode] = useState<string>(
    () => xulKeyToCode({ key: entry.key, keycode: entry.keycode }) ?? "",
  );
  const [isRecording, setIsRecording] = useState(false);
  const [keyConflict, setKeyConflict] = useState<FirefoxKeyEntry | null>(null);
  const [floorpConflict, setFloorpConflict] = useState<FloorpConflict | null>(
    null,
  );
  const [saveError, setSaveError] = useState<string | null>(null);
  const [saving, setSaving] = useState(false);
  const { keys } = useFirefoxKeys();
  const { config } = useKeyboardShortcutConfig();

  const formatKeyCode = (value: string): string => {
    if (!value) return "";
    return value.replace(/^(Key|Digit|Arrow)/, "");
  };

  const normalizeKeyCode = (value: string): string => {
    if (/^[A-Z]$/.test(value)) {
      return `Key${value}`;
    }
    if (/^[0-9]$/.test(value)) {
      return `Digit${value}`;
    }
    return value;
  };

  const formatFloorpShortcutText = (shortcut: ShortcutConfig): string => {
    const parts: string[] = [];
    if (shortcut.modifiers.alt) parts.push(formatModifierSymbol("alt"));
    if (shortcut.modifiers.ctrl) parts.push(formatModifierSymbol("ctrl"));
    if (shortcut.modifiers.meta) parts.push(formatModifierSymbol("meta"));
    if (shortcut.modifiers.shift) parts.push(formatModifierSymbol("shift"));
    if (shortcut.key) parts.push(formatKeyCode(shortcut.key).toUpperCase());
    return parts.join(" + ");
  };

  /**
   * Non-blocking conflict checks: the edited combo is compared against the
   * other mirrored Firefox keys and against Floorp's own shortcuts. Both
   * are warnings only — saving stays possible.
   */
  /**
   * Resolves a Floorp action id to a display label. Keyboard-only actions
   * live under `keyboardShortcut.actionLabels`, everything else under
   * `mouseGesture.actions` (same fallback chain as the shortcut action
   * list); the raw id is the final fallback.
   */
  const floorpActionLabel = (actionId: string): string => {
    const keyboardOnlyLabel = t(
      `keyboardShortcut.actionLabels.${actionId}`,
    );
    if (keyboardOnlyLabel !== `keyboardShortcut.actionLabels.${actionId}`) {
      return keyboardOnlyLabel;
    }
    return t(`mouseGesture.actions.${actionId}`, actionId);
  };
  const checkConflicts = (
    nextModifiers: Modifiers,
    nextCode: string,
  ): void => {
    if (!nextCode) {
      setKeyConflict(null);
      setFloorpConflict(null);
      return;
    }
    const canonical = comboToCanonicalCode(nextModifiers, nextCode);

    const otherKey = keys.find(
      (candidate) =>
        candidate.id !== entry.id &&
        candidate.canonicalCode !== null &&
        candidate.canonicalCode === canonical,
    );
    setKeyConflict(otherKey ?? null);

    const floorpMatch = Object.entries(config.shortcuts ?? {}).find(
      ([, shortcut]) =>
        comboToCanonicalCode(
          shortcut.modifiers,
          normalizeKeyCode(shortcut.key),
        ) === canonical,
    );
    setFloorpConflict(
      floorpMatch
        ? {
          actionId: floorpMatch[0],
          shortcutText: formatFloorpShortcutText(floorpMatch[1]),
        }
        : null,
    );
  };

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      e.preventDefault();
      e.stopPropagation();

      const recorded = getRecordedShortcutCode(e);
      if (!recorded) {
        return;
      }

      setCode(recorded);
      setIsRecording(false);
      checkConflicts(modifiers, normalizeKeyCode(recorded));
    };

    if (isRecording) {
      globalThis.addEventListener("keydown", handleKeyDown);
    }

    return () => {
      globalThis.removeEventListener("keydown", handleKeyDown);
    };
    // `modifiers` is included so the recorded-key conflict check always sees
    // the current checkbox state even when toggled mid-recording.
  }, [isRecording, modifiers]);

  useEffect(() => {
    if (code) {
      checkConflicts(modifiers, normalizeKeyCode(code));
    }
  }, [modifiers]);

  const previewText = (): string => {
    const parts: string[] = [];
    if (modifiers.alt) parts.push(formatModifierSymbol("alt"));
    if (modifiers.ctrl) parts.push(formatModifierSymbol("ctrl"));
    if (modifiers.meta) parts.push(formatModifierSymbol("meta"));
    if (modifiers.shift) parts.push(formatModifierSymbol("shift"));
    if (code) parts.push(formatKeyCode(code).toUpperCase());
    return parts.join(" + ");
  };

  // Null when the recorded code has no XUL representation and cannot be saved.
  const xulKeyDefinition = code ? codeToXulKey(normalizeKeyCode(code)) : null;

  const handleSave = async (): Promise<void> => {
    if (saving || !code || !xulKeyDefinition) return;
    setSaving(true);
    setSaveError(null);
    try {
      const combo: XulCombo = {
        modifiers: modifiersToXulString(modifiers, isMac()),
        ...("key" in xulKeyDefinition
          ? { key: xulKeyDefinition.key }
          : { keycode: xulKeyDefinition.keycode }),
      };
      const result = await keySync.changeKey(entry.id, combo);
      if (result.ok) {
        onClose();
        return;
      }
      setSaveError(
        t("keyboardShortcut.firefoxKeySaveFailed", {
          error: result.error ?? "",
        }),
      );
    } catch (error) {
      console.error(
        "[KeyboardShortcut] Could not save Firefox key",
        entry.id,
        error,
      );
      setSaveError(
        t("keyboardShortcut.firefoxKeySaveFailed", {
          error: error instanceof Error ? error.message : String(error),
        }),
      );
    } finally {
      setSaving(false);
    }
  };

  return (
    <Modal
      title={t("keyboardShortcut.firefoxKeyEditTitle")}
      onClose={onClose}
      closeLabel={t("keyboardShortcut.cancel")}
      closeOnEscape={!isRecording}
    >
      <div className="mb-4 flex items-center gap-2">
        <span className="text-sm font-medium">{entry.label}</span>
        {entry.customized && (
          <span className="badge badge-info badge-outline badge-sm">
            {t("keyboardShortcut.firefoxKeyCustomized")}
          </span>
        )}
      </div>
      <div className="space-y-6">
        <div className="bg-base-200 rounded-lg p-4">
          <div className="text-sm text-base-content/70 mb-2">
            {t("keyboardShortcut.preview")}
          </div>
          <div
            className={`text-xl font-mono ${isRecording ? "text-primary" : ""}`}
          >
            {previewText() || t("keyboardShortcut.pressKey")}
          </div>
        </div>

        {!xulKeyDefinition && code && (
          <div role="alert" className="floorp-notice floorp-notice-error">
            <span>{t("keyboardShortcut.invalidKey")}</span>
          </div>
        )}

        {saveError && (
          <div role="alert" className="floorp-notice floorp-notice-error">
            <span>{saveError}</span>
          </div>
        )}

        {keyConflict && (
          <div className="floorp-notice floorp-notice-warning">
            <span className="text-sm">
              {t("keyboardShortcut.firefoxKeyConflict", {
                label: keyConflict.label,
                shortcut: keyConflict.shortcutText,
              })}
            </span>
          </div>
        )}

        {floorpConflict && (
          <div className="floorp-notice floorp-notice-warning">
            <span className="text-sm">
              {t("keyboardShortcut.firefoxKeyConflict", {
                label: floorpActionLabel(floorpConflict.actionId),
                shortcut: floorpConflict.shortcutText,
              })}
            </span>
          </div>
        )}

        <div className="grid grid-cols-2 gap-4">
          <label className="floorp-field-label">
            <input
              type="checkbox"
              className="floorp-checkbox"
              aria-label={formatModifierLabel("alt")}
              checked={modifiers.alt}
              onChange={(e) =>
                setModifiers((prev) => ({
                  ...prev,
                  alt: e.target.checked,
                }))}
            />
            <span>{formatModifierLabel("alt")}</span>
          </label>
          <label className="floorp-field-label">
            <input
              type="checkbox"
              className="floorp-checkbox"
              aria-label={formatModifierLabel("ctrl")}
              checked={modifiers.ctrl}
              onChange={(e) =>
                setModifiers((prev) => ({
                  ...prev,
                  ctrl: e.target.checked,
                }))}
            />
            <span>{formatModifierLabel("ctrl")}</span>
          </label>
          <label className="floorp-field-label">
            <input
              type="checkbox"
              className="floorp-checkbox"
              aria-label={formatModifierLabel("meta")}
              checked={modifiers.meta}
              onChange={(e) =>
                setModifiers((prev) => ({
                  ...prev,
                  meta: e.target.checked,
                }))}
            />
            <span>{formatModifierLabel("meta")}</span>
          </label>
          <label className="floorp-field-label">
            <input
              type="checkbox"
              className="floorp-checkbox"
              aria-label={formatModifierLabel("shift")}
              checked={modifiers.shift}
              onChange={(e) =>
                setModifiers((prev) => ({
                  ...prev,
                  shift: e.target.checked,
                }))}
            />
            <span>{formatModifierLabel("shift")}</span>
          </label>
        </div>

        <div className="floorp-field">
          <label className="floorp-field-label">
            <span className="floorp-field-text">
              {t("keyboardShortcut.key")}
            </span>
          </label>
          <div className="relative">
            <Input
              aria-label={t("keyboardShortcut.key")}
              type="text"
              className="w-full"
              aria-invalid={Boolean(keyConflict)}
              value={formatKeyCode(code)}
              readOnly
              placeholder={t("keyboardShortcut.pressKey")}
              onFocus={() => setIsRecording(true)}
              onBlur={() => setIsRecording(false)}
            />
            {isRecording && (
              <div className="absolute inset-0 flex items-center justify-center rounded-lg bg-base-200/50">
                <span className="text-primary">
                  {t("keyboardShortcut.recording")}
                </span>
              </div>
            )}
          </div>
        </div>
      </div>

      <div className="floorp-form-actions">
        <Button type="button" variant="secondary" onClick={onClose}>
          {t("keyboardShortcut.cancel")}
        </Button>
        <Button
          type="button"
          variant="primary"
          onClick={handleSave}
          aria-disabled={saving}
          aria-busy={saving}
          disabled={!code || !xulKeyDefinition}
        >
          {t("keyboardShortcut.save")}
        </Button>
      </div>
    </Modal>
  );
};
