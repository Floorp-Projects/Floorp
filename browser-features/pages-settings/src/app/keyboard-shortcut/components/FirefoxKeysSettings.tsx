/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import styles from "@/components/common/settings-sections.module.css";
import { Button } from "../../../../../../libs/ui/button.tsx";
import { useState } from "react";
import { useTranslation } from "react-i18next";
import { comboToCanonicalCode } from "../../../../../modules/common/NRKeySyncCombo.ts";
import type { FirefoxKeyEntry } from "../../../../../modules/common/NRKeySyncTypes.ts";
import type { ShortcutConfig } from "../../../types/pref.ts";
import { keySync } from "../../../lib/rpc/keysync.ts";
import { useKeyboardShortcutConfig } from "../dataManager.ts";
import { useFirefoxKeys } from "../firefoxKeysManager.ts";
import { FirefoxKeyEditor } from "./FirefoxKeyEditor.tsx";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Keyboard } from "lucide-react";

/**
 * Normalize a Floorp shortcut key into the `event.code` space so it can be
 * compared against Firefox key canonical codes. Mirrors the local helper in
 * ShortcutEditor.tsx (kept separate to avoid changing its existing logic).
 */
function normalizeKeyCode(code: string): string {
  if (/^[A-Z]$/.test(code)) {
    return `Key${code}`;
  }
  if (/^[0-9]$/.test(code)) {
    return `Digit${code}`;
  }
  return code;
}

/** Canonical codes of every Floorp shortcut, used for conflict detection. */
function collectFloorpConflictCodes(
  shortcuts: Record<string, ShortcutConfig>,
): Set<string> {
  const codes = new Set<string>();
  for (const shortcut of Object.values(shortcuts)) {
    codes.add(
      comboToCanonicalCode(shortcut.modifiers, normalizeKeyCode(shortcut.key)),
    );
  }
  return codes;
}

const resetKey = async (id: string): Promise<void> => {
  try {
    const result = await keySync.resetKey(id);
    if (!result.ok) {
      console.error(
        "[KeyboardShortcut] Failed to reset Firefox key",
        id,
        result.error,
      );
    }
  } catch (error) {
    console.error("[KeyboardShortcut] Failed to reset Firefox key", id, error);
  }
};

export const FirefoxKeysSettings = () => {
  const { t } = useTranslation();
  const { keys, writeEnabled, loading } = useFirefoxKeys();
  const { config } = useKeyboardShortcutConfig();
  const [editingEntry, setEditingEntry] = useState<FirefoxKeyEntry | null>(
    null,
  );
  // The config hook starts with a partial object before its first load.
  const floorpConflictCodes = collectFloorpConflictCodes(
    config.shortcuts ?? {},
  );

  const mainKeys = keys.filter((entry) => entry.keysetKind === "main");
  const extensionKeys = keys.filter((entry) =>
    entry.keysetKind === "extension"
  );

  const renderRow = (entry: FirefoxKeyEntry) => {
    const canReset = entry.customized && !entry.internal &&
      entry.keysetKind === "main" && writeEnabled;
    const hasFloorpConflict = entry.canonicalCode !== null &&
      floorpConflictCodes.has(entry.canonicalCode);

    return (
      <tr key={entry.id}>
        <td>
          <span className="inline-flex items-center gap-2">
            {entry.label}
            {entry.internal && (
              <span className="badge badge-ghost badge-sm">
                {t("keyboardShortcut.firefoxKeyInternal")}
              </span>
            )}
          </span>
        </td>
        <td>
          <div className="flex flex-wrap items-center gap-2">
            {entry.shortcutText
              ? <span className="font-mono">{entry.shortcutText}</span>
              : (
                <span className="text-base-content/50">
                  {t("keyboardShortcut.notSet")}
                </span>
              )}
            {entry.customized && (
              <span className="badge badge-info badge-outline badge-sm">
                {t("keyboardShortcut.firefoxKeyCustomized")}
              </span>
            )}
            {hasFloorpConflict && (
              <span
                className="badge badge-warning badge-sm"
                title={t("keyboardShortcut.firefoxKeyFloorpConflictHint")}
              >
                {t("keyboardShortcut.firefoxKeyFloorpConflict")}
              </span>
            )}
          </div>
        </td>
        <td>
          {entry.keysetKind === "main" && (
            <div className={styles.actions}>
              <Button
                type="button"
                variant="primary"
                disabled={!writeEnabled || entry.internal}
                onClick={() => setEditingEntry(entry)}
              >
                {t("keyboardShortcut.edit")}
              </Button>
              {canReset && (
                <Button
                  type="button"
                  variant="danger"
                  onClick={() => void resetKey(entry.id)}
                >
                  {t("keyboardShortcut.firefoxKeyReset")}
                </Button>
              )}
            </div>
          )}
        </td>
      </tr>
    );
  };

  const renderGroup = (
    heading: string,
    note: string | null,
    groupKeys: FirefoxKeyEntry[],
  ) => {
    if (groupKeys.length === 0) {
      return null;
    }
    return (
      <div className="mt-6 first:mt-0">
        <h3 className="text-sm font-semibold mb-2">{heading}</h3>
        {note && <p className="text-xs text-base-content/60 mb-2">{note}</p>}
        <div className="overflow-x-auto">
          <table className={`floorp-table ${styles.table}`}>
            <thead>
              <tr>
                <th>{t("keyboardShortcut.action")}</th>
                <th>{t("keyboardShortcut.shortcut")}</th>
                <th>{t("keyboardShortcut.actions")}</th>
              </tr>
            </thead>
            <tbody>
              {groupKeys.map((entry) => renderRow(entry))}
            </tbody>
          </table>
        </div>
      </div>
    );
  };

  return (
    <Card className={styles.section}>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Keyboard className="size-5" />
          {t("keyboardShortcut.firefoxKeysTitle")}
        </CardTitle>
        <CardDescription className={styles.description}>
          <span className="inline-flex items-center gap-2">
            {t("keyboardShortcut.firefoxKeysDescription")}
            <Button
              type="button"
              variant="primary"
              onClick={() => void keySync.openAboutKeyboard()}
            >
              {t("keyboardShortcut.openAboutKeyboard")}
            </Button>
          </span>
        </CardDescription>
      </CardHeader>
      <CardContent>
        {!loading && !writeEnabled && (
          <div className="floorp-notice mb-4 text-base-content/70">
            <span className="text-sm">
              {t("keyboardShortcut.firefoxKeysReadOnly")}
            </span>
          </div>
        )}

        {loading
          ? <div className="py-6 text-center">{t("loading")}...</div>
          : (
            <>
              {renderGroup(
                t("keyboardShortcut.firefoxKeysMainGroup"),
                null,
                mainKeys,
              )}
              {renderGroup(
                t("keyboardShortcut.firefoxKeysExtensionGroup"),
                t("keyboardShortcut.firefoxKeyInternal"),
                extensionKeys,
              )}
            </>
          )}
      </CardContent>
      {editingEntry && (
        <FirefoxKeyEditor
          entry={editingEntry}
          onClose={() => setEditingEntry(null)}
        />
      )}
    </Card>
  );
};
