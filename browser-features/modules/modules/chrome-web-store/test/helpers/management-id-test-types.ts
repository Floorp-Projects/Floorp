// SPDX-License-Identifier: MPL-2.0
/// <reference types="jsr:@types/gecko" />
import type {
  NativeCorpusAddon,
  NativeCorpusExtension,
  NativeCorpusTab,
} from "../../../../../../tools/chrome-extension-runtime/native-test-types.ts";

export interface ManagementIDAddon extends NativeCorpusAddon {
  id: string;
  userPermissions?: { floorpChromeOriginalId?: unknown };
}

export interface ManagementIDAddonManager {
  installTemporaryAddon(file: nsIFile): Promise<ManagementIDAddon>;
  getAddonByID(id: string): Promise<ManagementIDAddon | null>;
}

export type ManagementIDExecution = "page" | "worker";
export type ManagementIDOperation =
  | "inspect"
  | "get"
  | "setEnabled"
  | "events";

export interface ManagementIDInfo {
  id: string;
  enabled: boolean;
  permissions?: string[];
}

export interface ManagementIDEvent {
  event: "onInstalled" | "onEnabled" | "onDisabled" | "onUninstalled";
  value: ManagementIDInfo | string;
}

export interface ManagementIDResult {
  runtimeId: string;
  execution: ManagementIDExecution;
  self?: ManagementIDInfo;
  all?: ManagementIDInfo[];
  info?: ManagementIDInfo;
  events?: ManagementIDEvent[];
}

export interface ManagementIDReply {
  commandId: string;
  result?: ManagementIDResult;
  error?: string;
}

export interface ManagementIDInstallOptions {
  id: string;
  markerId?: string;
  controller?: boolean;
  worker?: boolean;
}

export interface ManagementIDFixture {
  id: string;
  addon: ManagementIDAddon;
  path: string;
  extension: NativeCorpusExtension;
  page?: NativeCorpusTab;
}

export type ManagementIDOwnedAddon = Pick<
  ManagementIDFixture,
  "id" | "addon"
>;
