// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { CommandPaletteController } from "../controller.ts";
import type { PaletteCommand } from "../types.ts";
import { shareModeEnabled } from "../../browser-share-mode/browser-share-mode.tsx";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

function deferred<T>() {
  let resolve!: (value: T) => void;
  const promise = new Promise<T>((done) => resolve = done);
  return { promise, resolve };
}

function command(id: string, category = "test"): PaletteCommand {
  return { id, label: id, description: "", category, keywords: [], fn() {} };
}

function controlledSearch() {
  const history = [deferred<PaletteCommand[]>(), deferred<PaletteCommand[]>()];
  const bookmarks = [
    deferred<PaletteCommand[]>(),
    deferred<PaletteCommand[]>(),
  ];
  const startedHistory = [deferred<void>(), deferred<void>()];
  const startedBookmarks = [deferred<void>(), deferred<void>()];
  let historyCalls = 0;
  let bookmarkCalls = 0;
  return {
    history,
    bookmarks,
    async started(index: number) {
      let timer: ReturnType<typeof globalThis.setTimeout> | undefined;
      try {
        await Promise.race([
          Promise.all([
            startedHistory[index].promise,
            startedBookmarks[index].promise,
          ]),
          new Promise<never>((_, reject) => {
            timer = globalThis.setTimeout(
              () => reject(new Error("search providers did not start")),
              5000,
            );
          }),
        ]);
      } finally {
        globalThis.clearTimeout(timer);
      }
    },
    providers: {
      history() {
        const index = historyCalls++;
        startedHistory[index].resolve();
        return history[index].promise;
      },
      bookmarks() {
        const index = bookmarkCalls++;
        startedBookmarks[index].resolve();
        return bookmarks[index].promise;
      },
    },
  };
}

function timerWindow() {
  const events = new EventTarget();
  const timers = new Map<number, () => void>();
  let scheduled = 0;
  const win = {
    addEventListener: events.addEventListener.bind(events),
    removeEventListener: events.removeEventListener.bind(events),
    setTimeout(callback: () => void) {
      timers.set(++scheduled, callback);
      return scheduled;
    },
    clearTimeout(id: number) {
      timers.delete(id);
    },
  } as unknown as Window;
  return { win, timers, scheduled: () => scheduled };
}

export async function runAllTests(): Promise<void> {
  await runTests("controllerLifecycle.test.ts", [{
    name: "destroy hides immediately without creating an exit animation timer",
    fn() {
      const host = timerWindow();
      const controller = new CommandPaletteController(host.win);
      controller.state.setIsVisible(true);
      controller.destroy();
      controller.destroy();
      controller.togglePalette();
      controller.hidePalette();
      assertEquals(
        controller.state.isVisible(),
        false,
        "destroyed palette stays hidden",
      );
      assertEquals(
        controller.state.isAnimatingOut(),
        false,
        "destroy does not animate",
      );
      assertEquals(host.scheduled(), 0, "destroy does not schedule a timer");
    },
  }, {
    name: "destroy cancels an existing exit animation",
    fn() {
      const host = timerWindow();
      const controller = new CommandPaletteController(host.win);
      try {
        controller.state.setIsVisible(true);
        controller.hidePalette();
        assertEquals(
          host.timers.size,
          1,
          "normal hide starts an animation fallback",
        );
        controller.destroy();
        assertEquals(host.timers.size, 0, "destroy clears the fallback");
        assertEquals(
          controller.state.isAnimatingOut(),
          false,
          "animation state cleared",
        );
      } finally {
        controller.destroy();
      }
    },
  }, {
    name: "destroy cancels a pending debounce even while the palette is hidden",
    async fn() {
      const controller = new CommandPaletteController(window);
      const baseline = [command("baseline")];
      controller.state.setFilteredCommands(baseline);
      controller.updateSearch("lifecycle-pending-query");
      controller.destroy();
      controller.updateSearch("lifecycle-after-destroy");
      await new Promise((resolve) => globalThis.setTimeout(resolve, 60));
      assertEquals(
        controller.state.filteredCommands(),
        baseline,
        "no delayed callback mutates disposed state",
      );
    },
  }, {
    name: "history and bookmark results are discarded after destruction",
    async fn() {
      const savedShareMode = shareModeEnabled.value;
      shareModeEnabled.value = false;
      const search = controlledSearch();
      const controller = new CommandPaletteController(window, search.providers);
      try {
        controller.updateSearch("lifecycle-pending-query");
        await search.started(0);
        const baseline = controller.state.filteredCommands();
        controller.destroy();
        search.history[0].resolve([
          command("stale-history", "history-suggestions"),
        ]);
        search.bookmarks[0].resolve([
          command("stale-bookmark", "bookmark-suggestions"),
        ]);
        await Promise.resolve();
        await Promise.resolve();
        assertEquals(
          controller.state.filteredCommands(),
          baseline,
          "in-flight results cannot commit after destroy",
        );
      } finally {
        controller.destroy();
        shareModeEnabled.value = savedShareMode;
      }
    },
  }, {
    name: "a hidden search cannot commit into a later identical query",
    async fn() {
      const savedShareMode = shareModeEnabled.value;
      shareModeEnabled.value = false;
      const search = controlledSearch();
      const controller = new CommandPaletteController(window, search.providers);
      try {
        controller.updateSearch("lifecycle-same-query");
        await search.started(0);
        controller.hidePalette();
        controller.updateSearch("lifecycle-same-query");
        await search.started(1);
        const baseline = controller.state.filteredCommands();
        search.history[0].resolve([
          command("stale-history", "history-suggestions"),
        ]);
        search.bookmarks[0].resolve([
          command("stale-bookmark", "bookmark-suggestions"),
        ]);
        await Promise.resolve();
        await Promise.resolve();
        assertEquals(
          controller.state.filteredCommands(),
          baseline,
          "same query text does not revive an earlier search generation",
        );
        search.history[1].resolve([
          command("current-history", "history-suggestions"),
        ]);
        search.bookmarks[1].resolve([
          command("current-bookmark", "bookmark-suggestions"),
        ]);
        await Promise.resolve();
        await Promise.resolve();
        const ids = controller.state.filteredCommands().map((item) => item.id);
        assert(
          ids.includes("current-history") && ids.includes("current-bookmark"),
          "the active generation still accepts both result types",
        );
      } finally {
        controller.destroy();
        shareModeEnabled.value = savedShareMode;
      }
    },
  }]);
}
