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
    createRootHMR(() => {
      render(() => <style>{style}</style>, document?.head);
    }, import.meta.hot);

    createRootHMR(() => {
      render(
        () => (
          <Modal
            targetParent={ModalManager.parentElement}
            onBackdropClick={(e) => ModalManagerInstance.handleBackdropClick(e)}
          />
        ),
        ModalManager.parentElement,
      );
    }, import.meta.hot);

    this.initialized = true;
  }
}
