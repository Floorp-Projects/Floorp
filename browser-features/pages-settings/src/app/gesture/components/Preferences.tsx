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

  const handleSensitivityChange = async (
    e: React.ChangeEvent<HTMLInputElement>,
  ) => {
    const target = e.target;
    const value = Number.parseInt(target.value);
    await updateConfig({ sensitivity: value });
  };

  const handleTrailWidthChange = async (
    e: React.ChangeEvent<HTMLInputElement>,
  ) => {
    const target = e.target;
    const value = Number.parseInt(target.value);
    await updateConfig({ trailWidth: value });
  };

  const handleTrailColorChange = async (
    e: React.ChangeEvent<HTMLInputElement>,
  ) => {
    await updateConfig({ trailColor: e.target.value });
  };

  const handleMinDistanceChange = async (
    e: React.ChangeEvent<HTMLInputElement>,
  ) => {
    const target = e.target;
    const value = Number.parseInt(target.value);
    await updateConfig((current) => ({
      contextMenu: {
        ...current.contextMenu,
        minDistance: value,
      },
    }));
  };

  const handlePreventionTimeoutChange = async (
    e: React.ChangeEvent<HTMLInputElement>,
  ) => {
    const target = e.target;
    const value = Number.parseInt(target.value);
    await updateConfig((current) => ({
      contextMenu: {
        ...current.contextMenu,
        preventionTimeout: value,
      },
    }));
  };

  return (
    <Card>
      <CardHeader>
        <CardTitle className="flex items-center gap-2">
          <Settings className="size-5" />
          {t("mouseGesture.generalSettings")}
        </CardTitle>
        <CardDescription>
          {t("mouseGesture.generalSettingsDescription")}
        </CardDescription>
      </CardHeader>
      <CardContent>
        <div className="space-y-6">
          {/* 基本設定 */}
          <div className="space-y-3">
            <div className="flex items-center justify-between py-2">
              <div>
                <span className="text-base-content/90 font-medium">
                  {t("mouseGesture.enabled")}
                </span>
                <p className="text-sm text-base-content/60">
                  {t("mouseGesture.enabledDescription")}
                </p>
              </div>
              <Switch
                data-setting="mouse-gesture-enabled"
                checked={config.enabled}
                onChange={() => toggleEnabled()}
              />
            </div>

            <div className="flex items-center justify-between py-2">
              <div>
                <span className="text-base-content/90 font-medium">
                  {t("mouseGesture.rockerGesturesEnabled")}
                </span>
                <p className="text-sm text-base-content/60">
                  {t("mouseGesture.rockerGesturesDescription")}
                </p>
              </div>
              <Switch
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
                <div className="flex items-center justify-between py-2 pl-4">
                  <div className="flex-1">
                    <span className="text-base-content/90 font-medium">
                      {t("mouseGesture.rockerLeftRight")}
                    </span>
                    <p className="text-sm text-base-content/60">
                      {t("mouseGesture.rockerLeftRightDescription")}
                    </p>
                  </div>
                  <select
                    className="select select-bordered w-64 max-w-xs text-sm"
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
                  </select>
                </div>

                <div className="flex items-center justify-between py-2 pl-4">
                  <div className="flex-1">
                    <span className="text-base-content/90 font-medium">
                      {t("mouseGesture.rockerRightLeft")}
                    </span>
                    <p className="text-sm text-base-content/60">
                      {t("mouseGesture.rockerRightLeftDescription")}
                    </p>
                  </div>
                  <select
                    className="select select-bordered w-64 max-w-xs text-sm"
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
                  </select>
                </div>
              </>
            )}

            <div className="flex items-center justify-between py-2">
              <span className="text-base-content/90 font-medium">
                {t("mouseGesture.wheelGesturesEnabled")}
              </span>
              <Switch
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
                <div className="flex items-center justify-between py-2 pl-4">
                  <div className="flex-1">
                    <span className="text-base-content/90 font-medium">
                      {t("mouseGesture.wheelScrollUp")}
                    </span>
                    <p className="text-sm text-base-content/60">
                      {t("mouseGesture.wheelScrollUpDescription")}
                    </p>
                  </div>
                  <select
                    className="select select-bordered w-64 max-w-xs text-sm"
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
                  </select>
                </div>

                <div className="flex items-center justify-between py-2 pl-4">
                  <div className="flex-1">
                    <span className="text-base-content/90 font-medium">
                      {t("mouseGesture.wheelScrollDown")}
                    </span>
                    <p className="text-sm text-base-content/60">
                      {t("mouseGesture.wheelScrollDownDescription")}
                    </p>
                  </div>
                  <select
                    className="select select-bordered w-64 max-w-xs text-sm"
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
                  </select>
                </div>
              </>
            )}
          </div>

          <div className="divider" />

          {/* 感度設定 */}
          <div className="space-y-3">
            <h3 className="text-base font-semibold text-base-content">
              {t("mouseGesture.sensitivitySettings")}
            </h3>
            <p className="text-sm text-base-content/60 mb-4">
              {t("mouseGesture.sensitivitySettingsDescription")}
            </p>

            <Seekbar
              label={t("mouseGesture.sensitivity")}
              description={t("mouseGesture.sensitivityDescription")}
              min={1}
              max={100}
              value={config.sensitivity}
              onChange={handleSensitivityChange}
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
              onChange={handleMinDistanceChange}
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
              onChange={handlePreventionTimeoutChange}
              disabled={!config.enabled}
              valueSuffix="ms"
              minLabel="0ms"
              maxLabel="1000ms"
            />
          </div>

          <div className="divider" />

          {/* 軌跡設定 */}
          <div className="space-y-3">
            <h3 className="text-base font-semibold text-base-content">
              {t("mouseGesture.trailSettings")}
            </h3>
            <p className="text-sm text-base-content/60 mb-4">
              {t("mouseGesture.trailSettingsDescription")}
            </p>

            <div className="flex items-center justify-between py-2">
              <div>
                <span className="text-base-content/90 font-medium">
                  {t("mouseGesture.showTrail")}
                </span>
                <p className="text-sm text-base-content/60">
                  {t("mouseGesture.showTrailDescription")}
                </p>
              </div>
              <Switch
                checked={config.showTrail}
                onChange={() => toggleShowTrail()}
                disabled={!config.enabled}
              />
            </div>

            <div className="flex items-center justify-between py-2">
              <div>
                <span className="text-base-content/90 font-medium">
                  {t("mouseGesture.showLabel")}
                </span>
                <p className="text-sm text-base-content/60">
                  {t("mouseGesture.showLabelDescription")}
                </p>
              </div>
              <Switch
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
              onChange={handleTrailWidthChange}
              disabled={!config.enabled || !config.showTrail}
              valueSuffix="px"
              minLabel="1px"
              maxLabel="10px"
            />

            <div className="form-control flex items-center gap-2 flex-initial justify-between">
              <label className="mb-2">
                <span className="text-base-content/90">
                  {t("mouseGesture.trailColor")}
                </span>
              </label>
              <div className="flex items-center gap-2">
                <div className="relative w-10 h-10 rounded border border-base-300 overflow-hidden cursor-pointer">
                  <input
                    type="color"
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
                <input
                  type="text"
                  value={config.trailColor}
                  onChange={handleTrailColorChange}
                  className="input input-bordered w-full max-w-xs"
                  placeholder="#000000"
                  disabled={!config.enabled || !config.showTrail}
                />
              </div>
            </div>
          </div>
        </div>
      </CardContent>
    </Card>
  );
}
