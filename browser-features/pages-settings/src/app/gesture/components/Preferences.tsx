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
import {
    Card,
    CardContent,
    CardHeader,
    CardTitle,
    CardDescription,
} from "@/components/common/card.tsx";

interface GeneralSettingsProps {
    config: MouseGestureConfig;
    toggleEnabled: () => Promise<boolean>;
    updateConfig: (config: Partial<MouseGestureConfig>) => Promise<boolean>;
}

export function GeneralSettings({
    config,
    toggleEnabled,
    updateConfig,
}: GeneralSettingsProps) {
    const { t } = useTranslation();

    const toggleShowTrail = async () => {
        await updateConfig({ showTrail: !config.showTrail });
    };

    const toggleShowLabel = async () => {
        const current = config.showLabel ?? true;
        await updateConfig({ showLabel: !current });
    };

    const handleSensitivityChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
        const target = e.target;
        const value = Number.parseInt(target.value);
        await updateConfig({ sensitivity: value });
    };

    const handleTrailWidthChange = async (e: React.ChangeEvent<HTMLInputElement>) => {
        const target = e.target;
        const value = Number.parseInt(target.value);
        await updateConfig({ trailWidth: value });
    };

    const handleTrailColorChange = async (
        e: React.ChangeEvent<HTMLInputElement>,
    ) => {
        await updateConfig({ trailColor: e.target.value });
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
                <div className="space-y-4">
                    <div className="flex items-center justify-between py-2">
                        <span className="text-base-content/90">
                            {t("mouseGesture.enabled")}
                        </span>
                        <Switch checked={config.enabled} onChange={() => toggleEnabled()} />
                    </div>

                    <div className="flex items-center justify-between py-2">
                        <span className="text-base-content/90">
                            {t("mouseGesture.rockerGesturesEnabled", "Enable Rocker Gestures")}
                        </span>
                        <Switch
                            checked={config.rockerGesturesEnabled ?? true}
                            onChange={() => updateConfig({ rockerGesturesEnabled: !(config.rockerGesturesEnabled ?? true) })}
                            disabled={!config.enabled}
                        />
                    </div>

                    <div className="flex items-center justify-between py-2">
                        <span className="text-base-content/90">
                            {t("mouseGesture.showTrail")}
                        </span>
                        <Switch
                            checked={config.showTrail}
                            onChange={() => toggleShowTrail()}
                            disabled={!config.enabled}
                        />
                    </div>

                    <div className="flex items-center justify-between py-2">
                        <span className="text-base-content/90">
                            {t("mouseGesture.showLabel")}
                        </span>
                        <Switch
                            checked={(config.showLabel ?? true)}
                            onChange={() => toggleShowLabel()}
                            disabled={!config.enabled}
                        />
                    </div>

                    <Seekbar
                        label={t("mouseGesture.sensitivity")}
                        min={1}
                        max={100}
                        value={config.sensitivity}
                        onChange={handleSensitivityChange}
                        disabled={!config.enabled}
                        minLabel={t("mouseGesture.low")}
                        maxLabel={t("mouseGesture.high")}
                    />

                    <Seekbar
                        label={t("mouseGesture.trailWidth")}
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
                                ></div>
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
            </CardContent>
        </Card>
    );
}
