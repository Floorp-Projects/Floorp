/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import i18next from "i18next";
import { actions } from "./actions.ts";

export type GestureActionFn = () => void;
export interface GestureActionRegistration {
  name: string;
  fn: GestureActionFn;
}

export function getAllGestureActions(): GestureActionRegistration[] {
  return gestureActions.getActionsList();
}

export function executeGestureAction(name: string): boolean {
  const action = gestureActions.getAction(name);
  if (action) {
    try {
      action();
      return true;
    } catch (e) {
      console.error(`[mouse-gesture] Action failed: ${name}`, e);
      return false;
    }
  }
  console.warn(`[mouse-gesture] Action not found: ${name}`);
  return false;
}

export function getActionDisplayName(actionId: string): string {
  return i18next.t(`mouseGesture.actions.${actionId}`, {
    defaultValue: actionId,
  });
}

export function getActionDescription(actionId: string): string {
  return i18next.t(`mouseGesture.descriptions.${actionId}`, {
    defaultValue: "",
  });
}

class GestureActionsRegistry {
  private static instance: GestureActionsRegistry;
  private actions: Map<string, GestureActionRegistration> = new Map();

  private constructor() {
    this.registerActions(actions);
  }

  public static getInstance(): GestureActionsRegistry {
    if (!GestureActionsRegistry.instance) {
      GestureActionsRegistry.instance = new GestureActionsRegistry();
    }
    return GestureActionsRegistry.instance;
  }

  public registerAction(action: GestureActionRegistration): void {
    this.actions.set(action.name, action);
  }

  public registerActions(actions: GestureActionRegistration[]): void {
    for (const action of actions) {
      this.registerAction(action);
    }
  }

  public getAction(name: string): GestureActionFn | undefined {
    return this.actions.get(name)?.fn;
  }

  public getAllActions(): Map<string, GestureActionRegistration> {
    return this.actions;
  }

  public getActionsList(): GestureActionRegistration[] {
    return Array.from(this.actions.values());
  }
}

export const gestureActions = GestureActionsRegistry.getInstance();
