// SPDX-License-Identifier: MPL-2.0

interface ExplicitTabUserContextOperation {
  id: symbol;
  userContextId: number;
  consumed: boolean;
}

/**
 * Window-local, synchronous one-shot operations used to associate a TabOpen
 * event with an explicit container choice.
 */
export class ExplicitTabUserContextOperations {
  private operations: ExplicitTabUserContextOperation[] = [];

  run<T>(userContextId: number, openTab: () => T): T {
    if (!Number.isSafeInteger(userContextId) || userContextId < 0) {
      throw new TypeError("userContextId must be a non-negative integer");
    }

    const operation: ExplicitTabUserContextOperation = {
      id: Symbol("explicit-tab-user-context"),
      userContextId,
      consumed: false,
    };
    this.operations.push(operation);

    try {
      return openTab();
    } finally {
      const operationIndex = this.operations.findIndex(
        (candidate) => candidate.id === operation.id,
      );
      if (operationIndex >= 0) {
        this.operations.splice(operationIndex, 1);
      }
    }
  }

  consumeNext(): number | null {
    const operation = this.operations.at(-1);
    if (!operation || operation.consumed) {
      return null;
    }

    operation.consumed = true;
    return operation.userContextId;
  }
}
