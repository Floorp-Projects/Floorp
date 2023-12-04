import { FFIError, ICU4XError } from "icu4x";

export type Result<T> =
  | { ok: true; value: T }
  | { ok: false; error: FFIError<ICU4XError> };


export function Ok<T>(value: T): Result<T> {
    return { ok: true, value };
}

export function Err<T>(error: FFIError<ICU4XError>): Result<T> {
    return { ok: false, error };
}

// Convert exceptions into a `Result`.
export function result<T>(fn: () => T): Result<T> {
    try {
        return Ok(fn());
    } catch (e) {
        return Err(e);
    }
}

// Convert a `Result` into an exception.
export function unwrap<T>(result: Result<T>): T {
    switch (result.ok) {
        case true: return result.value;
        case false: throw result.error;
    }
}
