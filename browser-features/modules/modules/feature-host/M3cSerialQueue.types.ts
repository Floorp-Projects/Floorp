// SPDX-License-Identifier: MPL-2.0

export type M3cPreparedOperation<T> = () => Promise<T>;

export type M3cPreparation<T> =
  | { readonly operation: M3cPreparedOperation<T> }
  | { readonly error: unknown };
