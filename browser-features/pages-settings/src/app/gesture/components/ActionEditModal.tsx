import { Select } from "../../../../../../libs/ui/dropdown.tsx";
import { Button } from "../../../../../../libs/ui/button.tsx";
import { Modal } from "../../../../../../libs/ui/modal.tsx";
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useState } from "react";
import { useTranslation } from "react-i18next";
import type { GestureAction, GestureDirection } from "@/types/pref.ts";
import { patternToString } from "../dataManager.ts";
import { useAvailableActions } from "../useAvailableActions.ts";
import type { ActionEditModalProps } from "../types.ts";

export function ActionEditModal({
  action: initialAction,
  mode,
  onSave,
  onClose,
  isPatternDuplicate,
}: ActionEditModalProps) {
  const { t } = useTranslation();
  const [action, setAction] = useState<GestureAction>(initialAction);
  const [error, setError] = useState<string | null>(null);
  const [saving, setSaving] = useState(false);
  const availableActions = useAvailableActions();

  const updatePattern = (direction: GestureDirection) => {
    setError(null);

    if (
      action.pattern.length > 0 &&
      action.pattern[action.pattern.length - 1] === direction
    ) {
      return;
    }

    setAction({
      ...action,
      pattern: [...action.pattern, direction],
    });
  };

  const resetPattern = () => {
    setError(null);

    setAction({
      ...action,
      pattern: [],
    });
  };

  const removeDirection = (index: number) => {
    setError(null);

    const newPattern = [...action.pattern];
    newPattern.splice(index, 1);
    setAction({
      ...action,
      pattern: newPattern,
    });
  };

  const handleSave = async () => {
    if (saving) return;
    const finalAction = { ...action };

    if (finalAction.pattern.length === 0) {
      setError(t("mouseGesture.emptyPatternError"));
      return;
    }

    if (isPatternDuplicate(finalAction.pattern)) {
      setError(t("mouseGesture.duplicatePatternError"));
      return;
    }

    setSaving(true);
    setError(null);
    try {
      if (!await onSave(finalAction)) setError(t("ui.saveError"));
    } catch (error) {
      console.error("[MouseGesture] Could not save action", error);
      setError(t("ui.saveError"));
    } finally {
      setSaving(false);
    }
  };

  const getModalTitle = () => {
    switch (mode) {
      case "edit":
        return t("mouseGesture.editAction");
      case "duplicate":
        return t("mouseGesture.duplicateAction");
      default:
        return t("mouseGesture.addAction");
    }
  };

  return (
    <Modal title={getModalTitle()} onClose={onClose} closeLabel={t("mouseGesture.cancel")}>
      {error && (
        <div role="alert" className="floorp-notice floorp-notice-error mb-4 shadow-lg">
          <svg
            xmlns="http://www.w3.org/2000/svg"
            className="stroke-current flex-shrink-0 h-6 w-6"
            fill="none"
            viewBox="0 0 24 24"
          >
            <path
              strokeLinecap="round"
              strokeLinejoin="round"
              strokeWidth="2"
              d="M10 14l2-2m0 0l2-2m-2 2l-2-2m2 2l2 2m7-2a9 9 0 11-18 0 9 9 0 0118 0z"
            />
          </svg>
          <span>{error}</span>
        </div>
      )}

      <div className="floorp-field w-full">
        <label className="mb-2">
          <span className="text-base-content/90">
            {t("mouseGesture.action")}
          </span>
        </label>
        <Select aria-label={t("mouseGesture.action")}
          className="w-full"
          value={action.action}
          onChange={(e) =>
            setAction({
              ...action,
              action: e.target.value,
            })}
        >
          <option disabled value="">
            {t("mouseGesture.selectAction")}
          </option>
          {availableActions.map((availableAction) => (
            <option key={availableAction.id} value={availableAction.id}>
              {availableAction.name}
            </option>
          ))}
        </Select>
      </div>

      <div className="py-4 space-y-4">
        <div className="floorp-field w-full">
          <label className="mb-2">
            <span className="text-base-content/90">
              {t("mouseGesture.pattern")}
            </span>
          </label>
          <div className="flex flex-wrap gap-2 mb-3 min-h-8">
            {action.pattern.map((direction, index) => (
              <div
                key={index}
                className="floorp-tag font-mono gap-1"
              >
                {patternToString([direction])}
                <Button
                  type="button"
                  variant="ghost"
                  onClick={() => removeDirection(index)}
                >
                  ×
                </Button>
              </div>
            ))}
            {action.pattern.length === 0 && (
              <div className="text-sm text-base-content/60">
                {t("mouseGesture.noPattern")}
              </div>
            )}
          </div>
          <div className="space-y-2">
            <div className="grid grid-cols-3 gap-2">
              <Button
                type="button"
                variant="secondary"
                onClick={() =>
                  updatePattern("upLeft")}
              >
                ↖
              </Button>
              <Button
                type="button"
                variant="secondary"
                onClick={() =>
                  updatePattern("up")}
              >
                ↑
              </Button>
              <Button
                type="button"
                variant="secondary"
                onClick={() =>
                  updatePattern("upRight")}
              >
                ↗
              </Button>
              <Button
                type="button"
                variant="secondary"
                onClick={() => updatePattern("left")}
              >
                ←
              </Button>
              <div />
              <Button
                type="button"
                variant="secondary"
                onClick={() => updatePattern("right")}
              >
                →
              </Button>
              <Button
                type="button"
                variant="secondary"
                onClick={() => updatePattern("downLeft")}
              >
                ↙
              </Button>
              <Button
                type="button"
                variant="secondary"
                onClick={() => updatePattern("down")}
              >
                ↓
              </Button>
              <Button
                type="button"
                variant="secondary"
                onClick={() => updatePattern("downRight")}
              >
                ↘
              </Button>
            </div>
            <div>
              <Button
                type="button"
                variant="secondary"
                onClick={resetPattern}
              >
                {t("mouseGesture.reset")}
              </Button>
            </div>
          </div>
        </div>
      </div>

      <div className="floorp-form-actions">
        <Button type="button" variant="secondary" onClick={onClose}>
          {t("mouseGesture.cancel")}
        </Button>
        <Button
          type="button"
          variant="primary"
          onClick={handleSave}
          disabled={action.pattern.length === 0}
          aria-disabled={saving}
          aria-busy={saving}
        >
          {t("mouseGesture.save")}
        </Button>
      </div>
    </Modal>
  );
}
