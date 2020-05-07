/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_UsedNameTracker_h
#define frontend_UsedNameTracker_h

#include "mozilla/Attributes.h"

#include "js/AllocPolicy.h"
#include "js/HashTable.h"
#include "js/Vector.h"

#include "vm/StringType.h"

namespace js {
namespace frontend {

// A data structure for tracking used names per parsing session in order to
// compute which bindings are closed over. Scripts and scopes are numbered
// monotonically in textual order and name uses are tracked by lists of
// (script id, scope id) pairs of their use sites.
//
// Intuitively, in a pair (P,S), P tracks the most nested function that has a
// use of u, and S tracks the most nested scope that is still being parsed.
//
// P is used to answer the question "is u used by a nested function?"
// S is used to answer the question "is u used in any scopes currently being
//                                   parsed?"
//
// The algorithm:
//
// Let Used by a map of names to lists.
//
// 1. Number all scopes in monotonic increasing order in textual order.
// 2. Number all scripts in monotonic increasing order in textual order.
// 3. When an identifier u is used in scope numbered S in script numbered P,
//    and u is found in Used,
//   a. Append (P,S) to Used[u].
//   b. Otherwise, assign the the list [(P,S)] to Used[u].
// 4. When we finish parsing a scope S in script P, for each declared name d in
//    Declared(S):
//   a. If d is found in Used, mark d as closed over if there is a value
//     (P_d, S_d) in Used[d] such that P_d > P and S_d > S.
//   b. Remove all values (P_d, S_d) in Used[d] such that S_d are >= S.
//
// Steps 1 and 2 are implemented by UsedNameTracker::next{Script,Scope}Id.
// Step 3 is implemented by UsedNameTracker::noteUsedInScope.
// Step 4 is implemented by UsedNameTracker::noteBoundInScope and
// Parser::propagateFreeNamesAndMarkClosedOverBindings.
class UsedNameTracker {
 public:
  struct Use {
    uint32_t scriptId;
    uint32_t scopeId;
  };

  class UsedNameInfo {
    friend class UsedNameTracker;

    Vector<Use, 6> uses_;

    void resetToScope(uint32_t scriptId, uint32_t scopeId);

   public:
    explicit UsedNameInfo(JSContext* cx) : uses_(cx) {}

    UsedNameInfo(UsedNameInfo&& other) : uses_(std::move(other.uses_)) {}

    bool noteUsedInScope(uint32_t scriptId, uint32_t scopeId) {
      if (uses_.empty() || uses_.back().scopeId < scopeId) {
        return uses_.append(Use{scriptId, scopeId});
      }
      return true;
    }

    void noteBoundInScope(uint32_t scriptId, uint32_t scopeId,
                          bool* closedOver) {
      *closedOver = false;
      while (!uses_.empty()) {
        Use& innermost = uses_.back();
        if (innermost.scopeId < scopeId) {
          break;
        }
        if (innermost.scriptId > scriptId) {
          *closedOver = true;
        }
        uses_.popBack();
      }
    }

    bool isUsedInScript(uint32_t scriptId) const {
      return !uses_.empty() && uses_.back().scriptId >= scriptId;
    }
  };

  using UsedNameMap = HashMap<JSAtom*, UsedNameInfo, DefaultHasher<JSAtom*>>;

 private:
  // The map of names to chains of uses.
  UsedNameMap map_;

  // Monotonically increasing id for all nested scripts.
  uint32_t scriptCounter_;

  // Monotonically increasing id for all nested scopes.
  uint32_t scopeCounter_;

 public:
  explicit UsedNameTracker(JSContext* cx)
      : map_(cx), scriptCounter_(0), scopeCounter_(0) {}

  uint32_t nextScriptId() {
    MOZ_ASSERT(scriptCounter_ != UINT32_MAX,
               "ParseContext::Scope::init should have prevented wraparound");
    return scriptCounter_++;
  }

  uint32_t nextScopeId() {
    MOZ_ASSERT(scopeCounter_ != UINT32_MAX);
    return scopeCounter_++;
  }

  UsedNameMap::Ptr lookup(JSAtom* name) const { return map_.lookup(name); }

  MOZ_MUST_USE bool noteUse(JSContext* cx, JSAtom* name, uint32_t scriptId,
                            uint32_t scopeId);

  struct RewindToken {
   private:
    friend class UsedNameTracker;
    uint32_t scriptId;
    uint32_t scopeId;
  };

  RewindToken getRewindToken() const {
    RewindToken token;
    token.scriptId = scriptCounter_;
    token.scopeId = scopeCounter_;
    return token;
  }

  // Resets state so that scriptId and scopeId are the innermost script and
  // scope, respectively. Used for rewinding state on syntax parse failure.
  void rewind(RewindToken token);
};

}  // namespace frontend
}  // namespace js

#endif
