/**
 * An error that occurred in Rust.
 */
export class FFIError<E> extends Error {
    error_value: E;
}

export type u8 = number;
export type i8 = number;
export type u16 = number;
export type i16 = number;
export type u32 = number;
export type i32 = number;
export type u64 = bigint;
export type i64 = bigint;
export type usize = number;
export type isize = number;
export type f32 = number;
export type f64 = number;
export type char = string;
