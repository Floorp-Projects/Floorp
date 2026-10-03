import styles from "@/components/common/settings-sections.module.css";
import { Select } from "../../../../../../libs/ui/dropdown.tsx";
import { Input } from "../../../../../../libs/ui/input.tsx";
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type React from "react";
import { useTranslation } from "react-i18next";
import { Settings } from "lucide-react";
import { Switch } from "@/components/common/switch.tsx";
import { Seekbar } from "@/components/common/seekbar.tsx";
import type { MouseGestureConfig } from "@/types/pref.ts";
import { useAvailableActions } from "../useAvailableActions.ts";
import type { MouseGestureConfigUpdate } from "../configPersistence.ts";
import { REPEAT_SAFE_WHEEL_ACTIONS } from "#features-chrome/common/mouse-gesture/wheel-action-policy.ts";
import {
  Card,
  CardContent,
  CardDescription,
  CardHeader,
  CardTitle,
} from "@/components/common/card.tsx";

interface GeneralSettingsProps {
  config: MouseGestureConfig;
  toggleEnabled: () => Promise<boolean>;
  updateConfig: (update: MouseGestureConfigUpdate) => Promise<boolean>;
  updateRockerAction: (
    rockerType: "leftRight" | "rightLeft",
    action: string,
  ) => Promise<boolean>;
  updateWheelAction: (
    wheelType: "scrollUp" | "scrollDown",
    action: string,
  ) => Promise<boolean>;
}

export function GeneralSettings({
  config,
  toggleEnabled,
  updateConfig,
  updateRockerAction,
  updateWheelAction,
}: GeneralSettingsProps) {
  const { t } = useTranslation();
  const availableActions = useAvailableActions();
  const wheelActions = useAvailableActions(REPEAT_SAFE_WHEEL_ACTIONS);

  const toggleShowTrail = async () => {
    await updateConfig((current) => ({ showTrail: !current.showTrail }));
  };

  const toggleShowLabel = async () => {
    await updateConfig((current) => ({
      showLabel: !(current.showLabel ?? true),
    }));
  };

  const handleSensitivityChange = async (value: number) => {
    await updateConfig({ sensitivity: value });
  };

  const handleTrailWidthChange = async (value: number) => {
    await updateConfig({ trailWidth: value });
  };

  const handleTrailColorChange = async (
    e: React.ChangeEvent<HTMLInputElement>,
  ) => {
    await updateConfig({ trailColor: e.target.value });
  };

  const handleMinDistanceChange = async (value: number) => {
    await updateConfig((current) => ({
      contextMenu: {
        ...current.contextMenu,
        minDistance: value,
      },
    }));
  };

  const handlePreventionTimeoutChange = async (value: number) => {
    await updateConfig((current) => ({
      contextMenu: {
        ...current.contextMenu,
        preventionTimeout: value,
      },
    }));
  };

  return (
    <>
      <Card className={styles.section}>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Settings className="size-5" />
            {t("mouseGesture.generalSettings")}
          </CardTitle>
          <CardDescription className={styles.description}>
            {t("mouseGesture.generalSettingsDescription")}
          </CardDescription>
        </CardHeader>
        <CardContent>
          {/* 基本設定 */}
          <div className="space-y-3">
            <div className={styles.row}>
              <div>
                <span className="text-base-content/90 font-medium">
                  {t("mouseGesture.enabled")}
                </span>
                <p className="text-sm text-base-content/60">
                  {t("mouseGesture.enabledDescription")}
                </p>
              </div>
              <Switch
                aria-label={t("mouseGesture.enabled")}
                data-setting="mouse-gesture-enabled"
                checked={config.enabled}
                onChange={() => toggleEnabled()}
              />
            </div>

            <div className={styles.row}>
              <div>
                <span className="text-base-content/90 font-medium">
                  {t("mouseGesture.rockerGesturesEnabled")}
                </span>
                <p className="text-sm text-base-content/60">
                  {t("mouseGesture.rockerGesturesDescription")}
                </p>
              </div>
              <Switch
                aria-label={t("mouseGesture.rockerGesturesEnabled")}
                checked={config.rockerGesturesEnabled ?? true}
                onChange={() =>
                  updateConfig((current) => ({
                    rockerGesturesEnabled:
                      !(current.rockerGesturesEnabled ?? true),
                  }))}
                disabled={!config.enabled}
              />
            </div>

            {/* Rocker Actions - only show when enabled */}
            {config.rockerGesturesEnabled && (
              <>
                <div className={styles.field}>
                  <div className="flex-1">
                    <span className="text-base-content/90 font-medium">
                      {t("mouseGesture.rockerLeftRight")}
                    </span>
                    <p className="text-sm text-base-content/60">
                      {t("mouseGesture.rockerLeftRightDescription")}
                    </p>
                  </div>
                  <Select
                    className="w-64 max-w-xs text-sm"
                    aria-label={t("mouseGesture.rockerLeftRight")}
                    value={config.rockerActions.leftRight}
                    onChange={(e) =>
                      updateRockerAction("leftRight", e.target.value)}
                    disabled={!config.enabled}
                  >
                    {availableActions.map((action) => (
                      <option key={action.id} value={action.id}>
                        {action.name}
                      </option>
                    ))}
                  </Select>
                </div>

                <div className={styles.field}>
                  <div className="flex-1">
                    <span className="text-base-content/90 font-medium">
                      {t("mouseGesture.rockerRightLeft")}
                    </span>
                    <p className="text-sm text-base-content/60">
                      {t("mouseGesture.rockerRightLeftDescription")}
                    </p>
                  </div>
                  <Select
                    className="w-64 max-w-xs text-sm"
                    aria-label={t("mouseGesture.rockerRightLeft")}
                    value={config.rockerActions.rightLeft}
                    onChange={(e) =>
                      updateRockerAction("rightLeft", e.target.value)}
                    disabled={!config.enabled}
                  >
                    {availableActions.map((action) => (
                      <option key={action.id} value={action.id}>
                        {action.name}
                      </option>
                    ))}
                  </Select>
                </div>
              </>
            )}

            <div className={styles.row}>
              <span className="text-base-content/90 font-medium">
                {t("mouseGesture.wheelGesturesEnabled")}
              </span>
              <Switch
                aria-label={t("mouseGesture.wheelGesturesEnabled")}
                data-setting="mouse-gesture-wheel-enabled"
                checked={config.wheelGesturesEnabled ?? true}
                onChange={() =>
                  updateConfig((current) => ({
                    wheelGesturesEnabled:
                      !(current.wheelGesturesEnabled ?? true),
                  }))}
                disabled={!config.enabled}
              />
            </div>

            {/* Wheel Actions - only show when enabled */}
            {config.wheelGesturesEnabled && (
              <>
                <div className={styles.field}>
                  <div className="flex-1">
                    <span className="text-base-content/90 font-medium">
                      {t("mouseGesture.wheelScrollUp")}
                    </span>
                    <p className="text-sm text-base-content/60">
                      {t("mouseGesture.wheelScrollUpDescription")}
                    </p>
                  </div>
                  <Select
                    className="w-64 max-w-xs text-sm"
                    aria-label={t("mouseGesture.wheelScrollUp")}
                    value={config.wheelActions.scrollUp}
                    onChange={(e) =>
                      updateWheelAction("scrollUp", e.target.value)}
                    disabled={!config.enabled}
                  >
                    {wheelActions.map((action) => (
                      <option key={action.id} value={action.id}>
                        {action.name}
                      </option>
                    ))}
                  </Select>
                </div>

                <div className={styles.field}>
                  <div className="flex-1">
                    <span className="text-base-content/90 font-medium">
                      {t("mouseGesture.wheelScrollDown")}
                    </span>
                    <p className="text-sm text-base-content/60">
                      {t("mouseGesture.wheelScrollDownDescription")}
                    </p>
                  </div>
                  <Select
                    className="w-64 max-w-xs text-sm"
                    aria-label={t("mouseGesture.wheelScrollDown")}
                    value={config.wheelActions.scrollDown}
                    onChange={(e) =>
                      updateWheelAction("scrollDown", e.target.value)}
                    disabled={!config.enabled}
                  >
                    {wheelActions.map((action) => (
                      <option key={action.id} value={action.id}>
                        {action.name}
                      </option>
                    ))}
                  </Select>
                </div>
              </>
            )}
          </div>
        </CardContent>
      </Card>
      <Card className={styles.section}>
        <CardHeader>
          <CardTitle>{t("mouseGesture.sensitivitySettings")}</CardTitle>
          <CardDescription className={styles.description}>
            {t("mouseGesture.sensitivitySettingsDescription")}
          </CardDescription>
        </CardHeader>
        <CardContent>
          <div className="space-y-6">
            <Seekbar
              label={t("mouseGesture.sensitivity")}
              description={t("mouseGesture.sensitivityDescription")}
              min={1}
              max={100}
              value={config.sensitivity}
              onValueChange={handleSensitivityChange}
              disabled={!config.enabled}
              minLabel={t("mouseGesture.low")}
              maxLabel={t("mouseGesture.high")}
            />

            <Seekbar
              label={t("mouseGesture.minDistance")}
              description={t("mouseGesture.minDistanceDescription")}
              min={5}
              max={50}
              value={config.contextMenu.minDistance}
              onValueChange={handleMinDistanceChange}
              disabled={!config.enabled}
              valueSuffix="px"
              minLabel="5px"
              maxLabel="50px"
            />

            <Seekbar
              label={t("mouseGesture.preventionTimeout")}
              description={t("mouseGesture.preventionTimeoutDescription")}
              min={0}
              max={1000}
              step={50}
              value={config.contextMenu.preventionTimeout}
              onValueChange={handlePreventionTimeoutChange}
              disabled={!config.enabled}
              valueSuffix="ms"
              minLabel="0ms"
              maxLabel="1000ms"
            />
          </div>
        </CardContent>
      </Card>
      <Card className={styles.section}>
        <CardHeader>
          <CardTitle>{t("mouseGesture.trailSettings")}</CardTitle>
          <CardDescription className={styles.description}>
            {t("mouseGesture.trailSettingsDescription")}
          </CardDescription>
        </CardHeader>
        <CardContent>
          <div className="space-y-6">
            <div className={styles.row}>
              <div>
                <span className="text-base-content/90 font-medium">
                  {t("mouseGesture.showTrail")}
                </span>
                <p className="text-sm text-base-content/60">
                  {t("mouseGesture.showTrailDescription")}
                </p>
              </div>
              <Switch
                aria-label={t("mouseGesture.showTrail")}
                checked={config.showTrail}
                onChange={() => toggleShowTrail()}
                disabled={!config.enabled}
              />
            </div>

            <div className={styles.row}>
              <div>
                <span className="text-base-content/90 font-medium">
                  {t("mouseGesture.showLabel")}
                </span>
                <p className="text-sm text-base-content/60">
                  {t("mouseGesture.showLabelDescription")}
                </p>
              </div>
              <Switch
                aria-label={t("mouseGesture.showLabel")}
                checked={config.showLabel ?? true}
                onChange={() => toggleShowLabel()}
                disabled={!config.enabled}
              />
            </div>

            <Seekbar
              label={t("mouseGesture.trailWidth")}
              description={t("mouseGesture.trailWidthDescription")}
              min={1}
              max={10}
              value={config.trailWidth}
              onValueChange={handleTrailWidthChange}
              disabled={!config.enabled || !config.showTrail}
              valueSuffix="px"
              minLabel="1px"
              maxLabel="10px"
            />

            <div className={styles.field}>
              <label className="mb-2" htmlFor="gesture-trail-color-text">
                <span className="text-base-content/90">
                  {t("mouseGesture.trailColor")}
                </span>
              </label>
              <div className="flex items-center gap-2">
                <div className="relative size-11 shrink-0 rounded border border-base-300 overflow-hidden cursor-pointer">
                  <input
                    type="color"
                    aria-label={t("mouseGesture.trailColor")}
                    value={config.trailColor}
                    onChange={handleTrailColorChange}
                    className="absolute inset-0 w-full h-full opacity-0 cursor-pointer"
                    disabled={!config.enabled || !config.showTrail}
                  />
                  <div
                    className="w-full h-full"
                    style={{ backgroundColor: config.trailColor }}
                  >
                  </div>
                </div>
                <Input
                  type="text"
                  id="gesture-trail-color-text"
                  value={config.trailColor}
                  onChange={handleTrailColorChange}
                  className="w-full max-w-xs"
                  placeholder="#000000"
                  disabled={!config.enabled || !config.showTrail}
                />
              </div>
            </div>
          </div>
        </CardContent>
      </Card>
    </>
  );
}
