/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createRootHMR, render } from "@nora/solid-xul";
import { Modal } from "./components/modal.tsx";
import style from "./style.css?inline";
import { ModalManager } from "./modalManager.tsx";

export class ModalElement {
  private static instance: ModalElement;
  private initialized: boolean = false;

  private constructor() {
    // Private constructor for singleton
  }

  public static getInstance(): ModalElement {
    if (!ModalElement.instance) {
      ModalElement.instance = new ModalElement();
    }
    return ModalElement.instance;
  }

  public initializeModal(): void {
    if (this.initialized) return;

    const ModalManagerInstance = new ModalManager();
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
                ModalManagerInstance.handleBackdropClick(e)}
            />
          ),
          targetParent,
        );
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
