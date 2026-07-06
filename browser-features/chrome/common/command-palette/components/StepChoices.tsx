// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import type { CommandStepChoice } from "../types.ts";
import type { PaletteState } from "../data/state.ts";

interface StepChoicesProps {
    state: PaletteState;
    onSelect: (choice: CommandStepChoice) => void;
}

export function StepChoices(props: StepChoicesProps) {
    const choices = props.state.filteredStepChoices();
    const selectedIndex = props.state.selectedChoiceIndex();
    const isLoading = props.state.stepChoicesLoading();

    if (choices.length === 0 && !isLoading) {
        return null;
    }

    return (
        <div class="command-palette-step-choices" role="listbox">
            {isLoading && (
                <div class="command-palette-step-choices-loading">
                    {i18next.t("commandPalette.loadingChoices", {
                        defaultValue: "Loading options...",
                    })}
                </div>
            )}
            {!isLoading && choices.map((choice, index) => (
                <div
                    class="command-palette-step-choice-item"
                    data-selected={selectedIndex === index ? "true" : undefined}
                    onClick={() => props.onSelect(choice)}
                    onMouseEnter={() => props.state.setSelectedChoiceIndex(index)}
                    role="option"
                    aria-selected={selectedIndex === index}
                    tabIndex={-1}
                >
                    <div class="command-palette-step-choice-info">
                        <span class="command-palette-step-choice-label">{choice.label}</span>
                        {choice.description && (
                            <span class="command-palette-step-choice-description">{choice.description}</span>
                        )}
                    </div>
                </div>
            ))}
        </div>
    );
}
