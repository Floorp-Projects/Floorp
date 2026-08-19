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
import type { TForm, TFormResult } from "./utils/type.ts";

@noraComponent(import.meta.hot)
export default class ModalParent extends NoraComponentBase {
  private static instance: ModalParent;
  // NoraComponentBase invokes init() during super(). This declaration must not
  // emit a class-field initializer that would overwrite the manager afterward.
  declare private modalManager: ModalManager | null;

  public static getInstance(): ModalParent {
    if (!ModalParent.instance) {
      ModalParent.instance = new ModalParent();
    }
    return ModalParent.instance;
  }

  constructor() {
    super();
    // The loader may construct the decorated component before a caller uses
    // getInstance(). Preserve that rendered owner as the public singleton.
    if (!ModalParent.instance) {
      ModalParent.instance = this;
    }
  }

  init(): void {
    if (!this.modalManager) {
      this.modalManager = new ModalManager();
    }
    ModalElement.getInstance().initializeModal(this.modalManager);
  }

  public async showNoraModal(
    forms: TForm,
    options: { width: number; height: number },
    callback: (result: TFormResult | null) => void,
  ): Promise<TFormResult | null> {
    if (!this.modalManager) {
      throw new Error("ModalManager not initialized. Call init() first.");
    }
    const result = await this.modalManager.show(forms, options);
    callback(result);
    return result;
  }

  public hideNoraModal(): void {
    if (!this.modalManager) {
      throw new Error("ModalManager not initialized. Call init() first.");
    }
    this.modalManager.hide("hide");
  }

  public setModalSize(newSize: ModalSize): void {
    if (!this.modalManager) {
      throw new Error("ModalManager not initialized. Call init() first.");
    }
    this.modalManager.setModalSize(newSize);
  }
}
