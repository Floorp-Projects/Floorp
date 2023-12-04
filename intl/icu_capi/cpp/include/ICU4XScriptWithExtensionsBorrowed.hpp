#ifndef ICU4XScriptWithExtensionsBorrowed_HPP
#define ICU4XScriptWithExtensionsBorrowed_HPP
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <algorithm>
#include <memory>
#include <variant>
#include <optional>
#include "diplomat_runtime.hpp"

#include "ICU4XScriptWithExtensionsBorrowed.h"

class ICU4XScriptExtensionsSet;
class ICU4XCodePointSetData;

/**
 * A destruction policy for using ICU4XScriptWithExtensionsBorrowed with std::unique_ptr.
 */
struct ICU4XScriptWithExtensionsBorrowedDeleter {
  void operator()(capi::ICU4XScriptWithExtensionsBorrowed* l) const noexcept {
    capi::ICU4XScriptWithExtensionsBorrowed_destroy(l);
  }
};

/**
 * A slightly faster ICU4XScriptWithExtensions object
 * 
 * See the [Rust documentation for `ScriptWithExtensionsBorrowed`](https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html) for more information.
 */
class ICU4XScriptWithExtensionsBorrowed {
 public:

  /**
   * Get the Script property value for a code point
   * 
   * See the [Rust documentation for `get_script_val`](https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_val) for more information.
   */
  uint16_t get_script_val(uint32_t code_point) const;

  /**
   * Get the Script property value for a code point
   * 
   * See the [Rust documentation for `get_script_extensions_val`](https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_val) for more information.
   * 
   * Lifetimes: `this` must live at least as long as the output.
   */
  ICU4XScriptExtensionsSet get_script_extensions_val(uint32_t code_point) const;

  /**
   * Check if the Script_Extensions property of the given code point covers the given script
   * 
   * See the [Rust documentation for `has_script`](https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.has_script) for more information.
   */
  bool has_script(uint32_t code_point, uint16_t script) const;

  /**
   * Build the CodePointSetData corresponding to a codepoints matching a particular script
   * in their Script_Extensions
   * 
   * See the [Rust documentation for `get_script_extensions_set`](https://docs.rs/icu/latest/icu/properties/script/struct.ScriptWithExtensionsBorrowed.html#method.get_script_extensions_set) for more information.
   */
  ICU4XCodePointSetData get_script_extensions_set(uint16_t script) const;
  inline const capi::ICU4XScriptWithExtensionsBorrowed* AsFFI() const { return this->inner.get(); }
  inline capi::ICU4XScriptWithExtensionsBorrowed* AsFFIMut() { return this->inner.get(); }
  inline ICU4XScriptWithExtensionsBorrowed(capi::ICU4XScriptWithExtensionsBorrowed* i) : inner(i) {}
  ICU4XScriptWithExtensionsBorrowed() = default;
  ICU4XScriptWithExtensionsBorrowed(ICU4XScriptWithExtensionsBorrowed&&) noexcept = default;
  ICU4XScriptWithExtensionsBorrowed& operator=(ICU4XScriptWithExtensionsBorrowed&& other) noexcept = default;
 private:
  std::unique_ptr<capi::ICU4XScriptWithExtensionsBorrowed, ICU4XScriptWithExtensionsBorrowedDeleter> inner;
};

#include "ICU4XScriptExtensionsSet.hpp"
#include "ICU4XCodePointSetData.hpp"

inline uint16_t ICU4XScriptWithExtensionsBorrowed::get_script_val(uint32_t code_point) const {
  return capi::ICU4XScriptWithExtensionsBorrowed_get_script_val(this->inner.get(), code_point);
}
inline ICU4XScriptExtensionsSet ICU4XScriptWithExtensionsBorrowed::get_script_extensions_val(uint32_t code_point) const {
  return ICU4XScriptExtensionsSet(capi::ICU4XScriptWithExtensionsBorrowed_get_script_extensions_val(this->inner.get(), code_point));
}
inline bool ICU4XScriptWithExtensionsBorrowed::has_script(uint32_t code_point, uint16_t script) const {
  return capi::ICU4XScriptWithExtensionsBorrowed_has_script(this->inner.get(), code_point, script);
}
inline ICU4XCodePointSetData ICU4XScriptWithExtensionsBorrowed::get_script_extensions_set(uint16_t script) const {
  return ICU4XCodePointSetData(capi::ICU4XScriptWithExtensionsBorrowed_get_script_extensions_set(this->inner.get(), script));
}
#endif
