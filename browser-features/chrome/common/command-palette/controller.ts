// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import { debounce } from "@solid-primitives/scheduled";
import { createPaletteState, type PaletteState } from "./data/state.ts";
import {
  addRecentCommand,
  getFrequencies,
  getRecentCommands,
  incrementFrequency,
  isEnabled,
} from "./config.ts";
import {
  getPaletteCommands,
  searchBookmarkCommands,
  searchCommands,
  searchHistoryCommands,
} from "./command-registry.ts";
import { shareModeEnabled } from "../browser-share-mode/browser-share-mode.tsx";
import type {
  CommandStep,
  CommandStepChoice,
  PaletteCommand,
  StepChoicesResult,
} from "./types.ts";
import {
  isPaletteTargetAvailable,
  resolvePaletteTarget,
} from "./utils/targetContext.ts";

function looksLikeUrl(query: string): boolean {
  if (query.startsWith("http://") || query.startsWith("https://")) return true;
  if (query.startsWith("about:") || query.startsWith("floorp://")) return true;
  // domain-like: contains a dot with text on both sides and no spaces
  if (!query.includes(" ") && /^[^\s]+\.[a-z]{2,}$/i.test(query)) return true;
  return false;
}

export class CommandPaletteController {
  private eventListenersAttached = false;
  private targetWindow: Window;
  readonly state: PaletteState = createPaletteState();

  constructor(win: Window = globalThis as unknown as Window) {
    this.targetWindow = win;
    this.init();
  }

  private init(): void {
    if (this.eventListenersAttached) return;

    this.targetWindow.addEventListener(
      "keydown",
      this.handlePaletteKeyDown,
      true, // capture phase
    );
    this.eventListenersAttached = true;
  }

  public destroy(): void {
    this.stepChoicesLoadGeneration++;
    if (this.eventListenersAttached) {
      this.targetWindow.removeEventListener(
        "keydown",
        this.handlePaletteKeyDown,
        true,
      );
      this.eventListenersAttached = false;
    }
    this.clearAnimOutTimer();
    if (this.historySearchTimer) {
      clearTimeout(this.historySearchTimer);
      this.historySearchTimer = null;
    }
    if (this.bookmarkSearchTimer) {
      clearTimeout(this.bookmarkSearchTimer);
      this.bookmarkSearchTimer = null;
    }
    if (this.state.isVisible()) {
      this.hidePalette();
    }
  }

  public togglePalette(): void {
    if (!isEnabled()) return;
    if (this.state.isAnimatingOut()) return;

    if (this.state.isVisible()) {
      this.hidePalette();
    } else {
      this.showPalette();
    }
  }

  private handlePaletteKeyDown = (event: KeyboardEvent): void => {
    if (!this.state.isVisible()) return;

    // In input mode, handle step-specific keys
    if (this.state.mode() === "input") {
      this.handleInputModeKeyDown(event);
      return;
    }

    switch (event.key) {
      case "Escape":
        event.preventDefault();
        event.stopPropagation();
        this.hidePalette();
        break;

      case "ArrowDown":
        event.preventDefault();
        event.stopPropagation();
        this.handleArrowDown();
        break;

      case "ArrowUp":
        event.preventDefault();
        event.stopPropagation();
        this.handleArrowUp();
        break;

      case "Enter":
        event.preventDefault();
        event.stopPropagation();
        this.handleEnter();
        break;

      case "Tab":
        event.preventDefault();
        event.stopPropagation();
        if (event.shiftKey) {
          this.handleArrowUp();
        } else {
          this.handleArrowDown();
        }
        this.focusSelectedItem();
        break;
    }
  };

  private currentStepHasChoices(): boolean {
    const cmd = this.state.activeCommand();
    const stepIndex = this.state.currentStepIndex();
    const step = cmd?.steps?.[stepIndex];
    return (
      (!!step?.choices && step.choices.length > 0) || !!step?.choicesLoader
    );
  }

  private updateStepChoices(query: string): void {
    const baseChoices = this.state.stepChoicesBase();
    if (baseChoices.length === 0) {
      this.state.setFilteredStepChoices([]);
      return;
    }

    const q = query.trim().toLowerCase();
    if (!q) {
      this.state.setFilteredStepChoices(baseChoices);
    } else {
      const filtered = baseChoices.filter(
        (c) =>
          c.label.toLowerCase().includes(q) ||
          c.value.toLowerCase().includes(q) ||
          (c.description?.toLowerCase().includes(q) ?? false),
      );
      this.state.setFilteredStepChoices(filtered);
    }
    this.state.setSelectedChoiceIndex(0);
  }

  private handleInputModeKeyDown(event: KeyboardEvent): void {
    const hasChoices = this.currentStepHasChoices();

    switch (event.key) {
      case "Escape":
        event.preventDefault();
        event.stopPropagation();
        // Step back through previous steps before closing the palette entirely.
        // This avoids losing partially-completed input when the user just wants
        // to go back one step.
        this.goBackStep();
        break;

      case "Enter":
        event.preventDefault();
        event.stopPropagation();
        this.advanceStep();
        break;

      case "ArrowDown":
        if (hasChoices) {
          event.preventDefault();
          event.stopPropagation();
          const choices = this.state.filteredStepChoices();
          if (choices.length > 0) {
            const idx = this.state.selectedChoiceIndex();
            const nextIdx = idx + 1;
            if (nextIdx >= choices.length) {
              // At the end of the list — try loading more
              if (this.state.hasMoreChoices() && !this.state.loadingMore()) {
                this.loadMoreChoices();
              }
              // Wrap around only if no more to load
              if (!this.state.hasMoreChoices()) {
                this.state.setSelectedChoiceIndex(0);
              }
            } else {
              this.state.setSelectedChoiceIndex(nextIdx);
            }
          }
        }
        break;

      case "ArrowUp":
        if (hasChoices) {
          event.preventDefault();
          event.stopPropagation();
          const choices = this.state.filteredStepChoices();
          if (choices.length > 0) {
            const idx = this.state.selectedChoiceIndex();
            this.state.setSelectedChoiceIndex(
              (idx - 1 + choices.length) % choices.length,
            );
          }
        }
        break;

      case "Tab":
        if (hasChoices) {
          event.preventDefault();
          event.stopPropagation();
          const choices = this.state.filteredStepChoices();
          if (choices.length > 0) {
            const idx = this.state.selectedChoiceIndex();
            if (event.shiftKey) {
              this.state.setSelectedChoiceIndex(
                (idx - 1 + choices.length) % choices.length,
              );
            } else {
              const nextIdx = idx + 1;
              if (nextIdx >= choices.length) {
                if (this.state.hasMoreChoices() && !this.state.loadingMore()) {
                  this.loadMoreChoices();
                }
                if (!this.state.hasMoreChoices()) {
                  this.state.setSelectedChoiceIndex(0);
                }
              } else {
                this.state.setSelectedChoiceIndex(nextIdx);
              }
            }
          }
        }
        break;

      case "Backspace": {
        const input = this.targetWindow.document?.getElementById(
          "command-palette-search",
        ) as HTMLInputElement | null;
        // Only go back if the input is empty
        if (input && input.value === "") {
          event.preventDefault();
          event.stopPropagation();
          this.goBackStep();
        }
        break;
      }
    }
  }

  private focusSelectedItem(): void {
    this.targetWindow.setTimeout(() => {
      const selected = this.targetWindow.document?.querySelector(
        '.command-palette-item[data-selected="true"]',
      );
      if (selected) (selected as HTMLElement).focus();
    }, 0);
  }

  private focusSearchInput(clear: boolean = true): void {
    this.targetWindow.setTimeout(() => {
      const input = this.targetWindow.document?.getElementById(
        "command-palette-search",
      ) as HTMLInputElement | null;
      if (!input) return;
      // Clear input value directly on the DOM element when in input mode
      // (SolidJS reactive value binding may not reliably update in Firefox/XUL)
      if (clear && this.state.mode() === "input") {
        input.value = "";
      }
      input.focus();
    }, 0);
  }

  private handleArrowDown(): void {
    const commands = this.state.filteredCommands();
    const idx = this.state.selectedIndex();
    if (commands.length > 0) {
      this.state.setSelectedIndex((idx + 1) % commands.length);
    }
  }

  private handleArrowUp(): void {
    const commands = this.state.filteredCommands();
    const idx = this.state.selectedIndex();
    if (commands.length > 0) {
      this.state.setSelectedIndex(
        (idx - 1 + commands.length) % commands.length,
      );
    }
  }

  private handleEnter(): void {
    const commands = this.state.filteredCommands();
    const idx = this.state.selectedIndex();
    if (commands[idx]) {
      this.executeCommand(commands[idx]);
    }
  }

  private showPalette(): void {
    this.state.reset();
    this.state.setFilteredCommands(this.buildInitialCommandList());
    this.state.setIsVisible(true);

    this.focusSearchInput();
    this.fetchDefaultEngineName();
  }

  private fetchDefaultEngineName(): void {
    try {
      const { SearchService } = ChromeUtils.importESModule(
        "moz-src:///toolkit/components/search/SearchService.sys.mjs",
      );
      SearchService.getDefault()
        .then((engine: { name?: string }) => {
          this.defaultEngineName = engine?.name ?? null;
        })
        .catch(() => {
          this.defaultEngineName = null;
        });
    } catch {
      this.defaultEngineName = null;
    }
  }

  private animOutTimer: number | null = null;
  private defaultEngineName: string | null = null;
  private historySearchTimer: ReturnType<typeof setTimeout> | null = null;
  private bookmarkSearchTimer: ReturnType<typeof setTimeout> | null = null;
  private currentSearchQuery: string = "";
  private stepChoicesLoadGeneration = 0;

  public hidePalette(): void {
    this.stepChoicesLoadGeneration++;
    this.state.setIsAnimatingOut(true);
    this.state.setIsVisible(false);
    this.debouncedUpdateSearch.clear();

    if (this.historySearchTimer) {
      clearTimeout(this.historySearchTimer);
      this.historySearchTimer = null;
    }
    if (this.bookmarkSearchTimer) {
      clearTimeout(this.bookmarkSearchTimer);
      this.bookmarkSearchTimer = null;
    }

    // Safety fallback: reset isAnimatingOut even if transitionend never fires
    // (prefers-reduced-motion, element removed, etc.)
    this.clearAnimOutTimer();
    this.animOutTimer = this.targetWindow.setTimeout(() => {
      if (this.state.isAnimatingOut()) {
        this.state.setIsAnimatingOut(false);
      }
      this.animOutTimer = null;
    }, 300);
  }

  private clearAnimOutTimer(): void {
    if (this.animOutTimer !== null) {
      this.targetWindow.clearTimeout(this.animOutTimer);
      this.animOutTimer = null;
    }
  }

  public executeCommand(cmd: PaletteCommand): void {
    // If the command has steps, enter input mode instead of executing immediately
    if (cmd.steps && cmd.steps.length > 0) {
      this.enterInputMode(cmd);
      return;
    }

    addRecentCommand(cmd.id);
    incrementFrequency(cmd.id);
    this.hidePalette();
    try {
      cmd.fn(this.targetWindow);
    } catch (e) {
      console.error(`[command-palette] Action failed: ${cmd.id}`, e);
    }
  }

  // --- Multi-step input mode ---

  private shouldIncludeStep(
    step: CommandStep,
    args: Readonly<Record<string, string>>,
  ): boolean {
    try {
      return step.shouldInclude?.(args, this.targetWindow) ?? true;
    } catch (e) {
      console.error(
        `[command-palette] Failed to evaluate step condition: ${step.id}`,
        e,
      );
      return true;
    }
  }

  private findIncludedStepIndex(
    cmd: PaletteCommand,
    startIndex: number,
    direction: 1 | -1,
    args: Readonly<Record<string, string>>,
  ): number | null {
    const steps = cmd.steps ?? [];
    for (
      let index = startIndex;
      index >= 0 && index < steps.length;
      index += direction
    ) {
      if (this.shouldIncludeStep(steps[index], args)) return index;
    }
    return null;
  }

  private pruneExcludedInputs(
    cmd: PaletteCommand,
    inputs: Record<string, string>,
  ): Record<string, string> {
    const steps = cmd.steps ?? [];
    const pruned = { ...inputs };

    // Repeat because removing one stale value may make another conditional
    // step ineligible on the next pass.
    for (let pass = 0; pass < steps.length; pass++) {
      let changed = false;
      for (const step of steps) {
        if (
          Object.hasOwn(pruned, step.id) &&
          !this.shouldIncludeStep(step, pruned)
        ) {
          delete pruned[step.id];
          changed = true;
        }
      }
      if (!changed) break;
    }

    return pruned;
  }

  private isStepChoicesRequestCurrent(
    generation: number,
    command: PaletteCommand,
    step: CommandStep,
    stepIndex: number,
    targetWindow: Window,
  ): boolean {
    return (
      generation === this.stepChoicesLoadGeneration &&
      this.targetWindow === targetWindow &&
      this.state.mode() === "input" &&
      this.state.activeCommand() === command &&
      this.state.currentStepIndex() === stepIndex &&
      command.steps?.[stepIndex] === step
    );
  }

  public getStepProgress(): { current: number; total: number } {
    const cmd = this.state.activeCommand();
    const steps = cmd?.steps;
    if (!cmd || !steps) return { current: 0, total: 0 };

    const inputs = this.pruneExcludedInputs(cmd, this.state.stepInputs());
    const includedIndices = steps.flatMap((step, index) =>
      this.shouldIncludeStep(step, inputs) ? [index] : []
    );
    const ordinal = includedIndices.indexOf(this.state.currentStepIndex());

    return {
      current: ordinal >= 0 ? ordinal + 1 : 0,
      total: includedIndices.length,
    };
  }

  private loadStepChoices(stepIndex: number, restoreValue?: string): void {
    const cmd = this.state.activeCommand();
    const step = cmd?.steps?.[stepIndex];
    if (!step) return;

    const generation = ++this.stepChoicesLoadGeneration;
    const targetWindow = this.targetWindow;
    const args = { ...this.state.stepInputs() };

    this.state.setHasMoreChoices(false);
    this.state.setLoadMoreCallback(null);
    this.state.setLoadingMore(false);

    // Static choices take priority
    if (step.choices && step.choices.length > 0) {
      this.state.setStepChoicesBase(step.choices);
      this.state.setFilteredStepChoices(step.choices);
      this.state.setSelectedChoiceIndex(0);
      if (restoreValue) {
        const idx = step.choices.findIndex((c) => c.value === restoreValue);
        if (idx >= 0) this.state.setSelectedChoiceIndex(idx);
      }
      this.state.setStepChoicesLoading(false);
      return;
    }

    // Try dynamic choices loader
    if (step.choicesLoader) {
      this.state.setStepChoicesBase([]);
      this.state.setFilteredStepChoices([]);
      this.state.setSelectedChoiceIndex(0);
      this.state.setStepChoicesLoading(true);
      let choicesPromise: Promise<CommandStepChoice[] | StepChoicesResult>;
      try {
        choicesPromise = step.choicesLoader(targetWindow, args);
      } catch (e) {
        console.error("[command-palette] Failed to load step choices:", e);
        if (
          this.isStepChoicesRequestCurrent(
            generation,
            cmd,
            step,
            stepIndex,
            targetWindow,
          )
        ) {
          this.state.setStepChoicesLoading(false);
        }
        return;
      }

      choicesPromise
        .then((result) => {
          if (
            this.isStepChoicesRequestCurrent(
              generation,
              cmd,
              step,
              stepIndex,
              targetWindow,
            )
          ) {
            const isResultObject = (v: unknown): v is StepChoicesResult =>
              typeof v === "object" && v !== null && "choices" in v;
            const choices = isResultObject(result)
              ? result.choices
              : (result as CommandStepChoice[]);
            const defaultIdx = isResultObject(result)
              ? result.defaultIndex
              : undefined;

            this.state.setStepChoicesBase(choices);
            this.state.setFilteredStepChoices(choices);
            this.state.setSelectedChoiceIndex(
              defaultIdx !== undefined &&
                defaultIdx >= 0 &&
                defaultIdx < choices.length
                ? defaultIdx
                : 0,
            );
            if (restoreValue) {
              const idx = choices.findIndex((c) => c.value === restoreValue);
              if (idx >= 0) this.state.setSelectedChoiceIndex(idx);
            }
            // Store pagination metadata
            const hasMore = isResultObject(result)
              ? (result.hasMore ?? false)
              : false;
            const loadMore = isResultObject(result)
              ? result.loadMore
              : undefined;
            this.state.setHasMoreChoices(hasMore);
            this.state.setLoadMoreCallback(() => loadMore ?? null);
            this.state.setStepChoicesLoading(false);
          }
        })
        .catch((e) => {
          if (
            this.isStepChoicesRequestCurrent(
              generation,
              cmd,
              step,
              stepIndex,
              targetWindow,
            )
          ) {
            console.error("[command-palette] Failed to load step choices:", e);
            this.state.setStepChoicesBase([]);
            this.state.setFilteredStepChoices([]);
            this.state.setStepChoicesLoading(false);
            this.state.setHasMoreChoices(false);
            this.state.setLoadMoreCallback(null);
          }
        });
      return;
    }

    // No choices at all
    this.state.setStepChoicesBase([]);
    this.state.setFilteredStepChoices([]);
    this.state.setStepChoicesLoading(false);
  }

  private loadMoreChoices(): void {
    const loadMore = this.state.loadMoreCallback();
    if (!loadMore || this.state.loadingMore() || !this.state.hasMoreChoices()) {
      return;
    }

    const command = this.state.activeCommand();
    const stepIndex = this.state.currentStepIndex();
    const step = command?.steps?.[stepIndex];
    if (!command || !step) return;

    const generation = this.stepChoicesLoadGeneration;
    const targetWindow = this.targetWindow;
    this.state.setLoadingMore(true);
    loadMore()
      .then(({ choices: newChoices, hasMore }) => {
        if (
          !this.isStepChoicesRequestCurrent(
            generation,
            command,
            step,
            stepIndex,
            targetWindow,
          )
        ) return;

        this.state.setHasMoreChoices(hasMore);

        const currentBase = this.state.stepChoicesBase();
        const updatedBase = [...currentBase, ...newChoices];
        this.state.setStepChoicesBase(updatedBase);

        // Re-apply current filter to the expanded list
        const q = this.state.query().trim().toLowerCase();
        if (!q) {
          this.state.setFilteredStepChoices(updatedBase);
        } else {
          const filtered = updatedBase.filter(
            (c) =>
              c.label.toLowerCase().includes(q) ||
              c.value.toLowerCase().includes(q) ||
              (c.description?.toLowerCase().includes(q) ?? false),
          );
          this.state.setFilteredStepChoices(filtered);
        }

        // Keep selection at the same position (where "load more" was triggered)
        // Don't reset selectedChoiceIndex — keep it where it was

        this.state.setLoadingMore(false);
      })
      .catch((e) => {
        if (
          !this.isStepChoicesRequestCurrent(
            generation,
            command,
            step,
            stepIndex,
            targetWindow,
          )
        ) return;
        console.error("[command-palette] Failed to load more choices:", e);
        this.state.setLoadingMore(false);
      });
  }

  private enterInputMode(cmd: PaletteCommand): void {
    this.state.setMode("input");
    this.state.setActiveCommand(cmd);
    this.state.setCurrentStepIndex(0);
    this.state.setStepInputs({});
    this.state.setStepError(null);
    this.state.setQuery("");

    const firstIndex = this.findIncludedStepIndex(cmd, 0, 1, {});
    if (firstIndex === null) {
      this.executeWithArgs(cmd, {});
      return;
    }

    this.state.setCurrentStepIndex(firstIndex);
    this.loadStepChoices(firstIndex);

    this.focusSearchInput();
  }

  public advanceStep(): void {
    const cmd = this.state.activeCommand();
    if (!cmd?.steps) return;

    const stepIndex = this.state.currentStepIndex();
    const step = cmd.steps[stepIndex];
    if (!step) return;

    // Determine the value: use selected choice if available, otherwise use typed query
    let value: string;
    const stepHasChoices = (!!step.choices && step.choices.length > 0) ||
      !!step.choicesLoader;
    if (stepHasChoices) {
      const filteredChoices = this.state.filteredStepChoices();
      const choiceIdx = this.state.selectedChoiceIndex();
      if (filteredChoices[choiceIdx]) {
        value = filteredChoices[choiceIdx].value;
      } else {
        // No choice selected, fall back to typed text
        value = this.state.query().trim();
      }
    } else {
      value = this.state.query().trim();
    }

    // Run validation if defined
    if (step.validate) {
      const result = step.validate(value);
      if (result !== true) {
        this.state.setStepError(
          typeof result === "string" ? result : "Invalid input",
        );
        return;
      }
    }

    // Clear any previous error
    this.state.setStepError(null);

    // Save the input for this step
    const inputs = this.pruneExcludedInputs(cmd, {
      ...this.state.stepInputs(),
      [step.id]: value,
    });
    this.state.setStepInputs(inputs);

    const nextIndex = this.findIncludedStepIndex(
      cmd,
      stepIndex + 1,
      1,
      inputs,
    );
    if (nextIndex !== null) {
      this.state.setCurrentStepIndex(nextIndex);
      this.state.setQuery("");

      // Initialize choices for the next step if available
      this.loadStepChoices(nextIndex);

      this.focusSearchInput();
    } else {
      // All steps completed — execute the command with collected args
      this.executeWithArgs(cmd, inputs);
    }
  }

  public goBackStep(): void {
    const cmd = this.state.activeCommand();
    if (!cmd?.steps) {
      this.exitInputMode();
      return;
    }

    const stepIndex = this.state.currentStepIndex();
    const inputs = this.pruneExcludedInputs(cmd, this.state.stepInputs());
    this.state.setStepInputs(inputs);
    const prevIndex = this.findIncludedStepIndex(
      cmd,
      stepIndex - 1,
      -1,
      inputs,
    );

    if (prevIndex !== null) {
      // Go to previous step and restore its input
      const stepId = cmd.steps[prevIndex]?.id;

      this.state.setCurrentStepIndex(prevIndex);
      // Restore the display label (not the internal value) for steps with choices
      const prevValue = stepId ? (inputs[stepId] ?? "") : "";
      const prevStep = cmd.steps[prevIndex];
      const choiceLabel = prevStep?.choices?.find(
        (c) => c.value === prevValue,
      )?.label;
      this.state.setQuery(choiceLabel ?? prevValue);
      this.state.setStepError(null);

      // Restore choices for the previous step
      this.loadStepChoices(prevIndex, prevValue);

      this.focusSearchInput(false);
    } else {
      // At first step — go back to command selection
      this.exitInputMode();
    }
  }

  private exitInputMode(): void {
    this.stepChoicesLoadGeneration++;
    this.state.setMode("command");
    this.state.setActiveCommand(null);
    this.state.setCurrentStepIndex(0);
    this.state.setStepInputs({});
    this.state.setStepError(null);
    this.state.setQuery("");

    // Restore the command list
    this.state.setFilteredCommands(this.buildInitialCommandList());
    this.focusSearchInput();
  }

  private executeWithArgs(
    cmd: PaletteCommand,
    args: Record<string, string>,
  ): void {
    addRecentCommand(cmd.id);
    incrementFrequency(cmd.id);
    this.hidePalette();
    try {
      cmd.fn(this.targetWindow, args);
    } catch (e) {
      console.error(`[command-palette] Action failed: ${cmd.id}`, e);
    }
  }

  // --- Search ---

  private doUpdateSearch(query: string): void {
    // In input mode, don't search — just update query state
    if (this.state.mode() === "input") {
      this.state.setQuery(query);
      this.state.setStepError(null);
      // Update filtered choices if the current step has choices
      this.updateStepChoices(query);
      return;
    }

    const trimmed = query.trim();
    const results: PaletteCommand[] = [];

    // URL navigation suggestion
    if (trimmed && looksLikeUrl(trimmed)) {
      const url = trimmed.startsWith("http") ? trimmed : `https://${trimmed}`;
      results.push({
        id: "__navigate-url",
        label: i18next.t("commandPalette.navigateTo", {
          defaultValue: `Go to ${trimmed}`,
          url: trimmed,
        }),
        description: url,
        category: "navigation-suggestion",
        keywords: [],
        fn: (win) => {
          try {
            const navUrl = trimmed.includes("://")
              ? trimmed
              : `https://${trimmed}`;
            const target = resolvePaletteTarget(win);
            if (!target) return;
            target.browser.loadURI?.(Services.io.newURI(navUrl), {
              triggeringPrincipal: target.principal,
            });
          } catch (e) {
            console.error("[command-palette] Navigation failed", e);
          }
        },
      });
    }

    const commandResults = trimmed
      ? searchCommands(trimmed, this.targetWindow)
      : this.buildInitialCommandList();
    results.push(...commandResults);

    // Show search engine suggestion at the bottom of the list as a fallback.
    // Placing it at the bottom keeps the first matched command selected by
    // default, so Enter always executes the best match — not the search fallback.
    if (trimmed) {
      const engineName = this.defaultEngineName;
      const descriptionText = engineName
        ? i18next.t("commandPalette.searchWithEngineNamed", {
          defaultValue: "Search with {{engine}}",
          engine: engineName,
        })
        : i18next.t("commandPalette.searchWithEngineDescription", {
          defaultValue: "Search with your default search engine",
        });

      results.push({
        id: "__search-engine-fallback",
        label: i18next.t("commandPalette.searchWithEngine", {
          defaultValue: `Search for "${trimmed}"`,
          query: trimmed,
        }),
        description: descriptionText,
        category: "search-suggestion",
        keywords: [],
        fn: (win) => {
          try {
            const target = resolvePaletteTarget(win);
            if (!target) return;
            const { SearchService } = ChromeUtils.importESModule(
              "moz-src:///toolkit/components/search/SearchService.sys.mjs",
            );
            const timeoutPromise = new Promise((_, reject) => {
              win.setTimeout(
                () => reject(new Error("Search engine timeout")),
                2000,
              );
            });
            Promise.race([SearchService.getDefault(), timeoutPromise])
              .then((engine) => {
                if (engine) {
                  if (!isPaletteTargetAvailable(target)) {
                    console.error(
                      "[command-palette] Search suggestion target changed before execution",
                    );
                    return;
                  }
                  const sysPrincipal = (
                    globalThis as typeof globalThis & {
                      Services: {
                        scriptSecurityManager: {
                          getSystemPrincipal(): unknown;
                        };
                      };
                    }
                  ).Services.scriptSecurityManager.getSystemPrincipal();
                  const submission = engine.getSubmission(trimmed);
                  const tab = target.gBrowser.addTab(submission.uri.spec, {
                    triggeringPrincipal: sysPrincipal,
                    inBackground: false,
                    userContextId: target.workspaceUserContextId,
                    postData: submission.postData,
                  } as {
                    inBackground?: boolean;
                    postData?: unknown;
                    triggeringPrincipal?: unknown;
                    userContextId?: number;
                  });
                  target.gBrowser.selectedTab = tab;
                }
              })
              .catch((e) => {
                console.error(
                  "[command-palette] Search fallback failed:",
                  e.message,
                );
              });
          } catch (e) {
            console.error("[command-palette] Search fallback sync error:", e);
          }
        },
      });
    }

    this.state.setFilteredCommands(results);
    this.state.setSelectedIndex(0);

    // Debounced async history and bookmark search
    this.currentSearchQuery = trimmed;
    if (this.historySearchTimer) {
      clearTimeout(this.historySearchTimer);
      this.historySearchTimer = null;
    }
    if (this.bookmarkSearchTimer) {
      clearTimeout(this.bookmarkSearchTimer);
      this.bookmarkSearchTimer = null;
    }

    if (trimmed && !shareModeEnabled()) {
      this.bookmarkSearchTimer = setTimeout(() => {
        this.performBookmarkSearch(trimmed);
      }, 100);
      this.historySearchTimer = setTimeout(() => {
        this.performHistorySearch(trimmed);
      }, 200);
    }
  }

  private async performHistorySearch(query: string): Promise<void> {
    try {
      const results = await searchHistoryCommands(query, 10);
      // Only apply if query hasn't changed since we started
      if (query !== this.currentSearchQuery) return;

      const currentResults = this.state.filteredCommands();
      const existingIds = new Set(currentResults.map((c) => c.id));
      const newResults = results.filter((c) => !existingIds.has(c.id));

      if (newResults.length > 0) {
        this.state.setFilteredCommands([...currentResults, ...newResults]);
      }
    } catch (e) {
      console.error("[command-palette] History search failed:", e);
    }
  }

  private async performBookmarkSearch(query: string): Promise<void> {
    console.debug("[command-palette] performBookmarkSearch called");
    try {
      const results = await searchBookmarkCommands(query, 10);
      console.debug(
        "[command-palette] Bookmark search returned:",
        results.length,
        "results",
      );

      // Only apply if query hasn't changed since we started
      if (query !== this.currentSearchQuery) {
        console.debug("[command-palette] Bookmark search stale");
        return;
      }

      const currentResults = this.state.filteredCommands();
      console.debug(
        "[command-palette] Current filtered commands count:",
        currentResults.length,
      );
      const existingIds = new Set(currentResults.map((c) => c.id));
      const newResults = results.filter((c) => !existingIds.has(c.id));
      console.debug(
        "[command-palette] New bookmark results after dedup:",
        newResults.length,
      );

      if (newResults.length > 0) {
        // Insert bookmark results before history results to maintain display order
        const nonAsyncResults = currentResults.filter(
          (c) =>
            c.category !== "history-suggestions" &&
            c.category !== "bookmark-suggestions",
        );
        const existingBookmarkResults = currentResults.filter(
          (c) => c.category === "bookmark-suggestions",
        );
        const existingHistoryResults = currentResults.filter(
          (c) => c.category === "history-suggestions",
        );
        this.state.setFilteredCommands([
          ...nonAsyncResults,
          ...existingBookmarkResults,
          ...newResults,
          ...existingHistoryResults,
        ]);
        console.debug(
          "[command-palette] Updated filtered commands with bookmark results",
        );
      }
    } catch (e) {
      console.error("[command-palette] Bookmark search failed:", e);
    }
  }

  private debouncedUpdateSearch = debounce((query: string) => {
    this.doUpdateSearch(query);
  }, 30);

  public updateSearch(query: string): void {
    // Step values must be current when Enter is pressed immediately after an
    // input event; command search is the only path that needs debouncing.
    if (this.state.mode() === "input") {
      this.doUpdateSearch(query);
      return;
    }

    if (query.trim()) {
      this.debouncedUpdateSearch(query);
    } else {
      this.doUpdateSearch(query);
    }
  }

  private buildInitialCommandList(): PaletteCommand[] {
    const recentIds = getRecentCommands();
    const allCommands = getPaletteCommands(this.targetWindow);
    const recentSet = new Set(recentIds);
    const freqs = getFrequencies();

    const recentCommands = recentIds
      .map((id) => allCommands.find((c) => c.id === id))
      .filter((c): c is PaletteCommand => c !== undefined)
      .map((c) => ({ ...c, category: "recent" as string }));

    const otherCommands = allCommands
      .filter((c) => !recentSet.has(c.id))
      .sort((a, b) => (freqs[b.id] ?? 0) - (freqs[a.id] ?? 0));

    return [...recentCommands, ...otherCommands];
  }
}
