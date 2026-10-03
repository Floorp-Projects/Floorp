// SPDX-License-Identifier: MPL-2.0

export interface TestRunSharedData {
  has(key: string): boolean;
  set(key: string, value: unknown): void;
}
