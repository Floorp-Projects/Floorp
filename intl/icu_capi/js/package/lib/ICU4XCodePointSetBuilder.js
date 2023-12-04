import wasm from "./diplomat-wasm.mjs"
import * as diplomatRuntime from "./diplomat-runtime.js"
import { ICU4XCodePointSetData } from "./ICU4XCodePointSetData.js"

const ICU4XCodePointSetBuilder_box_destroy_registry = new FinalizationRegistry(underlying => {
  wasm.ICU4XCodePointSetBuilder_destroy(underlying);
});

export class ICU4XCodePointSetBuilder {
  #lifetimeEdges = [];
  constructor(underlying, owned, edges) {
    this.underlying = underlying;
    this.#lifetimeEdges.push(...edges);
    if (owned) {
      ICU4XCodePointSetBuilder_box_destroy_registry.register(this, underlying);
    }
  }

  static create() {
    return new ICU4XCodePointSetBuilder(wasm.ICU4XCodePointSetBuilder_create(), true, []);
  }

  build() {
    return new ICU4XCodePointSetData(wasm.ICU4XCodePointSetBuilder_build(this.underlying), true, []);
  }

  complement() {
    wasm.ICU4XCodePointSetBuilder_complement(this.underlying);
  }

  is_empty() {
    return wasm.ICU4XCodePointSetBuilder_is_empty(this.underlying);
  }

  add_char(arg_ch) {
    wasm.ICU4XCodePointSetBuilder_add_char(this.underlying, diplomatRuntime.extractCodePoint(arg_ch, 'arg_ch'));
  }

  add_u32(arg_ch) {
    wasm.ICU4XCodePointSetBuilder_add_u32(this.underlying, arg_ch);
  }

  add_inclusive_range(arg_start, arg_end) {
    wasm.ICU4XCodePointSetBuilder_add_inclusive_range(this.underlying, diplomatRuntime.extractCodePoint(arg_start, 'arg_start'), diplomatRuntime.extractCodePoint(arg_end, 'arg_end'));
  }

  add_inclusive_range_u32(arg_start, arg_end) {
    wasm.ICU4XCodePointSetBuilder_add_inclusive_range_u32(this.underlying, arg_start, arg_end);
  }

  add_set(arg_data) {
    wasm.ICU4XCodePointSetBuilder_add_set(this.underlying, arg_data.underlying);
  }

  remove_char(arg_ch) {
    wasm.ICU4XCodePointSetBuilder_remove_char(this.underlying, diplomatRuntime.extractCodePoint(arg_ch, 'arg_ch'));
  }

  remove_inclusive_range(arg_start, arg_end) {
    wasm.ICU4XCodePointSetBuilder_remove_inclusive_range(this.underlying, diplomatRuntime.extractCodePoint(arg_start, 'arg_start'), diplomatRuntime.extractCodePoint(arg_end, 'arg_end'));
  }

  remove_set(arg_data) {
    wasm.ICU4XCodePointSetBuilder_remove_set(this.underlying, arg_data.underlying);
  }

  retain_char(arg_ch) {
    wasm.ICU4XCodePointSetBuilder_retain_char(this.underlying, diplomatRuntime.extractCodePoint(arg_ch, 'arg_ch'));
  }

  retain_inclusive_range(arg_start, arg_end) {
    wasm.ICU4XCodePointSetBuilder_retain_inclusive_range(this.underlying, diplomatRuntime.extractCodePoint(arg_start, 'arg_start'), diplomatRuntime.extractCodePoint(arg_end, 'arg_end'));
  }

  retain_set(arg_data) {
    wasm.ICU4XCodePointSetBuilder_retain_set(this.underlying, arg_data.underlying);
  }

  complement_char(arg_ch) {
    wasm.ICU4XCodePointSetBuilder_complement_char(this.underlying, diplomatRuntime.extractCodePoint(arg_ch, 'arg_ch'));
  }

  complement_inclusive_range(arg_start, arg_end) {
    wasm.ICU4XCodePointSetBuilder_complement_inclusive_range(this.underlying, diplomatRuntime.extractCodePoint(arg_start, 'arg_start'), diplomatRuntime.extractCodePoint(arg_end, 'arg_end'));
  }

  complement_set(arg_data) {
    wasm.ICU4XCodePointSetBuilder_complement_set(this.underlying, arg_data.underlying);
  }
}
