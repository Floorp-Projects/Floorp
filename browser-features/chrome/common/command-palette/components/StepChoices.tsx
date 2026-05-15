// SPDX-License-Identifier: MPL-2.0

import { For, Show } from "solid-js";
import i18next from "i18next";
import type { CommandStepChoice } from "../command-registry.ts";
import type { PaletteState } from "../data/state.ts";

interface StepChoicesProps {
    state: PaletteState;
    onSelect: (choice: CommandStepChoice) => void;
}

export function StepChoices(props: StepChoicesProps) {
    const choices = () => props.state.filteredStepChoices();
    const selectedIndex = () => props.state.selectedChoiceIndex();
    const isLoading = () => props.state.stepChoicesLoading();

    return (
        <Show when={choices().length > 0 || isLoading()}>
            <div class="command-palette-step-choices" role="listbox">
                <Show when={isLoading()}>
                    <div class="command-palette-step-choices-loading">
                        {i18next.t("commandPalette.loadingChoices", {
                            defaultValue: "Loading options...",
                        })}
                    </div>
                </Show>
                <Show when={!isLoading()}>
                    <For each={choices()}>
                        {(choice, index) => (
                            <div
                                class="command-palette-step-choice-item"
                                data-selected={selectedIndex() === index() ? "true" : undefined}
                                onClick={() => props.onSelect(choice)}
                                onMouseEnter={() => props.state.setSelectedChoiceIndex(index())}
                                role="option"
                                aria-selected={selectedIndex() === index()}
                                tabindex={-1}
                            >
                                <div class="command-palette-step-choice-info">
                                    <span class="command-palette-step-choice-label">{choice.label}</span>
                                    <Show when={choice.description}>
                                        {(desc) => (
                                            <span class="command-palette-step-choice-description">{desc()}</span>
                                        )}
                                    </Show>
                                </div>
                            </div>
                        )}
                    </For>
                </Show>
            </div>
        </Show>
    );
}
