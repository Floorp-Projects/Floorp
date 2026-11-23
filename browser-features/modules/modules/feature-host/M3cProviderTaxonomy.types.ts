// SPDX-License-Identifier: MPL-2.0

export type M3cProviderInfrastructureReason =
  | "cache-io"
  | "unsafe-layout"
  | "cache-contract"
  | "unsupported-crypto"
  | "unexpected";

export type M3cProviderFailureClassification =
  | { readonly kind: "candidate-unavailable" }
  | {
    readonly kind: "infrastructure";
    readonly reason: M3cProviderInfrastructureReason;
  };
