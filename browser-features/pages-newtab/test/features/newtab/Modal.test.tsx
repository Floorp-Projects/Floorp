// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import { createRoot } from "react-dom/client";
import { flushSync } from "react-dom";
import { StandardUIProvider } from "../../../../../libs/ui/standard-provider.tsx";
import { Modal } from "../../../src/features/newtab/ui.tsx";
import {
  assert,
  assertEquals,
  runTests,
  type TestCase,
} from "../../../../chrome/test/utils/test_harness.ts";

async function until(condition: () => boolean, message: string): Promise<void> {
  const deadline = performance.now() + 3000;
  while (!condition() && performance.now() < deadline)
    await new Promise((resolve) => setTimeout(resolve, 20));
  assert(condition(), message);
}

const tests: TestCase[] = [
  {
    name: "Native dialog cancellation finishes before React closes state and restores the opener",
    fn: async () => {
      const previousFocus = document.activeElement;
      const container = document.createElementNS(
        "http://www.w3.org/1999/xhtml",
        "div",
      );
      container.style.cssText = "position:fixed;inset:0;z-index:2147483647";
      document.documentElement.appendChild(container);
      const root = createRoot(container);
      let requestingClose = false;
      let closedDuringNativeRequest = false;
      let closeCount = 0;
      const render = (open: boolean) =>
        root.render(
          <StandardUIProvider>
            <button
              id="modal-test-opener"
              type="button"
              onClick={() => render(true)}
            >
              Create folder
            </button>
            {open && (
              <Modal
                title="Create folder"
                closeLabel="Close"
                close={() => {
                  closedDuringNativeRequest ||= requestingClose;
                  closeCount++;
                  render(false);
                }}
              >
                <input aria-label="Folder name" data-initial-focus />
              </Modal>
            )}
          </StandardUIProvider>,
        );
      try {
        flushSync(() => render(false));
        const opener =
          container.querySelector<HTMLButtonElement>("#modal-test-opener");
        assert(opener, "The dialog has an opener");
        for (let attempt = 1; attempt <= 3; attempt++) {
          opener.focus();
          flushSync(() => opener.click());
          const dialog = container.querySelector<HTMLDialogElement>("dialog");
          assert(dialog, "The modal is mounted");
          await until(
            () =>
              dialog.open &&
              document.activeElement === dialog.querySelector("input"),
            "The native modal opens and focuses its input",
          );
          assert(
            typeof dialog.requestClose === "function",
            "Gecko supports native dialog cancellation",
          );
          requestingClose = true;
          try {
            dialog.requestClose();
          } finally {
            requestingClose = false;
          }
          assertEquals(
            closedDuringNativeRequest,
            false,
            "React does not close state while native cancel is being dispatched",
          );
          await until(
            () => !dialog.isConnected && document.activeElement === opener,
            "The native close event unmounts the modal and preserves opener focus",
          );
          assertEquals(
            closeCount,
            attempt,
            "Each native cancellation closes React state exactly once",
          );
        }
      } finally {
        flushSync(() => root.unmount());
        container.remove();
        if (previousFocus instanceof HTMLElement && previousFocus.isConnected)
          previousFocus.focus({ preventScroll: true });
      }
    },
  },
];

export async function runAllTests(): Promise<void> {
  await runTests("Modal.test.tsx", tests);
}
