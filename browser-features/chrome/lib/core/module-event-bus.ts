// SPDX-License-Identifier: MPL-2.0

/**
 * Module Event Bus
 *
 * DOP-style per-module event bus with a tuple-based Result type.
 * Provides safe method dispatch across module boundaries.
 */

// ============================================================================
// Result Type (Tuple-based)
// ============================================================================

import { R } from "@mobily/ts-belt";

// Compatibility thin wrapper: previously the library used a tuple-based
// Result type. Internally we now prefer ts-belt's `R.Result`. Export a few
// helper functions matching the previous API backed by ts-belt.

export type Result<T extends {}, E extends {} = Error> = R.Result<T, E>;

export const ok = <T extends {}>(value: T): R.Ok<T> => R.Ok(value);

export const err = <E extends {} = Error>(error: string | E): R.Error<E> =>
  R.Error(
    (error instanceof Error ? error : new Error(String(error))) as unknown as E,
  );

export const isOk = <T extends {}, E extends {} = Error>(
  result: Result<T, E>,
): result is R.Ok<T> => R.isOk(result);

export const isErr = <T extends {}, E extends {} = Error>(
  result: Result<T, E>,
): result is R.Error<E> => R.isError(result);

export const unwrap = <T extends {}, E extends {} = Error>(
  result: Result<T, E>,
): T => R.getExn(result);

export const unwrapOr = <T extends {}, E extends {} = Error>(
  result: Result<T, E>,
  defaultValue: T,
): T => R.getWithDefault(result, defaultValue);

export const mapResult = <T extends {}, U extends {}, E extends {} = Error>(
  result: Result<T, E>,
  fn: (v: T) => U,
): Result<U, E> => R.map(result, fn) as Result<U, E>;

// ============================================================================
// Module Event Bus
// ============================================================================

// eslint-disable-next-line @typescript-eslint/no-explicit-any
type EventMethods = Record<string, (...args: any[]) => unknown>;

const _registry: Map<string, EventMethods> = new Map();

export const registerModuleEventBus = (
  name: string,
  methods: EventMethods,
): void => {
  if (_registry.has(name)) {
    console.warn(
      `[ModuleEventBus] Module ${name} is already registered, replacing`,
    );
  }
  _registry.set(name, methods);
};

export const unregisterModuleEventBus = (name: string): void => {
  _registry.delete(name);
};

export const isModuleRegistered = (name: string): boolean =>
  _registry.has(name);

// eslint-disable-next-line @typescript-eslint/no-explicit-any
export const getModuleEventBus = (name: string): Record<string, (...args: any[]) => Promise<any>> => {
  // eslint-disable-next-line @typescript-eslint/no-explicit-any
  return new Proxy({} as any, {
    get(_target, prop: string) {
      // eslint-disable-next-line @typescript-eslint/no-explicit-any
      return async (...args: any[]) => {
        const module = _registry.get(name);
        if (!module || !(prop in module)) {
          return ok({} as never);
        }
        try {
          const result = await module[prop](...args);
          return ok(result as {});
        } catch (e) {
          return err(e instanceof Error ? e : new Error(String(e)));
        }
      };
    },
  });
};
