/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { useTranslation } from "react-i18next";
import { useMouseGestureConfig } from "./dataManager.ts";
import { GeneralSettings } from "./components/Preferences.tsx";
import { ActionsSettings } from "./components/ActionsSettings.tsx";
import { HelpSection } from "@/components/common/help-section.tsx";
import { LearnButton } from "@/components/common/learn-button.tsx";
import type { TutorialStep } from "@/components/common/tutorial-modal.tsx";

export default function Page() {
    const { t } = useTranslation();
    const {
        config,
        loading,
        updateConfig,
        toggleEnabled,
        addAction,
        updateAction,
        deleteAction,
        updateRockerAction,
    } = useMouseGestureConfig();

    const gestureTutorialSteps: TutorialStep[] = [
        {
            titleKey: "mouseGesture.tutorial.step1.title",
            descriptionKey: "mouseGesture.tutorial.step1.description",
        },
        {
            titleKey: "mouseGesture.tutorial.step2.title",
            descriptionKey: "mouseGesture.tutorial.step2.description",
        },
        {
            titleKey: "mouseGesture.tutorial.step3.title",
            descriptionKey: "mouseGesture.tutorial.step3.description",
        },
    ];

    if (loading) {
        return <div className="py-6 text-center">{t("loading")}...</div>;
    }

    return (
        <div className="p-6 space-y-3">
            <div className="flex flex-col items-start pl-6">
                <h1 className="text-3xl font-bold mb-2">
                    {t("pages.mouseGesture")}
                </h1>
                <p className="text-sm mb-3">
                    {t("mouseGesture.description")}
                </p>
                <div className="flex items-center gap-2 mb-4">
                    <LearnButton steps={gestureTutorialSteps} title={t("pages.mouseGesture")} />
                </div>
                <div className="w-full max-w-2xl mb-4">
                    <HelpSection summary={t("mouseGesture.helpDefaultGestures")}>
                        <p>{t("mouseGesture.helpDefaultGesturesDescription")}</p>
                    </HelpSection>
                </div>
            </div>

            <div className="space-y-3 pl-6">
                <GeneralSettings
                    config={config}
                    toggleEnabled={toggleEnabled}
                    updateConfig={updateConfig}
                    updateRockerAction={updateRockerAction}
                />

                <ActionsSettings
                    config={config}
                    addAction={addAction}
                    updateAction={updateAction}
                    deleteAction={deleteAction}
                />
            </div>
        </div>
    );
}
