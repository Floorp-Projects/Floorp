// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import { debounce } from "@solid-primitives/scheduled";
import { createPaletteState, type PaletteState } from "./data/state.ts";
import {
  isEnabled,
  addRecentCommand,
  getFrequencies,
  getRecentCommands,
  incrementFrequency,
  getShowTabs,
  getShowHistory,
  getShowBookmarks,
  getCategoryPriority,
  getMaxResultsPerCategory,
  getMaxBookmarkSuggestions,
  getMaxHistorySuggestions,
  getMaxTabsResults,
  getShortcuts,
  RESERVED_SHORTCUT_PREFIXES,
} from "./config.ts";
import {
  getPaletteCommands,
  getCommand,
  searchCommands,
  searchHistoryCommands,
  searchBookmarkCommands,
  isTabCommand,
} from "./command-registry.ts";
import { shareModeEnabled } from "../browser-share-mode/browser-share-mode.tsx";
import { fuzzyScore, fuzzySearch } from "./fuzzy.ts";
import { getTabCommands } from "./tab-provider.ts";
import { searchBookmarks } from "./bookmark-provider.ts";
import { searchHistory } from "./history-provider.ts";
import {
  compareByPriority,
  sortCategoriesByPriority,
  truncateByCategory,
} from "./category-priority.ts";
import type {
  CommandStep,
  CommandStepChoice,
  PaletteCommand,
  StepChoicesResult,
  CommandPaletteShortcut,
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
  private searchGeneration = 0;

  public hidePalette(): void {
    this.stepChoicesLoadGeneration++;
    // Invalidate any in-flight deferred bookmark/history search: its result
    // callback checks `query !== this.currentSearchQuery` and must drop the
    // stale result now that the palette is hidden.
    this.currentSearchQuery = "";
    // Monotonic guard: hide -> reopen -> re-type the SAME query would otherwise
    // pass the `query !== this.currentSearchQuery` check and apply a stale
    // result from the previous lifecycle. Compare against this generation too.
    this.searchGeneration++;
    // Drop bookmark/history suggestion rows that a deferred Places lookup
    // already applied during this (now closing) lifecycle: the generation
    // guard above only covers lookups still in flight, so rows appended
    // before hidePalette() would otherwise leak into the reopened list.
    this.state.setFilteredCommands(
      this.state.filteredCommands().filter(
        (c) =>
          c.category !== "bookmark-suggestions" &&
          c.category !== "history-suggestions",
      ),
    );
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

    // Reserved shortcut rows ("__reserved:s" / "__reserved:t") transition into
    // their mode by replacing the query without hiding the palette.
    if (cmd.id.startsWith("__reserved:")) {
      try {
        cmd.fn(this.targetWindow);
      } catch (e) {
        console.error(`[command-palette] Action failed: ${cmd.id}`, e);
      }
      return;
    }

    // Shortcut pseudo-commands manage their own recent/frequency using the
    // underlying command id (not the "__shortcut:..." pseudo-id), so that
    // shortcut usage contributes to the real command's recency/frequency.
    if (cmd.category !== "shortcut") {
      addRecentCommand(cmd.id);
      incrementFrequency(cmd.id);
    }
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

  /**
   * Reorder command results to match the user's category priority.
   *
   * The flat array order produced here MATCHES the grouped display order in
   * `CommandList.tsx` (priority-major, fuzzy score minor WITHIN each category).
   * This keeps `selectedIndex` (flat-array space) consistent with the rendered
   * `getGlobalIndex` (grouped-display space), so arrow-key navigation highlights
   * the correct item.
   *
   * Pseudo-categories:
   * - `recent` is always pinned to the top regardless of the priority list
   *   (it is the "recently used" section shown for empty queries).
   * - `navigation-suggestion` is NOT handled here; it is inserted by
   *   `doUpdateSearch` at a fixed top position. The `search` category (search-
   *   engine fallback) is also NOT special-cased here; it flows through normal
   *   priority sorting and naturally sinks to the bottom because it is absent
   *   from `DEFAULT_CATEGORY_PRIORITY` (returns `MAX_SAFE_INTEGER`).
   */
  private applyPriorityTiebreak(
    items: PaletteCommand[],
    query: string,
    priorityList: readonly string[],
  ): PaletteCommand[] {
    if (items.length <= 1) return items;
    const trimmed = query.trim();

    // Group by category, preserving insertion order within each group.
    const groups = new Map<string, PaletteCommand[]>();
    for (const item of items) {
      const list = groups.get(item.category);
      if (list) {
        list.push(item);
      } else {
        groups.set(item.category, [item]);
      }
    }

    // For non-empty queries: sort each non-recent group by fuzzy score
    // descending (stable sort — items with equal scores keep their incoming
    // relative order).
    if (trimmed) {
      // Cache fuzzy scores per item id to avoid O(N log N) recompute inside the
      // comparator. Items without a score (e.g. recent) are not in the cache.
      const scores = new Map<string, number>();
      for (const [category, list] of groups) {
        if (category === "recent") continue; // preserve recency/frequency order
        for (const item of list) {
          scores.set(item.id, fuzzyScore(trimmed, item) ?? 0);
        }
        if (list.length > 1) {
          list.sort(
            (a, b) =>
              (scores.get(b.id) ?? 0) - (scores.get(a.id) ?? 0),
          );
        }
      }
    }

    // Concatenate: recent first (preserved), then visible categories in
    // priority order, then unknown categories (priority index =
    // MAX_SAFE_INTEGER) at the bottom in their incoming order.
    const recentList = groups.get("recent") ?? [];
    const otherCategories = [...groups.keys()].filter((c) => c !== "recent");
    otherCategories.sort((a, b) =>
      compareByPriority({ category: a }, { category: b }, priorityList),
    );

    // `recent` is pinned first here; `CommandList.tsx` also places `recent`
    // before `navigation-suggestion`. The two pseudo-categories never coexist
    // in practice (recent is empty-query-only, navigation-suggestion is
    // URL-query-only), which is what keeps the flat/grouped index invariant
    // sound. See CommandList.tsx for the matching note.
    const result: PaletteCommand[] = [...recentList];
    for (const cat of otherCategories) {
      result.push(...(groups.get(cat) ?? []));
    }
    return result;
  }

  /**
   * Appends asynchronous bookmark/history suggestion results to the current
   * filtered list, ordered by the user's category priority.
   *
   * Pseudo-category `recent` and `navigation-suggestion` are pinned to the top
   * (in their existing order). All other items (main results + suggestions,
   * including the `search` category which has the lowest priority) are
   * re-sorted by priority via `middleItems`. The highlighted command is
   * preserved across the re-sort (selection follows the same command id, or is
   * clamped if it fell out of the list).
   */
  private appendSuggestionResults(
    newResults: PaletteCommand[],
    priorityList: readonly string[],
  ): void {
    if (newResults.length === 0) return;

    const currentResults = this.state.filteredCommands();
    const existingIds = new Set(currentResults.map((c) => c.id));
    const filteredNew = newResults.filter((c) => !existingIds.has(c.id));
    if (filteredNew.length === 0) return;

    const prevIndex = this.state.selectedIndex();
    const prevId = currentResults[prevIndex]?.id;

    const PSEUDO_TOP = new Set(["recent", "navigation-suggestion", "shortcut"]);

    const topItems: PaletteCommand[] = [];
    const middleItems: PaletteCommand[] = [];
    for (const item of currentResults) {
      if (PSEUDO_TOP.has(item.category)) topItems.push(item);
      else middleItems.push(item);
    }
    middleItems.push(...filteredNew);

    const sortedMiddle = sortCategoriesByPriority(middleItems, priorityList);
    const truncatedMiddle = truncateByCategory(
      sortedMiddle,
      getMaxResultsPerCategory(),
      this.buildCategoryLimitOverrides(),
    );
    const newList: PaletteCommand[] = [
      ...topItems,
      ...truncatedMiddle,
    ];
    this.state.setFilteredCommands(newList);

    // Keep the highlight on the same command across the re-sort; if it was
    // truncated out, clamp the index into the new list's range.
    const restoredIndex = prevId === undefined
      ? -1
      : newList.findIndex((c) => c.id === prevId);
    if (restoredIndex !== -1) {
      this.state.setSelectedIndex(restoredIndex);
    } else {
      this.state.setSelectedIndex(
        Math.max(0, Math.min(prevIndex, newList.length - 1)),
      );
    }
  }

  /**
   * Builds a per-category limit override map for `truncateByCategory`.
   * Dynamic-search categories get their own configurable caps; all other
   * categories fall back to the global `maxResultsPerCategory`.
   */
  private buildCategoryLimitOverrides(): Map<string, number> {
    const m = new Map<string, number>();
    m.set("bookmark-suggestions", getMaxBookmarkSuggestions());
    m.set("history-suggestions", getMaxHistorySuggestions());
    m.set("open-tabs", getMaxTabsResults());
    return m;
  }

  /**
   * Builds the @prefix shortcut result list. `prefixPart` is the token the
   * user typed after the leading "@" (no whitespace), and `argsPart` is the
   * remainder of the query after the first whitespace (already trimmed, or
   * "" when there is none).
   *
   * Two modes:
   *
   * 1. `argsPart === ""` (e.g. "@", "@s", "@sh"): ranked list of every
   *    shortcut that matches — exact prefix match first, then starts-with,
   *    then substring. "@" alone returns every shortcut in declaration order.
   *
   * 2. `argsPart !== ""` (e.g. "@s hello world"): process ONLY the exact
   *    prefix match. Starts-with/substring matching would be ambiguous once
   *    the user has committed to a specific shortcut by typing arguments, so
   *    only a single candidate is produced. If the aliased command has
   *    `steps`, the candidate carries the args mapped onto the FIRST step's
   *    id (remaining steps fall back to their defaults — e.g. search-web's
   *    default engine + new-tab). If the aliased command has no steps, the
   *    args are ignored and the plain (args-less) candidate is returned as a
   *    safe fallback. No exact match → empty array (nothing to show).
   *
   * Each shortcut is rendered as a pseudo-`PaletteCommand` whose `fn` resolves
   * and invokes the aliased command. Recent/frequency are recorded under the
   * REAL command id (see `executeCommand`'s `shortcut` guard) so shortcut usage
   * feeds the underlying command's recency.
   */
  private buildShortcutCommands(
    prefixPart: string,
    argsPart: string = "",
  ): PaletteCommand[] {
    // Filter out stale reserved-prefix entries (e.g. old "s"/"t" single-char
    // shortcuts left over from before the prefix was reserved). These would
    // otherwise duplicate the built-in __reserved:s / __reserved:t rows in
    // the "@" alone list.
    const shortcuts = getShortcuts().filter(
      (s) => !RESERVED_SHORTCUT_PREFIXES.includes(s.prefix),
    );
    if (shortcuts.length === 0) return [];

    // --- args-bearing mode: "@prefix <args>" ---
    if (argsPart) {
      const exactMatch = shortcuts.find((s) => s.prefix === prefixPart);
      if (!exactMatch) return [];

      const aliased = getCommand(exactMatch.commandId, this.targetWindow);
      if (!aliased) return [];

      // Non-step aliased command: args don't apply. Fall back to the plain
      // shortcut candidate so the user still gets a usable row.
      if (!aliased.steps || aliased.steps.length === 0) {
        return [this.buildPlainShortcutCommand(exactMatch, aliased)];
      }

      return [this.buildShortcutArgsCommand(exactMatch, aliased, argsPart)];
    }

    // --- empty-args mode: original ranking behavior ---
    const exact: CommandPaletteShortcut[] = [];
    const prefixMatch: CommandPaletteShortcut[] = [];
    const substringMatch: CommandPaletteShortcut[] = [];
    // Dedup by prefix so the same `@prefix` never appears twice (first declared
    // wins). The settings UI enforces prefix uniqueness, but the pref is
    // user-editable, so the controller defensively dedups by prefix to avoid
    // rendering duplicate `@prefix` rows when a user manually mixes same-prefix /
    // different-commandId entries.
    const seen = new Set<string>();

    for (const s of shortcuts) {
      if (seen.has(s.prefix)) continue;
      seen.add(s.prefix);

      if (prefixPart === "") {
        // "@" alone — list everything, preserving declaration order.
        exact.push(s);
      } else if (s.prefix === prefixPart) {
        exact.push(s);
      } else if (s.prefix.startsWith(prefixPart)) {
        prefixMatch.push(s);
      } else if (s.prefix.includes(prefixPart)) {
        substringMatch.push(s);
      }
    }

    const ranked = [...exact, ...prefixMatch, ...substringMatch];

    // Resolve the aliased command once per shortcut and drop any whose target no
    // longer exists (deleted/renamed command) — emitting a dead pseudo-command
    // that only warns at run time is worse than omitting it. The command
    // catalogue is built ONCE into a Map (getPaletteCommands rebuilds the full
    // array per call, so a per-shortcut lookup would be O(n×m)) and each
    // shortcut resolves against it by id. `aliased` is reused for the label.
    // The `fn` re-resolves against the actual target `win` at execution time,
    // since the command set may differ from the window used to build the list.
    const commandsById = new Map(
      getPaletteCommands(this.targetWindow).map((c) => [c.id, c] as const),
    );
    const resolved = ranked
      .map((s) => ({ s, aliased: commandsById.get(s.commandId) }))
      .filter(
        (r): r is { s: CommandPaletteShortcut; aliased: PaletteCommand } =>
          r.aliased !== undefined,
      );

    return resolved.map(({ s, aliased }) =>
      this.buildPlainShortcutCommand(s, aliased),
    );
  }

  /**
   * Builds the plain (args-less) pseudo-command for a single shortcut. Used
   * both for the ranked shortcut list (empty-args mode) and as the fallback
   * for args-bearing queries whose aliased command has no steps.
   */
  private buildPlainShortcutCommand(
    s: CommandPaletteShortcut,
    aliased: PaletteCommand,
  ): PaletteCommand {
    return {
      id: `__shortcut:${s.prefix}:${s.commandId}`,
      label: i18next.t("commandPalette.shortcutLabel", {
        defaultValue: `@${s.prefix}`,
        prefix: s.prefix,
      }),
      description: aliased.label,
      category: "shortcut",
      keywords: [s.prefix, `@${s.prefix}`],
      fn: (win) => {
        const cmd = getCommand(s.commandId, win);
        if (cmd) {
          // Record usage under the REAL command id so the aliased command's
          // recency/frequency grows with shortcut invocations.
          addRecentCommand(s.commandId);
          incrementFrequency(s.commandId);
          try {
            cmd.fn(win);
          } catch (e) {
            console.error(
              "[command-palette] Shortcut action failed:",
              s.commandId,
              e,
            );
          }
        } else {
          console.warn(
            "[command-palette] Shortcut target not found:",
            s.commandId,
          );
        }
      },
    };
  }

  /**
   * Builds an args-bearing pseudo-command for "@prefix <args>". The aliased
   * command MUST have `steps` (caller ensures this); `argsPart` is mapped onto
   * the FIRST step's id, and remaining steps fall back to their defaults
   * (e.g. search-web: default engine + new tab).
   *
   * For any `search`-category command, the description is localized as a
   * "Search \"<query>\"" string to mirror the existing search fallback row.
   * For any other step command, the aliased command's own label is used as
   * the description (keeps the row meaningful without command-specific
   * special-casing beyond the search category).
   */
  private buildShortcutArgsCommand(
    s: CommandPaletteShortcut,
    aliased: PaletteCommand,
    argsPart: string,
  ): PaletteCommand {
    return {
      id: `__shortcut:${s.prefix}:${s.commandId}:args`,
      label: i18next.t("commandPalette.shortcutWithArgsLabel", {
        defaultValue: `@${s.prefix} ${argsPart}`,
        prefix: s.prefix,
        args: argsPart,
      }),
      description: aliased.category === "search"
        ? i18next.t("commandPalette.searchShortcutDescription", {
          defaultValue: `Search "${argsPart}"`,
          query: argsPart,
        })
        : aliased.label,
      category: "shortcut",
      keywords: [s.prefix, `@${s.prefix}`],
      fn: (win) => {
        const cmd = getCommand(s.commandId, win);
        if (cmd) {
          // Validate the args against the first step's validator (if any).
          // This prevents e.g. `@u javascript:alert(1)` from bypassing
          // open-url's URL validation. Returns `true` to pass, or an error
          // message string.
          const firstStep = cmd.steps?.[0];
          if (firstStep?.validate) {
            const validateResult = firstStep.validate(argsPart);
            if (validateResult !== true) {
              console.warn(
                "[command-palette] Shortcut args rejected by step validation:",
                s.commandId,
                validateResult,
              );
              return;
            }
          }
          addRecentCommand(s.commandId);
          incrementFrequency(s.commandId);
          try {
            // Pass argsPart as the value of the FIRST step. Remaining steps
            // fall back to their defaults (e.g. search-web: default engine +
            // new tab).
            const firstStepId = firstStep?.id;
            const args = firstStepId
              ? { [firstStepId]: argsPart }
              : undefined;
            cmd.fn(win, args);
          } catch (e) {
            console.error(
              "[command-palette] Shortcut action failed:",
              s.commandId,
              e,
            );
          }
        } else {
          console.warn(
            "[command-palette] Shortcut target not found:",
            s.commandId,
          );
        }
      },
    };
  }

  /**
   * Rows shown when the query is exactly "@": the reserved built-in shortcuts.
   * Selecting one transitions into its mode by replacing the query (e.g. "@s ")
   * without hiding the palette — see the "__reserved:" branch in executeCommand.
   */
  private buildReservedShortcutCommands(): PaletteCommand[] {
    const reserved: PaletteCommand[] = [];

    // @s — built-in web search
    const searchCmd = getCommand("floorp-search-web", this.targetWindow);
    if (searchCmd) {
      reserved.push({
        id: "__reserved:s",
        label: i18next.t("commandPalette.shortcutLabel", {
          defaultValue: "@s",
          prefix: "s",
        }),
        description: searchCmd.label,
        category: "shortcut",
        keywords: ["s", "@s"],
        fn: (win) => {
          this.state.setQuery("@s ");
          this.state.setHighlightQuery("");
          this.updateSearch("@s ");
          // The SolidJS controlled value binding does not reliably update the
          // input's displayed value in Firefox/XUL, so mirror focusSearchInput
          // and sync the input's value with state.query() via direct DOM
          // manipulation.
          const input = win.document?.getElementById(
            "command-palette-search",
          ) as HTMLInputElement | null;
          if (input) {
            input.value = "@s ";
          }
        },
      });
    }

    // @t — built-in tab search
    reserved.push({
      id: "__reserved:t",
      label: i18next.t("commandPalette.shortcutLabel", {
        defaultValue: "@t",
        prefix: "t",
      }),
      description: i18next.t("commandPalette.shortcuts.reservedTabSearch", {
        defaultValue: "Search Open Tabs",
      }),
      category: "shortcut",
      keywords: ["t", "@t"],
      fn: (win) => {
        this.state.setQuery("@t ");
        this.state.setHighlightQuery("");
        this.updateSearch("@t ");
        // The SolidJS controlled value binding does not reliably update the
        // input's displayed value in Firefox/XUL, so mirror focusSearchInput
        // and sync the input's value with state.query() via direct DOM
        // manipulation.
        const input = win.document?.getElementById(
          "command-palette-search",
        ) as HTMLInputElement | null;
        if (input) {
          input.value = "@t ";
        }
      },
    });

    // @b — built-in bookmark search
    reserved.push({
      id: "__reserved:b",
      label: i18next.t("commandPalette.shortcutLabel", {
        defaultValue: "@b",
        prefix: "b",
      }),
      description: i18next.t("commandPalette.shortcuts.reservedBookmarkSearch", {
        defaultValue: "Search Bookmarks",
      }),
      category: "shortcut",
      keywords: ["b", "@b"],
      fn: (win) => {
        this.state.setQuery("@b ");
        this.state.setHighlightQuery("");
        this.updateSearch("@b ");
        // The SolidJS controlled value binding does not reliably update the
        // input's displayed value in Firefox/XUL, so mirror focusSearchInput
        // and sync the input's value with state.query() via direct DOM
        // manipulation.
        const input = win.document?.getElementById(
          "command-palette-search",
        ) as HTMLInputElement | null;
        if (input) {
          input.value = "@b ";
        }
      },
    });

    // @h — built-in history search
    reserved.push({
      id: "__reserved:h",
      label: i18next.t("commandPalette.shortcutLabel", {
        defaultValue: "@h",
        prefix: "h",
      }),
      description: i18next.t("commandPalette.shortcuts.reservedHistorySearch", {
        defaultValue: "Search History",
      }),
      category: "shortcut",
      keywords: ["h", "@h"],
      fn: (win) => {
        this.state.setQuery("@h ");
        this.state.setHighlightQuery("");
        this.updateSearch("@h ");
        // The SolidJS controlled value binding does not reliably update the
        // input's displayed value in Firefox/XUL, so mirror focusSearchInput
        // and sync the input's value with state.query() via direct DOM
        // manipulation.
        const input = win.document?.getElementById(
          "command-palette-search",
        ) as HTMLInputElement | null;
        if (input) {
          input.value = "@h ";
        }
      },
    });

    return reserved;
  }

  private doUpdateSearch(query: string): void {
    // In input mode, don't search — just update query state
    if (this.state.mode() === "input") {
      this.state.setQuery(query);
      this.state.setStepError(null);
      // Update filtered choices if the current step has choices
      this.updateStepChoices(query);
      return;
    }

    const hasTrailingSpace = /\s$/.test(query);
    const trimmed = query.trim();
    const results: PaletteCommand[] = [];

    // @prefix shortcut matching — pinned to the very top, above everything else.
    // When query starts with "@", show matching shortcuts (exact prefix match
    // first, then prefix matches). Selecting one executes the aliased command.
    // Shortcut results bypass priority sorting/truncation entirely (they are a
    // pseudo-category like `recent`/`navigation-suggestion`; see PSEUDO_TOP in
    // `appendSuggestionResults`).
    if (trimmed.startsWith("@")) {
      const afterAt = trimmed.slice(1); // strip "@"
      // Split "@prefix args": first whitespace separates the prefix token
      // from the rest (the argument). This lets "@s hello world" resolve to
      // the "@s" shortcut with "hello world" as the search query. "@s" exactly
      // (no trailing whitespace) enters the reserved shortcut branch (shows
      // __reserved:s + user-defined matches); "@s " (trailing whitespace) or
      // "@s <query>" commits to the dedicated web-search mode.
      const spaceIdx = afterAt.search(/\s/);
      const prefixPart = spaceIdx === -1 ? afterAt : afterAt.slice(0, spaceIdx);
      const argsPart = spaceIdx === -1 ? "" : afterAt.slice(spaceIdx + 1).trim();

      // @t — built-in open-tabs search mode. The "t" prefix is reserved (see
      // RESERVED_SHORTCUT_PREFIXES in config.ts) and always wins over any
      // user-defined shortcut with the same prefix. Typing "@t" lists every open
      // tab; "@t <query>" filters them by title/URL with the same fuzzy scoring
      // used across the palette. Enter executes the tab command, which switches
      // to that tab. All tabs are shown without the per-category limit so the
      // list behaves like a dedicated fzf-style tab switcher. Unlike normal
      // search — where the `showTabs` pref excludes tab commands when it is off
      // — @t is an explicit tab-search mode, so it ignores the pref and always
      // shows every open tab (intentional).
      if (prefixPart === "t") {
        // Clear any in-flight or debounced bookmark/history search armed by the
        // normal search path (or a previous @b/@h query) so the stale async
        // result cannot leak into the tab-search / web-search list below.
        // Same clearing pattern as the @b/@h branch.
        this.currentSearchQuery = "";
        if (this.bookmarkSearchTimer) {
          clearTimeout(this.bookmarkSearchTimer);
          this.bookmarkSearchTimer = null;
        }
        if (this.historySearchTimer) {
          clearTimeout(this.historySearchTimer);
          this.historySearchTimer = null;
        }
        // "@t" exactly (no args, no trailing space): don't commit to the
        // dedicated tab-search mode yet. Show the reserved @t row plus any
        // user-defined shortcuts starting with "@t" so typing continues to
        // narrow the list (fzf-style). "@t " (trailing space) or "@t <query>"
        // commits to tab-search mode below.
        if (!argsPart && !hasTrailingSpace) {
          results.push(
            ...this.buildReservedShortcutCommands().filter(
              (c) => c.id === "__reserved:t",
            ),
          );
          results.push(...this.buildShortcutCommands("t", ""));
          this.state.setHighlightQuery(trimmed);
          this.state.setFilteredCommands(results);
          this.state.setSelectedIndex(0);
          return; // Skip URL navigation, command search, search-engine fallback and async suggestions
        }

        const tabCommands = getTabCommands(this.targetWindow);
        const filtered = argsPart
          ? fuzzySearch(argsPart, tabCommands, tabCommands.length)
          : tabCommands;
        results.push(...filtered);
        this.state.setHighlightQuery(argsPart);
        this.state.setFilteredCommands(results);
        this.state.setSelectedIndex(0);
        return; // Skip URL navigation, command search, search-engine fallback and async suggestions
      }

      // @s — built-in web search (reserved prefix, always wins over any user-defined
      // shortcut with the same prefix). Mirrors the settings UI's "reserved" row:
      // "@s" always resolves to floorp-search-web regardless of the user shortcuts
      // pref, which may be empty or contain a stale "s" entry from before the prefix
      // was reserved. "@s <query>" maps the query to the first step and executes the
      // search directly; "@s" alone resolves to the plain (args-less) pseudo-command,
      // which calls search-web's fn with no args — search-web's fn early-returns when
      // query is absent, so this is a silent no-op (not a step-input entry).
      if (prefixPart === "s") {
        // Clear any in-flight or debounced bookmark/history search armed by the
        // normal search path (or a previous @b/@h query) so the stale async
        // result cannot leak into the tab-search / web-search list below.
        // Same clearing pattern as the @b/@h branch.
        this.currentSearchQuery = "";
        if (this.bookmarkSearchTimer) {
          clearTimeout(this.bookmarkSearchTimer);
          this.bookmarkSearchTimer = null;
        }
        if (this.historySearchTimer) {
          clearTimeout(this.historySearchTimer);
          this.historySearchTimer = null;
        }
        // "@s" exactly (no args, no trailing space): don't commit to the
        // dedicated web-search mode yet. Show the reserved @s row (when
        // floorp-search-web is registered) plus any user-defined shortcuts
        // starting with "@s" so typing continues to narrow the list
        // (fzf-style). "@s " (trailing space) or "@s <query>" commits to
        // web-search mode below.
        if (!argsPart && !hasTrailingSpace) {
          results.push(
            ...this.buildReservedShortcutCommands().filter(
              (c) => c.id === "__reserved:s",
            ),
          );
          results.push(...this.buildShortcutCommands("s", ""));
          this.state.setHighlightQuery(trimmed);
          this.state.setFilteredCommands(results);
          this.state.setSelectedIndex(0);
          return; // Same as @t: skip URL navigation, command search, search fallback and async suggestions
        }

        const searchCmd = getCommand("floorp-search-web", this.targetWindow);
        if (searchCmd) {
          // Guard: args-bearing mode requires steps (buildShortcutArgsCommand
          // assumes the caller verified steps exist). Fall back to the plain
          // shortcut when steps are missing — mirrors buildShortcutCommands'
          // existing guard at the non-reserved args path.
          results.push(
            argsPart && searchCmd.steps?.length
              ? this.buildShortcutArgsCommand(
                { prefix: "s", commandId: "floorp-search-web" },
                searchCmd,
                argsPart,
              )
              : this.buildPlainShortcutCommand(
                { prefix: "s", commandId: "floorp-search-web" },
                searchCmd,
              ),
          );
        }
        this.state.setHighlightQuery(argsPart);
        this.state.setFilteredCommands(results);
        this.state.setSelectedIndex(0);
        return; // Same as @t: skip URL navigation, command search, search fallback and async suggestions
      }

      // @b — built-in bookmark search mode / @h — built-in history search mode
      // (reserved prefixes, always win over any user-defined shortcut with the
      // same prefix). "@b"/"@h" exactly (no args, no trailing space): don't
      // commit to the dedicated search mode yet — show the reserved row plus
      // any user-defined shortcuts starting with the prefix so typing
      // continues to narrow the list (fzf-style). "@b " / "@h " (trailing
      // space) or "@b <query>" / "@h <query>" commits to the dedicated
      // bookmark/history search mode below.
      if (prefixPart === "b" || prefixPart === "h") {
        const isBookmark = prefixPart === "b";
        const reservedId = isBookmark ? "__reserved:b" : "__reserved:h";
        if (!argsPart && !hasTrailingSpace) {
          // Clear any in-flight or debounced @b/@h search from a previous
          // committed query (e.g. "@b foo" deleted back to "@b"): the async
          // Places result or armed debounce timer of the old query must not
          // leak into this fzf-style narrowing list. Same clearing pattern
          // as hidePalette.
          this.currentSearchQuery = "";
          if (this.bookmarkSearchTimer) {
            clearTimeout(this.bookmarkSearchTimer);
            this.bookmarkSearchTimer = null;
          }
          if (this.historySearchTimer) {
            clearTimeout(this.historySearchTimer);
            this.historySearchTimer = null;
          }
          results.push(
            ...this.buildReservedShortcutCommands().filter(
              (c) => c.id === reservedId,
            ),
          );
          results.push(...this.buildShortcutCommands(prefixPart, ""));
          this.state.setHighlightQuery(trimmed);
          this.state.setFilteredCommands(results);
          this.state.setSelectedIndex(0);
          return; // Same as @t/@s: skip URL navigation, command search, search fallback and async suggestions
        }

        // Committed bookmark/history search mode: the result list is populated
        // asynchronously (Places) using the same debounce + stale-query
        // protection as the dynamic search below. The empty-args case ("@b " /
        // "@h ") lists the most recent entries up to the configured limit;
        // args-bearing queries search by term.
        //
        // Unlike dynamic suggestions — which respect getShowBookmarks() /
        // getShowHistory() / shareModeEnabled() — @b/@h is an explicit
        // bookmark/history search mode, so those settings are intentionally
        // ignored: users who disabled bookmark/history suggestions in settings
        // can still use @b/@h for explicit searches (same rationale as @t
        // ignoring the `showTabs` pref, documented above).
        this.currentSearchQuery = trimmed;
        const generation = this.searchGeneration;
        if (this.historySearchTimer) {
          clearTimeout(this.historySearchTimer);
          this.historySearchTimer = null;
        }
        if (this.bookmarkSearchTimer) {
          clearTimeout(this.bookmarkSearchTimer);
          this.bookmarkSearchTimer = null;
        }

        const runSearch = async (): Promise<void> => {
          let suggestions: PaletteCommand[] = [];
          try {
            if (isBookmark) {
              suggestions = argsPart
                ? await searchBookmarkCommands(
                  argsPart,
                  getMaxBookmarkSuggestions(),
                )
                : await searchBookmarks("", getMaxBookmarkSuggestions());
            } else {
              suggestions = argsPart
                ? await searchHistoryCommands(
                  argsPart,
                  getMaxHistorySuggestions(),
                )
                : await searchHistory("", getMaxHistorySuggestions());
            }
          } catch (e) {
            console.error(
              `[command-palette] ${
                isBookmark ? "Bookmark" : "History"
              } search failed:`,
              e,
            );
          }
          // Only apply if the query hasn't changed since we started and the
          // search belongs to the current palette lifecycle.
          if (trimmed !== this.currentSearchQuery) return;
          if (generation !== this.searchGeneration) return;
          this.appendSuggestionResults(suggestions, getCategoryPriority());
        };

        if (isBookmark) {
          this.bookmarkSearchTimer = setTimeout(() => {
            void runSearch();
          }, 100);
        } else {
          this.historySearchTimer = setTimeout(() => {
            void runSearch();
          }, 200);
        }

        this.state.setHighlightQuery(argsPart);
        this.state.setFilteredCommands(results);
        this.state.setSelectedIndex(0);
        return; // Same as @t/@s: skip URL navigation, command search, search fallback and async suggestions
      }

      const shortcutResults = this.buildShortcutCommands(prefixPart, argsPart);
      if (shortcutResults.length > 0) {
        // Pin shortcut results to top; subsequent pushes append below them.
        results.push(...shortcutResults);
      }

      // "@" alone: show the reserved built-in shortcuts first, then user-defined ones.
      if (prefixPart === "") {
        results.unshift(...this.buildReservedShortcutCommands());
      }
    }

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

    const rawCommandResults = trimmed
      ? searchCommands(trimmed, this.targetWindow)
      : this.buildInitialCommandList();
    const filteredByTabs = getShowTabs()
      ? rawCommandResults
      : rawCommandResults.filter((c) => !isTabCommand(c.id));
    // Reorder commands to match the user's category priority.
    //
    // The flat array order produced by `applyPriorityTiebreak` mirrors the
    // grouped display order consumed by `CommandList` (priority-major, fuzzy
    // score minor WITHIN each category). This keeps `selectedIndex` (flat-array
    // space) consistent with the rendered `getGlobalIndex` (grouped-display
    // space), so arrow-key navigation highlights the correct row.
    //
    // `recent` is always pinned to the top inside the helper. The pseudo-
    // categories `navigation-suggestion` (added below at the top for URL-like
    // queries) and the `search` category (search-engine fallback, added at the
    // bottom — `search` is absent from DEFAULT_CATEGORY_PRIORITY so it sinks to
    // the lowest priority automatically) are NOT touched by the helper.
    const priorityList = getCategoryPriority();
    const maxPerCategory = getMaxResultsPerCategory();
    const sorted = this.applyPriorityTiebreak(filteredByTabs, trimmed, priorityList);
    // `recent` is a multi-item pseudo-category (recently-used commands shown on
    // empty query). Exempt it from per-category truncation so users always see
    // their full recents list regardless of the limit. Mirrors the PSEUDO_TOP
    // policy in `appendSuggestionResults`.
    const recentItems = sorted.filter((c) => c.category === "recent");
    const nonRecent = sorted.filter((c) => c.category !== "recent");
    results.push(...recentItems, ...truncateByCategory(nonRecent, maxPerCategory, this.buildCategoryLimitOverrides()));

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
        category: "search",
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

    // Keep the highlight query in sync with the full query for all paths that
    // fall through here (@t mode early-returns above, so it is unaffected).
    this.state.setHighlightQuery(trimmed);
    this.state.setFilteredCommands(results);
    this.state.setSelectedIndex(0);

    // Debounced async history and bookmark search
    this.currentSearchQuery = trimmed;
    const generation = this.searchGeneration;
    if (this.historySearchTimer) {
      clearTimeout(this.historySearchTimer);
      this.historySearchTimer = null;
    }
    if (this.bookmarkSearchTimer) {
      clearTimeout(this.bookmarkSearchTimer);
      this.bookmarkSearchTimer = null;
    }

    // @prefix shortcut mode is a synchronous, self-contained result set pinned
    // to the top; bookmark/history async suggestions would only interleave with
    // (and potentially displace) shortcut results, so skip them entirely for
    // `@`-prefixed queries.
    if (trimmed && !trimmed.startsWith("@") && !shareModeEnabled()) {
      if (getShowBookmarks()) {
        this.bookmarkSearchTimer = setTimeout(() => {
          void this.performBookmarkSearch(trimmed, generation);
        }, 100);
      }
      if (getShowHistory()) {
        this.historySearchTimer = setTimeout(() => {
          void this.performHistorySearch(trimmed, generation);
        }, 200);
      }
    }
  }

  private async performHistorySearch(
    query: string,
    generation: number,
  ): Promise<void> {
    try {
      const results = await searchHistoryCommands(
        query,
        getMaxHistorySuggestions(),
      );
      // Only apply if query hasn't changed since we started and the search
      // belongs to the current palette lifecycle.
      if (query !== this.currentSearchQuery) return;
      if (generation !== this.searchGeneration) return;
      this.appendSuggestionResults(results, getCategoryPriority());
    } catch (e) {
      console.error("[command-palette] History search failed:", e);
    }
  }

  private async performBookmarkSearch(
    query: string,
    generation: number,
  ): Promise<void> {
    try {
      const results = await searchBookmarkCommands(
        query,
        getMaxBookmarkSuggestions(),
      );
      // Only apply if query hasn't changed since we started and the search
      // belongs to the current palette lifecycle.
      if (query !== this.currentSearchQuery) return;
      if (generation !== this.searchGeneration) return;
      this.appendSuggestionResults(results, getCategoryPriority());
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
