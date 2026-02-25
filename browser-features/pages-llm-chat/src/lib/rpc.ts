/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createBirpc } from "birpc";

// RPC functions available from the parent (Firefox chrome context)
export interface NRSettingsParentFunctions {
  getBoolPref: (prefName: string) => Promise<boolean | null>;
  getIntPref: (prefName: string) => Promise<number | null>;
  getStringPref: (prefName: string) => Promise<string | null>;
  setBoolPref: (prefName: string, value: boolean) => Promise<void>;
  setIntPref: (prefName: string, value: number) => Promise<void>;
  setStringPref: (prefName: string, value: string) => Promise<void>;
}

// Window extensions provided by NRSettingsChild actor
declare global {
  interface Window {
    NRSettingsSend?: (data: string) => void;
    NRSettingsRegisterReceiveCallback?: (
      callback: (data: string) => void,
    ) => void;
    NRSPing?: () => boolean;
  }
}

// Check if we're running in dev mode (localhost) or in chrome context
const isDevMode =
  typeof location !== "undefined" &&
  (location.port === "5190" || location.port === "5183");

// Create RPC client
function createRpc(): NRSettingsParentFunctions {
  const host = globalThis as Window;
  if (
    isDevMode &&
    host.NRSettingsSend &&
    host.NRSettingsRegisterReceiveCallback
  ) {
    // Dev mode: use birpc with actor communication
    return createBirpc<NRSettingsParentFunctions, Record<string, never>>(
      {},
      {
        post: (data) => host.NRSettingsSend!(data),
        on: (callback) => {
          host.NRSettingsRegisterReceiveCallback!(callback);
        },
        serialize: (v) => JSON.stringify(v),
        deserialize: (v) => JSON.parse(v),
      },
    );
  }

  // Fallback: direct Services access (only works in chrome context)
  // This won't work in content pages, but we keep it for potential future use
  throw new Error("RPC not available: NRSettings actor not connected");
}

let _rpc: NRSettingsParentFunctions | null = null;

export function getRpc(): NRSettingsParentFunctions {
  if (!_rpc) {
    _rpc = createRpc();
  }
  return _rpc;
}

// Check if RPC is available
export function isRpcAvailable(): boolean {
  const host = globalThis as Window;
  return (
    isDevMode &&
    typeof host.NRSettingsSend === "function" &&
    typeof host.NRSettingsRegisterReceiveCallback === "function"
  );
}
