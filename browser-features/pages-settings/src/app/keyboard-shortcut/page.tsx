/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useTranslation } from "react-i18next";
import { useKeyboardShortcutConfig } from "./dataManager.ts";
import { GeneralSettings } from "./components/GeneralSettings.tsx";
import { ShortcutsSettings } from "./components/ShortcutsSettings.tsx";

export default function Page() {
    const { t } = useTranslation();
    const {
        config,
        loading,
        updateConfig,
        toggleEnabled,
        addShortcut,
        updateShortcut,
        deleteShortcut,
    } = useKeyboardShortcutConfig();

    if (loading) {
        return <div className="py-6 text-center">{t("loading")}...</div>;
    }

    return (
        <div className="floorp-settings-page">
            <div className="floorp-page-header">
                <h1 className="floorp-page-heading">
                    {t("pages.keyboardShortcut")}
                </h1>
                <p className="floorp-page-description">
                    {t("keyboardShortcut.description")}
                </p>
            </div>

            <div className="floorp-settings-sections">
                <GeneralSettings
                    config={config}
                    toggleEnabled={toggleEnabled}
                    updateConfig={updateConfig}
                />

                <ShortcutsSettings
                    config={config}
                    addShortcut={addShortcut}
                    updateShortcut={updateShortcut}
                    deleteShortcut={deleteShortcut}
                />
            </div>
        </div>
    );
}