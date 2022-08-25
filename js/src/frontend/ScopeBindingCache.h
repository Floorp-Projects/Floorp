/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_ScopeBindingCache_h
#define frontend_ScopeBindingCache_h

namespace js {
namespace frontend {

class ScopeBindingCache {
};

class NoScopeBindingCache final : public ScopeBindingCache {
};

class StencilScopeBindingCache final : public ScopeBindingCache {
};

class RuntimeScopeBindingCache final : public ScopeBindingCache {
};

}  // namespace frontend
}  // namespace js

#endif  // frontend_ScopeBindingCache_h
