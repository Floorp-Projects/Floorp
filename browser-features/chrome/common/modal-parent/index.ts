/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { ModalManager } from "./modalManager.tsx";
import { ModalElement } from "./modalElement.tsx";
import type { ModalSize } from "./data/data.ts";
import { TForm, TFormResult } from "./utils/type.ts";

@noraComponent(import.meta.hot)
export default class ModalParent extends NoraComponentBase {
  private static instance: ModalParent;
  private modalManager: ModalManager | null = null;

  public static getInstance(): ModalParent {
    if (!ModalParent.instance) {
      ModalParent.instance = new ModalParent();
    }
    return ModalParent.instance;
  }

  constructor() {
    super();
  }

  init(): void {
    this.modalManager = new ModalManager();
    ModalElement.getInstance().initializeModal();
  }

  public async showNoraModal(
    forms: TForm,
    options: { width: number; height: number },
    callback: (result: TFormResult) => void,
  ): Promise<void> {
    if (!this.modalManager) {
      throw new Error("ModalManager not initialized. Call init() first.");
    }
    const result = await this.modalManager.show(forms, options);
    callback(result as TFormResult);
    this.modalManager.hide();
  }

  public hideNoraModal(): void {
    if (!this.modalManager) {
      throw new Error("ModalManager not initialized. Call init() first.");
    }
    this.modalManager.hide();
  }

  public setModalSize(newSize: ModalSize): void {
    if (!this.modalManager) {
      throw new Error("ModalManager not initialized. Call init() first.");
    }
    this.modalManager.setModalSize(newSize);
  }
}
