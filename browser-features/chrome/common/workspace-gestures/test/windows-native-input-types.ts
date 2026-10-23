// SPDX-License-Identifier: MPL-2.0

import type { TestWindow } from "../../workspaces/test/workspace-last-tab-test-types.ts";

export type NativeInputWindow = TestWindow & {
  __keyboardShortcutControllerAttached?: boolean;
  gBrowser: TestWindow["gBrowser"] & {
    selectedBrowser: TestWindow["gBrowser"]["selectedBrowser"] & {
      focus(): void;
    };
  };
};

export type NativeInputDirection = "Previous" | "Next";

export interface NativeInputReply {
  status: "sent";
  processId: number;
  windowHandle: string;
  direction: NativeInputDirection;
  key: "F13" | "F14";
  repeatCount: number;
  sentInputCount: number;
  expectedInputCount: number;
  cleanupInputCount: number;
  foregroundVerified: boolean;
  executable: string;
  processStartedAt: string;
  sessionId: number;
  error: null;
}

export interface NativeInputKey {
  window: "primary" | "secondary";
  type: string;
  code: string;
  key: string;
  repeat: boolean;
  trusted: boolean;
  ctrl: boolean;
  shift: boolean;
  alt: boolean;
  meta: boolean;
  altGraph: boolean;
  defaultPrevented: boolean;
}

export interface NativeInputSnapshot {
  workspaceId: string;
  selectedTab: string;
}

export interface NativeInputStep {
  name: string;
  target: "primary" | "secondary";
  direction: NativeInputDirection;
  repeatCount: number;
  before: NativeInputSnapshot;
  after?: NativeInputSnapshot;
  reply?: NativeInputReply;
  keys: NativeInputKey[];
  changedWorkspaces: string[];
}

export interface NativeInputEvidence {
  status: "running" | "passed" | "failed";
  os: string;
  runtimeVersion: string;
  runtimeBuildId: string;
  startedAt: string;
  finishedAt?: string;
  steps: NativeInputStep[];
  error?: string;
}

export interface NativeInputProcess {
  stdout: { readString(): Promise<string> };
  wait(): Promise<{ exitCode: number }>;
  kill(timeout: number): Promise<unknown>;
}

export interface NativeInputSubprocess {
  pathSearch(command: string): Promise<string>;
  call(options: {
    command: string;
    arguments: string[];
    stderr: "stdout";
    workdir: string;
  }): Promise<NativeInputProcess>;
}

export interface NativeInputSavedPref {
  name: string;
  hadUserValue: boolean;
  value: boolean | string;
}
