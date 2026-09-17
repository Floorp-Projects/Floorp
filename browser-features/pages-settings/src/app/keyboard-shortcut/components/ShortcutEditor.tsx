import { Button } from "../../../../../../libs/ui/button.tsx";
import { Modal } from "../../../../../../libs/ui/modal.tsx";
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useEffect, useState } from "react";
import { useTranslation } from "react-i18next";
import {
  getRecordedShortcutCode,
  type ShortcutConfig,
} from "../../../types/pref.ts";
import { Input } from "@/components/common/input.tsx";
import { formatModifierLabel, formatModifierSymbol } from "../platform.ts";
import type { ShortcutEditorProps } from "../types.ts";

export const ShortcutEditor = ({
  isOpen,
  onClose,
  onSave,
  initialShortcut,
  existingShortcuts,
  actionId,
}: ShortcutEditorProps) => {
  const { t } = useTranslation();
  const [shortcut, setShortcut] = useState<ShortcutConfig>(
    initialShortcut || {
      modifiers: {
        alt: false,
        ctrl: false,
        meta: false,
        shift: false,
      },
      key: "",
      action: actionId,
    },
  );
  const [isRecording, setIsRecording] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [saveError, setSaveError] = useState(false);
  const [saving, setSaving] = useState(false);

  const handleSave = async () => {
    if (saving || !shortcut.key || error) return;
    setSaving(true);
    setSaveError(false);
    try {
      if (await onSave(shortcut)) onClose();
      else setSaveError(true);
    } catch (error) {
      console.error("[KeyboardShortcut] Could not save shortcut", error);
      setSaveError(true);
    } finally {
      setSaving(false);
    }
  };

  const formatKeyCode = (code: string) => {
    if (!code) return "";
    return code.replace(/^(Key|Digit|Arrow)/, "");
  };

  const normalizeKeyCode = (code: string): string => {
    if (/^[A-Z]$/.test(code)) {
      return `Key${code}`;
    }
    if (/^[0-9]$/.test(code)) {
      return `Digit${code}`;
    }
    return code;
  };

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      e.preventDefault();
      e.stopPropagation();

      const code = getRecordedShortcutCode(e);
      if (!code) {
        return;
      }

      setShortcut((prev) => ({
        ...prev,
        key: code,
      }));
      setIsRecording(false);
      checkDuplicate(code);
    };

    if (isOpen && isRecording) {
      globalThis.addEventListener("keydown", handleKeyDown);
    }

    return () => {
      globalThis.removeEventListener("keydown", handleKeyDown);
    };
  }, [isOpen, isRecording]);

  const checkDuplicate = (key: string) => {
    const newShortcut = {
      ...shortcut,
      key: normalizeKeyCode(key),
    };

    const otherShortcuts = existingShortcuts.filter(
      (s) => s.action !== initialShortcut?.action,
    );

    const isDuplicate = otherShortcuts.some((existing) => {
      const normalizedExistingKey = normalizeKeyCode(existing.key);
      return (
        normalizedExistingKey === newShortcut.key &&
        existing.modifiers.alt === newShortcut.modifiers.alt &&
        existing.modifiers.ctrl === newShortcut.modifiers.ctrl &&
        existing.modifiers.meta === newShortcut.modifiers.meta &&
        existing.modifiers.shift === newShortcut.modifiers.shift
      );
    });

    if (isDuplicate) {
      setError(t("keyboardShortcut.keyConflict"));
    } else {
      setError(null);
    }
  };

  useEffect(() => {
    if (shortcut.key) {
      checkDuplicate(shortcut.key);
    }
  }, [shortcut.modifiers]);

  if (!isOpen) return null;

  const previewShortcut = () => {
    const modifiers = [];
    if (shortcut.modifiers.alt) modifiers.push(formatModifierSymbol("alt"));
    if (shortcut.modifiers.ctrl) modifiers.push(formatModifierSymbol("ctrl"));
    if (shortcut.modifiers.meta) modifiers.push(formatModifierSymbol("meta"));
    if (shortcut.modifiers.shift) modifiers.push(formatModifierSymbol("shift"));
    if (shortcut.key) modifiers.push(formatKeyCode(shortcut.key).toUpperCase());
    return modifiers.join(" + ");
  };

  return (
    <Modal
      title={t("keyboardShortcut.editShortcut")}
      onClose={onClose}
      closeLabel={t("keyboardShortcut.cancel")}
      closeOnEscape={!isRecording}
    >
      <p className="text-sm text-base-content/70 mb-4">
        {t("keyboardShortcut.modal.description")}
      </p>
      <div className="space-y-6">
        <div className="bg-base-200 p-4 rounded-lg">
          <div className="text-sm text-base-content/70 mb-2">
            {t("keyboardShortcut.preview")}
          </div>
          <div
            className={`text-xl font-mono ${isRecording ? "text-primary" : ""}`}
          >
            {previewShortcut() || t("keyboardShortcut.pressKey")}
          </div>
        </div>

        {(error || saveError) && (
          <div role="alert" className="floorp-notice floorp-notice-error">
            <span>{error || t("ui.saveError")}</span>
          </div>
        )}

        <div className="grid grid-cols-2 gap-4">
          <label className="floorp-field-label">
            <input
              type="checkbox"
              className="floorp-checkbox"
              aria-label={formatModifierLabel("alt")} checked={shortcut.modifiers.alt}
              onChange={(e) =>
                setShortcut((prev) => ({
                  ...prev,
                  modifiers: { ...prev.modifiers, alt: e.target.checked },
                }))}
            />
            <span>{formatModifierLabel("alt")}</span>
          </label>
          <label className="floorp-field-label">
            <input
              type="checkbox"
              className="floorp-checkbox"
              aria-label={formatModifierLabel("ctrl")} checked={shortcut.modifiers.ctrl}
              onChange={(e) =>
                setShortcut((prev) => ({
                  ...prev,
                  modifiers: { ...prev.modifiers, ctrl: e.target.checked },
                }))}
            />
            <span>{formatModifierLabel("ctrl")}</span>
          </label>
          <label className="floorp-field-label">
            <input
              type="checkbox"
              className="floorp-checkbox"
              aria-label={formatModifierLabel("meta")} checked={shortcut.modifiers.meta}
              onChange={(e) =>
                setShortcut((prev) => ({
                  ...prev,
                  modifiers: { ...prev.modifiers, meta: e.target.checked },
                }))}
            />
            <span>{formatModifierLabel("meta")}</span>
          </label>
          <label className="floorp-field-label">
            <input
              type="checkbox"
              className="floorp-checkbox"
              aria-label={formatModifierLabel("shift")} checked={shortcut.modifiers.shift}
              onChange={(e) =>
                setShortcut((prev) => ({
                  ...prev,
                  modifiers: { ...prev.modifiers, shift: e.target.checked },
                }))}
            />
            <span>{formatModifierLabel("shift")}</span>
          </label>
        </div>

        <div className="floorp-field">
          <label className="floorp-field-label">
            <span className="floorp-field-text">{t("keyboardShortcut.key")}</span>
          </label>
          <div className="relative">
            <Input aria-label={t("keyboardShortcut.key")}
              type="text"
              className="w-full"
              aria-invalid={Boolean(error)}
              value={formatKeyCode(shortcut.key)}
              readOnly
              placeholder={t("keyboardShortcut.pressKey")}
              onFocus={() => setIsRecording(true)}
              onBlur={() => setIsRecording(false)}
            />
            {isRecording && (
              <div className="absolute inset-0 flex items-center justify-center bg-base-200/50 rounded-lg">
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
          disabled={!shortcut.key || !!error}
        >
          {t("keyboardShortcut.save")}
        </Button>
      </div>
    </Modal>
  );
};
