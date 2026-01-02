/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useTranslation } from "react-i18next";
import type { KeyboardShortcutConfig } from "../../../types/pref.ts";
import {
    Card,
    CardContent,
    CardDescription,
    CardHeader,
    CardTitle,
} from "@/components/common/card.tsx";
import { Switch } from "@/components/common/switch.tsx";
import { Settings } from "lucide-react";
import { InfoTip } from "@nora/ui-components";

interface GeneralSettingsProps {
    config: KeyboardShortcutConfig;
    toggleEnabled: () => void;
    updateConfig: (config: Partial<KeyboardShortcutConfig>) => void;
}

export const GeneralSettings = ({
    config,
    toggleEnabled,
}: GeneralSettingsProps) => {
    const { t } = useTranslation();

    return (
        <Card>
            <CardHeader>
                <CardTitle className="flex items-center gap-2">
                    <Settings className="size-5" />
                    {t("keyboardShortcut.basicSettings")}
                </CardTitle>
                <CardDescription>
                    {t("keyboardShortcut.basicSettingsDescription")}
                </CardDescription>
            </CardHeader>
            <CardContent className="space-y-3">
                <div>
                    <div className="mb-2 inline-flex items-center gap-2">
                        <h3 className="text-base font-medium">
                            {t("keyboardShortcut.enableOrDisable")}
                        </h3>
                        <InfoTip
                            description={t("keyboardShortcut.enableDescription")}
                        />
                    </div>
                    <div className="space-y-1">
                        <div className="flex items-center justify-between gap-2">
                            <div className="space-y-1">
                                <label htmlFor="enable-shortcuts">
                                    {t("keyboardShortcut.enable")}
                                </label>
                            </div>
                            <Switch
                                id="enable-shortcuts"
                                checked={config.enabled}
                                onChange={toggleEnabled}
                            />
                        </div>
                        <div className="text-sm text-base-content/70">
                            {t("keyboardShortcut.needRestartDescription")}
                        </div>
                    </div>
                </div>
            </CardContent>
        </Card>
    );
}; 