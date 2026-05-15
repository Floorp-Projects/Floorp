// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";
import { debounce } from "@solid-primitives/scheduled";
import { createPaletteState, type PaletteState } from "./data/state.ts";
import {
  isEnabled,
  addRecentCommand,
  getRecentCommands,
  incrementFrequency,
  getFrequencies,
} from "./config.ts";
import { getPaletteCommands, searchCommands } from "./command-registry.ts";
import type { PaletteCommand } from "./command-registry.ts";

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
    if (this.eventListenersAttached) {
      this.targetWindow.removeEventListener(
        "keydown",
        this.handlePaletteKeyDown,
        true,
      );
      this.eventListenersAttached = false;
    }
    this.clearAnimOutTimer();
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

  private handleInputModeKeyDown(event: KeyboardEvent): void {
    switch (event.key) {
      case "Escape":
        event.preventDefault();
        event.stopPropagation();
        this.hidePalette();
        break;

      case "Enter":
        event.preventDefault();
        event.stopPropagation();
        this.advanceStep();
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

  private focusSearchInput(): void {
    this.targetWindow.setTimeout(() => {
      const input = this.targetWindow.document?.getElementById(
        "command-palette-search",
      ) as HTMLInputElement | null;
      if (!input) return;
      // Clear input value directly on the DOM element when in input mode
      // (SolidJS reactive value binding may not reliably update in Firefox/XUL)
      if (this.state.mode() === "input") {
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
        .then((engine: any) => {
          this.defaultEngineName = engine?.name ?? null;
        })
        .catch(() => {
          this.defaultEngineName = null;
        });
    } catch {
      this.defaultEngineName = null;
    }
  }

  private animOutTimer: ReturnType<typeof setTimeout> | null = null;
  private defaultEngineName: string | null = null;

  public hidePalette(): void {
    this.state.setIsAnimatingOut(true);
    this.state.setIsVisible(false);
    this.debouncedUpdateSearch.clear();

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

  private enterInputMode(cmd: PaletteCommand): void {
    this.state.setMode("input");
    this.state.setActiveCommand(cmd);
    this.state.setCurrentStepIndex(0);
    this.state.setStepInputs({});
    this.state.setStepError(null);
    this.state.setQuery("");

    this.focusSearchInput();
  }

  public advanceStep(): void {
    const cmd = this.state.activeCommand();
    if (!cmd?.steps) return;

    const stepIndex = this.state.currentStepIndex();
    const step = cmd.steps[stepIndex];
    if (!step) return;

    const value = this.state.query().trim();

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
    const inputs = { ...this.state.stepInputs(), [step.id]: value };
    this.state.setStepInputs(inputs);

    // Check if there are more steps
    const nextIndex = stepIndex + 1;
    if (nextIndex < cmd.steps.length) {
      this.state.setCurrentStepIndex(nextIndex);
      this.state.setQuery("");
      this.focusSearchInput();
    } else {
      // All steps completed — execute the command with collected args
      this.executeWithArgs(cmd, inputs);
    }
  }

  public goBackStep(): void {
    const stepIndex = this.state.currentStepIndex();
    if (stepIndex > 0) {
      // Go to previous step and restore its input
      const prevIndex = stepIndex - 1;
      const cmd = this.state.activeCommand();
      const stepId = cmd?.steps?.[prevIndex]?.id;
      const inputs = this.state.stepInputs();

      this.state.setCurrentStepIndex(prevIndex);
      this.state.setQuery(stepId ? (inputs[stepId] ?? "") : "");
      this.state.setStepError(null);
      this.focusSearchInput();
    } else {
      // At first step — go back to command selection
      this.exitInputMode();
    }
  }

  private exitInputMode(): void {
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
        fn: (_win) => {
          try {
            const navUrl = trimmed.includes("://")
              ? trimmed
              : `https://${trimmed}`;
            let principal =
              globalThis.gBrowser?.selectedBrowser?.contentPrincipal;
            globalThis.gBrowser.loadURI?.(Services.io.newURI(navUrl), {
              triggeringPrincipal: principal,
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

    // Always show search engine suggestion at the top when there's a query
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

      results.unshift({
        id: "__search-engine-fallback",
        label: i18next.t("commandPalette.searchWithEngine", {
          defaultValue: `Search for "${trimmed}"`,
          query: trimmed,
        }),
        description: descriptionText,
        category: "search-suggestion",
        keywords: [],
        fn: (_win) => {
          try {
            const { SearchService } = ChromeUtils.importESModule(
              "moz-src:///toolkit/components/search/SearchService.sys.mjs",
            );
            const timeoutPromise = new Promise((_, reject) => {
              globalThis.setTimeout(
                () => reject(new Error("Search engine timeout")),
                2000,
              );
            });
            Promise.race([SearchService.getDefault(), timeoutPromise])
              .then((engine) => {
                if (engine) {
                  const sysPrincipal =
                    globalThis.Services.scriptSecurityManager.getSystemPrincipal();
                  const submission = engine.getSubmission(trimmed);
                  const tab = globalThis.gBrowser?.addTab(submission.uri.spec, {
                    triggeringPrincipal: sysPrincipal,
                    inBackground: false,
                    postData: submission.postData,
                  });
                  if (globalThis.gBrowser && tab) {
                    globalThis.gBrowser.selectedTab = tab;
                  }
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
    // Default to the second item when search suggestion is at top and other results exist,
    // so the first real command is selected instead of the always-present search suggestion.
    this.state.setSelectedIndex(trimmed && results.length > 1 ? 1 : 0);
  }

  private debouncedUpdateSearch = debounce((query: string) => {
    this.doUpdateSearch(query);
  }, 30);

  public updateSearch(query: string): void {
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
