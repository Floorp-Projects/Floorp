import styles from "@/components/common/settings-sections.module.css";
import { Button } from "../../../../../../libs/ui/button.tsx";
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL-2.0/. */

import { useState } from "react";
import { useTranslation } from "react-i18next";
import { comboToCanonicalCode } from "../../../../../modules/common/NRKeySyncCombo.ts";
import type { FirefoxKeyEntry } from "../../../../../modules/common/NRKeySyncTypes.ts";
import type { ShortcutConfig } from "../../../types/pref.ts";
import type { ShortcutsSettingsProps } from "../types.ts";
import { useAvailableActions } from "../../gesture/useAvailableActions.ts";
import { getKeyboardShortcutActionOptions } from "../actionCatalog.ts";
import {
  buildUnifiedShortcutRows,
  groupUnifiedShortcutRows,
  type ShortcutCategoryId,
  type UnifiedShortcutRow,
} from "../shortcutDisplay.ts";
import { useFirefoxKeys } from "../firefoxKeysManager.ts";
import { FirefoxKeyEditor } from "./FirefoxKeyEditor.tsx";
import { ShortcutEditor } from "./ShortcutEditor.tsx";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";
import { Keyboard } from "lucide-react";
import { InfoTip } from "@/components/common/infotip.tsx";
import { keySync } from "../../../lib/rpc/keysync.ts";
import { formatModifierSymbol } from "../platform.ts";

function formatKeyCode(code: string): string {
  return code.replace(/^(Key|Digit|Arrow)/, "").toUpperCase();
}

function normalizeKeyCode(code: string): string {
  if (/^[A-Z]$/.test(code)) {
    return `Key${code}`;
  }
  if (/^[0-9]$/.test(code)) {
    return `Digit${code}`;
  }
  return code;
}

function collectFloorpConflictCodes(
  shortcuts: Record<string, ShortcutConfig[]>,
): Set<string> {
  const codes = new Set<string>();
  for (const bindings of Object.values(shortcuts)) {
    for (const shortcut of bindings) {
      codes.add(
        comboToCanonicalCode(
          shortcut.modifiers,
          normalizeKeyCode(shortcut.key),
        ),
      );
    }
  }
  return codes;
}

const resetFirefoxKey = async (id: string): Promise<void> => {
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

export const ShortcutsSettings = ({
  config,
  addShortcut,
  updateShortcut,
  deleteShortcut,
}: ShortcutsSettingsProps) => {
  const { t } = useTranslation();
  const availableActions = useAvailableActions();
  const { keys: firefoxKeys, writeEnabled, loading: firefoxKeysLoading } =
    useFirefoxKeys();
  const actions = getKeyboardShortcutActionOptions(
    (key, fallback) => t(key, fallback),
    availableActions,
  );
  const { rows, hiddenActionCount } = buildUnifiedShortcutRows(
    actions,
    config.shortcuts ?? {},
    firefoxKeys,
  );
  const categories = groupUnifiedShortcutRows(rows);
  const [editingAction, setEditingAction] = useState<string | null>(null);
  const [isEditorOpen, setIsEditorOpen] = useState(false);
  const [editingShortcut, setEditingShortcut] = useState<ShortcutConfig | null>(
    null,
  );
  const [editingIndex, setEditingIndex] = useState<number | null>(null);
  const [editingEntry, setEditingEntry] = useState<FirefoxKeyEntry | null>(
    null,
  );
  const floorpConflictCodes = collectFloorpConflictCodes(
    config.shortcuts ?? {},
  );

  const handleSaveShortcut = async (shortcut: ShortcutConfig) => {
    if (!editingAction || editingIndex === null) return false;
    const saved = editingIndex < 0
      ? await addShortcut(editingAction, shortcut)
      : await updateShortcut(editingAction, editingIndex, shortcut);
    if (!saved) return false;
    setEditingShortcut(null);
    setEditingAction(null);
    setEditingIndex(null);
    return true;
  };

  const renderShortcutText = (shortcut: ShortcutConfig) => (
    <div className="flex items-center space-x-2">
      {shortcut.modifiers.alt && <span>{formatModifierSymbol("alt")}</span>}
      {shortcut.modifiers.ctrl && <span>{formatModifierSymbol("ctrl")}</span>}
      {shortcut.modifiers.meta && <span>{formatModifierSymbol("meta")}</span>}
      {shortcut.modifiers.shift && <span>{formatModifierSymbol("shift")}</span>}
      <span>{formatKeyCode(shortcut.key)}</span>
    </div>
  );

  const renderFloorpCell = (row: UnifiedShortcutRow) => {
    if (!row.shortcut) {
      return (
        <span className="text-base-content/50">
          {row.isAddRow
            ? t("keyboardShortcut.addAnotherShortcut")
            : t("keyboardShortcut.notSet")}
        </span>
      );
    }
    return renderShortcutText(row.shortcut);
  };

  const renderFirefoxCell = (row: UnifiedShortcutRow) => {
    const entry = row.firefoxKey;
    if (!entry) return null;

    return (
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
        {entry.canonicalCode !== null &&
          floorpConflictCodes.has(entry.canonicalCode) && (
          <span
            className="badge badge-warning badge-sm"
            title={t(
              "keyboardShortcut.firefoxKeyFloorpConflictHint",
            )}
          >
            {t("keyboardShortcut.firefoxKeyFloorpConflict")}
          </span>
        )}
      </div>
    );
  };

  const renderRow = (row: UnifiedShortcutRow) => {
    const isFloorp = row.source === "floorp";
    const entry = row.firefoxKey;
    const canReset = entry?.customized && !entry.internal &&
      entry.keysetKind === "main" && writeEnabled;

    return (
      <tr key={row.key}>
        <td>{row.name}</td>
        <td>
          <span className="badge badge-ghost badge-sm">
            {t(`keyboardShortcut.source.${row.source}`)}
          </span>
        </td>
        <td>{isFloorp ? renderFloorpCell(row) : renderFirefoxCell(row)}</td>
        <td>
          <div className={styles.actions}>
            {isFloorp && (
              <>
                <Button
                  type="button"
                  variant="primary"
                  onClick={() => {
                    setEditingShortcut(row.shortcut ?? null);
                    setEditingAction(row.actionId);
                    setEditingIndex(row.shortcutIndex ?? -1);
                    setIsEditorOpen(true);
                  }}
                >
                  {row.shortcut
                    ? t("keyboardShortcut.edit")
                    : t("keyboardShortcut.add")}
                </Button>
                {row.shortcut && (
                  <Button
                    type="button"
                    variant="danger"
                    onClick={() => {
                      if (
                        row.actionId !== null &&
                        row.shortcutIndex !== undefined
                      ) {
                        void deleteShortcut(row.actionId, row.shortcutIndex);
                      }
                    }}
                  >
                    {t("keyboardShortcut.delete")}
                  </Button>
                )}
              </>
            )}
            {entry && entry.keysetKind === "main" && (
              <Button
                type="button"
                variant="primary"
                disabled={!writeEnabled || entry.internal}
                onClick={() => setEditingEntry(entry)}
              >
                {t("keyboardShortcut.edit")}
              </Button>
            )}
            {entry && canReset && (
              <Button
                type="button"
                variant="danger"
                onClick={() => void resetFirefoxKey(entry.id)}
              >
                {t("keyboardShortcut.firefoxKeyReset")}
              </Button>
            )}
          </div>
        </td>
      </tr>
    );
  };

  const renderCategory = (category: {
    id: ShortcutCategoryId;
    actions: UnifiedShortcutRow[];
  }) => (
    <tbody key={category.id}>
      <tr>
        <th colSpan={4}>
          {t(`keyboardShortcut.categories.${category.id}`)}
        </th>
      </tr>
      {category.actions.map(renderRow)}
    </tbody>
  );

  return (
    <Card className={styles.section}>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Keyboard className="size-5" />
          {t("keyboardShortcut.shortcuts")}
        </CardTitle>
        <CardDescription className={styles.description}>
          <span className="inline-flex items-center gap-2">
            {t("keyboardShortcut.shortcutsDescription")}
            <InfoTip
              description={t("keyboardShortcut.shortcutsTip")}
            />
          </span>
        </CardDescription>
      </CardHeader>
      <CardContent>
        {!firefoxKeysLoading && !writeEnabled && (
          <div className="floorp-notice mb-4 text-base-content/70">
            <span className="text-sm">
              {t("keyboardShortcut.firefoxKeysReadOnly")}
            </span>
          </div>
        )}
        {!firefoxKeysLoading && hiddenActionCount > 0 && (
          <div className="floorp-notice mb-4 text-base-content/70">
            <span className="text-sm">
              {t("keyboardShortcut.hiddenByFirefox", {
                replacementCount: hiddenActionCount,
              })}
            </span>
          </div>
        )}
        <div className="overflow-x-auto">
          <table className={`floorp-table ${styles.table}`}>
            <thead>
              <tr>
                <th>{t("keyboardShortcut.action")}</th>
                <th>{t("keyboardShortcut.source.title")}</th>
                <th>{t("keyboardShortcut.shortcut")}</th>
                <th>{t("keyboardShortcut.actions")}</th>
              </tr>
            </thead>
            {categories.map(renderCategory)}
          </table>
        </div>

        {isEditorOpen && (
          <ShortcutEditor
            isOpen={isEditorOpen}
            onClose={() => setIsEditorOpen(false)}
            onSave={handleSaveShortcut}
            initialShortcut={editingShortcut}
            existingShortcuts={Object.values(config.shortcuts).flat()}
            actionId={editingAction || ""}
          />
        )}
        {editingEntry && (
          <FirefoxKeyEditor
            entry={editingEntry}
            onClose={() => setEditingEntry(null)}
          />
        )}
      </CardContent>
    </Card>
  );
};
