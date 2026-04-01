/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

export function assertEquals<T>(actual: T, expected: T, message: string): void {
  if (actual !== expected) {
    throw new Error(
      `${message} (expected: ${String(expected)}, actual: ${String(actual)})`,
    );
  }
}

export async function assertRejects(
  fn: () => Promise<unknown>,
  expectedMessagePart: string,
  message: string,
): Promise<void> {
  try {
    await fn();
  } catch (error) {
    const actual = error instanceof Error ? error.message : String(error);
    assert(
      actual.includes(expectedMessagePart),
      `${message} (expected error containing: "${expectedMessagePart}", actual: "${actual}")`,
    );
    return;
  }

  throw new Error(`${message} (expected rejection)`);
}
