/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

export function pending() {
  return { state: "pending" };
}
export function fulfilled(value) {
  return { state: "fulfilled", value };
}
export function rejected(value) {
  return { state: "rejected", value };
}

export function asSettled(value) {
  return value && value.state !== "pending" ? value : null;
}

export function isPending(value) {
  return value.state === "pending";
}
export function isFulfilled(value) {
  return value.state === "fulfilled";
}
export function isRejected(value) {
  return value.state === "rejected";
}
