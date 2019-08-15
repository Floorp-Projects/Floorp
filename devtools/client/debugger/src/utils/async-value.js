/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

export type FulfilledValue<+T> = {|
  state: "fulfilled",
  +value: T,
|};
export type RejectedValue = {|
  state: "rejected",
  value: mixed,
|};
export type PendingValue = {|
  state: "pending",
|};
export type SettledValue<+T> = FulfilledValue<T> | RejectedValue;
export type AsyncValue<+T> = SettledValue<T> | PendingValue;

export function pending(): PendingValue {
  return { state: "pending" };
}
export function fulfilled<+T>(value: T): FulfilledValue<T> {
  return { state: "fulfilled", value };
}
export function rejected(value: mixed): RejectedValue {
  return { state: "rejected", value };
}

export function asSettled<T>(
  value: AsyncValue<T> | null
): SettledValue<T> | null {
  return value && value.state !== "pending" ? value : null;
}

export function isPending(value: AsyncValue<mixed>): boolean %checks {
  return value.state === "pending";
}
export function isFulfilled(value: AsyncValue<mixed>): boolean %checks {
  return value.state === "fulfilled";
}
export function isRejected(value: AsyncValue<mixed>): boolean %checks {
  return value.state === "rejected";
}
