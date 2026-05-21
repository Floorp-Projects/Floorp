// SPDX-License-Identifier: MPL-2.0
// Core framework module

export {
  EventSystem,
  createEventSystem,
  type EventSystemType,
} from "./EventSystem.ts";

export {
  registerModule,
  unregister,
  hotswap,
  getInstance,
  hasModule,
  getModuleNames,
  cleanupAll,
  type Module,
  type ModuleHandle,
  type ModuleInstance,
  type ModuleContext,
} from "./module_factory.ts";

export {
  registerModuleEventBus,
  unregisterModuleEventBus,
  isModuleRegistered,
  getModuleEventBus,
  ok,
  err,
  isOk,
  isErr,
  unwrap,
  unwrapOr,
  mapResult,
  type Result,
} from "./module-event-bus.ts";
