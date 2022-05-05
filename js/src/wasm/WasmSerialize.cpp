/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "wasm/WasmSerialize.h"

#include "mozilla/Maybe.h"
#include <cstdint>
#include <type_traits>

using namespace js;
using namespace js::wasm;

namespace js {
namespace wasm {

// A pointer is not cacheable POD
static_assert(!is_cacheable_pod<const uint8_t*>);

// A non-fixed sized array is not cacheable POD
static_assert(!is_cacheable_pod<uint8_t[]>);

// Cacheable POD is not inherited
struct PodBase {};
WASM_DECLARE_CACHEABLE_POD(PodBase);
struct InheritPodBase : public PodBase {};
static_assert(!is_cacheable_pod<InheritPodBase>);

}  // namespace wasm
}  // namespace js
