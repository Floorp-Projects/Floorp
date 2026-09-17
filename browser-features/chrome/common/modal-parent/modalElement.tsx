/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createRootHMR, render } from "@nora/solid-xul";
import { onCleanup } from "solid-js";
import { Modal } from "./components/modal.tsx";
import style from "./style.css?inline";
import { ModalManager } from "./modalManager.tsx";

export function attachModalBackdropListener(
  targetParent: HTMLElement,
  getModalManager: () => Pick<ModalManager, "hide"> | null,
): () => void {
  const listener = (event: MouseEvent) => {
    const target = event.target;
    if (
      target instanceof HTMLElement && target.id === "modal-parent-container"
    ) {
      getModalManager()?.hide("backdrop");
    }
  };
  targetParent.addEventListener("click", listener);
  return () => targetParent.removeEventListener("click", listener);
}

export class ModalElement {
  private static instance: ModalElement;
  private initialized: boolean = false;
  private currentManager: ModalManager | null = null;

  private constructor() {
    // Private constructor for singleton
  }

  public static getInstance(): ModalElement {
    if (!ModalElement.instance) {
      ModalElement.instance = new ModalElement();
    }
    return ModalElement.instance;
  }

  public initializeModal(modalManager: ModalManager): void {
    this.currentManager = modalManager;
    if (this.initialized) return;

    const head = document?.head;
    if (!head) {
      console.warn(
        "[ModalElement] document.head is unavailable; skip modal style injection.",
      );
      return;
    }

    const targetParent = ModalManager.parentElement;
    if (!targetParent) {
      console.error(
        "[ModalElement] Modal parent element not found; modal cannot be initialized.",
      );
      return;
    }

    createRootHMR(() => {
      try {
        render(() => <style>{style}</style>, head);
      } catch (error) {
        const reason = error instanceof Error
          ? error
          : new Error(String(error));
        console.error("[ModalElement] Failed to render modal styles.", reason);
      }
    }, import.meta.hot);

    createRootHMR(() => {
      try {
        render(
          () => (
            <Modal
              targetParent={targetParent}
              onBackdropClick={(e) =>
                this.currentManager?.handleBackdropClick(e)}
            />
          ),
          targetParent,
        );
        const detachBackdrop = attachModalBackdropListener(
          targetParent,
          () => this.currentManager,
        );
        onCleanup(detachBackdrop);
      } catch (error) {
        const reason = error instanceof Error
          ? error
          : new Error(String(error));
        console.error("[ModalElement] Failed to render modal root.", reason);
      }
    }, import.meta.hot);

    this.initialized = true;
  }
}
