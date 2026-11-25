// SPDX-License-Identifier: MPL-2.0

import type {
  M3cPreparation,
  M3cPreparedOperation,
} from "./M3cSerialQueue.types.ts";

export class M3cSerialQueue {
  readonly #capacity: number;
  #accepting = true;
  #operationCount = 0;
  #queue: Promise<void> = Promise.resolve();
  #closePromise: Promise<void> | null = null;

  constructor(capacity = 16) {
    this.#capacity = capacity;
  }

  get pendingCount(): number {
    return this.#operationCount;
  }

  schedule<T>(prepare: () => M3cPreparedOperation<T>): Promise<T> {
    if (!this.#accepting) return Promise.reject(new Error("facade-closed"));
    if (this.#operationCount >= this.#capacity) {
      return Promise.reject(new Error("busy"));
    }
    this.#operationCount += 1;

    let resolvePreparation!: (preparation: M3cPreparation<T>) => void;
    const preparation = new Promise<M3cPreparation<T>>((resolve) => {
      resolvePreparation = resolve;
    });
    const result = this.#queue.then(async () => {
      const prepared = await preparation;
      if ("error" in prepared) throw prepared.error;
      return await prepared.operation();
    });
    this.#queue = result.then(() => undefined, () => undefined).finally(() => {
      this.#operationCount -= 1;
    });

    try {
      resolvePreparation({ operation: prepare() });
    } catch (error) {
      resolvePreparation({ error });
    }
    return result;
  }

  close(close: () => Promise<void>): Promise<void> {
    if (this.#closePromise) return this.#closePromise;
    this.#accepting = false;
    this.#closePromise = this.#queue.then(close);
    return this.#closePromise;
  }
}
