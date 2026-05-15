// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type { PaletteState } from "../data/state.ts";

interface StepIndicatorProps {
    state: PaletteState;
}

export function StepIndicator(props: StepIndicatorProps) {
    const cmd = () => props.state.activeCommand();
    const stepIndex = () => props.state.currentStepIndex();
    const totalSteps = () => cmd()?.steps?.length ?? 0;

    const stepLabel = () => {
        const c = cmd();
        const idx = stepIndex();
        const step = c?.steps?.[idx];
        return step?.label ?? "";
    };

    const breadcrumb = () =>
        i18next.t("commandPalette.stepIndicator", {
            defaultValue: "Step {{current}} of {{total}}",
            current: stepIndex() + 1,
            total: totalSteps(),
        });

    return (
        <div class="command-palette-step-indicator">
            <span class="command-palette-step-command-name">{cmd()?.label}</span>
            <span class="command-palette-step-breadcrumb">{breadcrumb()}</span>
            <span class="command-palette-step-label">{stepLabel()}</span>
        </div>
    );
}
