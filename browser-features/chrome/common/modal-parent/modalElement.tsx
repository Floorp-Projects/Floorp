/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { safeRender } from "@nora/preact-xul";
import { addDisposer, createRootHMR } from "@nora/preact-xul/lifetime";
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
  private currentManager:
    | Pick<ModalManager, "hide" | "handleBackdropClick">
    | null = null;

  private constructor() {
    // Private constructor for singleton
  }

  public static getInstance(): ModalElement {
    if (!ModalElement.instance) {
      ModalElement.instance = new ModalElement();
    }
    return ModalElement.instance;
  }

  public initializeModal(
    modalManager: Pick<ModalManager, "hide" | "handleBackdropClick">,
    targetParent = ModalManager.parentElement,
    head = document?.head,
  ): void {
    this.currentManager = modalManager;
    if (this.initialized) return;

    if (!head) {
      console.warn(
        "[ModalElement] document.head is unavailable; skip modal style injection.",
      );
      return;
    }

    if (!targetParent) {
      console.error(
        "[ModalElement] Modal parent element not found; modal cannot be initialized.",
      );
      return;
    }

    try {
      createRootHMR(() => {
        safeRender(<style>{style}</style>, head);
        safeRender(
          <Modal
            targetParent={targetParent}
            onBackdropClick={(e) => this.currentManager?.handleBackdropClick(e)}
          />,
          targetParent,
        );
        const detachBackdrop = attachModalBackdropListener(
          targetParent,
          () => this.currentManager,
        );
        addDisposer(detachBackdrop);
        this.initialized = true;
        addDisposer(() => {
          this.initialized = false;
          this.currentManager = null;
        });
      }, import.meta.hot);
    } catch (error) {
      // createRootHMR disposes partial styles/UI before a later retry.
      console.error("[ModalElement] Failed to render modal root.", error);
    }
  }
}
