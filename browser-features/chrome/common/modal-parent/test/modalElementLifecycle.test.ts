// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { createRoot } from "@nora/preact-xul/lifetime";
import { ModalElement } from "../modalElement.tsx";
import { assertEquals, runTests } from "../../../test/utils/test_harness.ts";

export async function runAllTests(): Promise<void> {
  await runTests("modalElementLifecycle.test.ts", [{
    name: "the same modal element can mount again after its owner is disposed",
    fn() {
      // Keep this instance and its DOM detached from the running modal service.
      const ConstructModal = ModalElement as unknown as new () => ModalElement;
      const modal = new ConstructModal();
      const parent = document.createElement("div");
      const head = document.createElement("head");
      const manager = { hide() {}, handleBackdropClick() {} };
      for (let attempt = 0; attempt < 2; attempt++) {
        const dispose = createRoot((cleanup) => {
          modal.initializeModal(manager, parent, head);
          return cleanup;
        });
        try {
          assertEquals(
            parent.querySelectorAll("#modal-parent-container").length,
            1,
            "one modal root",
          );
          assertEquals(
            head.querySelectorAll("style").length,
            1,
            "one style root",
          );
          modal.initializeModal(manager, parent, head);
          assertEquals(
            parent.querySelectorAll("#modal-parent-container").length,
            1,
            "repeated init is idempotent",
          );
        } finally {
          dispose();
        }
        assertEquals(
          parent.childNodes.length,
          0,
          "modal root and portal unmount",
        );
        assertEquals(head.childNodes.length, 0, "owned styles unmount");
      }
    },
  }]);
}
