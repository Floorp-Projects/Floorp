/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file defines a strongly-typed opaque gamepad handle
//
// The handle is designed to be copied around and passed over IPC. It keeps
// track of which "provider" created the handle so it can ensure that
// providers are never mixed. It also allows each provider to have its own
// algorithm for generating gamepad IDs, since the VR and Platform services
// do it differently.

#ifndef mozilla_dom_gamepad_GamepadHandle_h
#define mozilla_dom_gamepad_GamepadHandle_h

#include "PLDHashTable.h"
#include <type_traits>
#include <cinttypes>

namespace IPC {

template <class>
struct ParamTraits;

}  // namespace IPC

namespace mozilla::gfx {

class VRDisplayClient;
class VRManager;

}  // namespace mozilla::gfx

namespace mozilla::dom {

class GamepadPlatformService;
class GamepadServiceTest;
class XRInputSource;

// The "kind" of a gamepad handle is based on which provider created it
enum class GamepadHandleKind : uint8_t {
  GamepadPlatformManager,
  VR,
};

class GamepadHandle {
 public:
  // Allow handle to be passed around as a simple object
  GamepadHandle() = default;
  GamepadHandle(const GamepadHandle&) = default;
  GamepadHandle& operator=(const GamepadHandle&) = default;

  // Helps code know which manager to send requests to
  GamepadHandleKind GetKind() const;

  // Define operators so the handle can compared and stored in maps
  friend bool operator==(const GamepadHandle& a, const GamepadHandle& b);
  friend bool operator!=(const GamepadHandle& a, const GamepadHandle& b);
  friend bool operator<(const GamepadHandle& a, const GamepadHandle& b);

  PLDHashNumber Hash() const;

 private:
  explicit GamepadHandle(uint32_t aValue, GamepadHandleKind aKind);
  uint32_t GetValue() const { return mValue; }

  uint32_t mValue{0};
  GamepadHandleKind mKind{GamepadHandleKind::GamepadPlatformManager};

  // These are the classes that are "gamepad managers". They are allowed to
  // create new handles and inspect their actual value
  friend class mozilla::dom::GamepadPlatformService;
  friend class mozilla::dom::GamepadServiceTest;
  friend class mozilla::dom::XRInputSource;
  friend class mozilla::gfx::VRDisplayClient;
  friend class mozilla::gfx::VRManager;

  // Allow IPDL to serialize us
  friend struct IPC::ParamTraits<mozilla::dom::GamepadHandle>;
};

static_assert(std::is_trivially_copyable<GamepadHandle>::value,
              "GamepadHandle must be trivially copyable");

}  // namespace mozilla::dom

#endif  // mozilla_dom_gamepad_GamepadHandle_h
