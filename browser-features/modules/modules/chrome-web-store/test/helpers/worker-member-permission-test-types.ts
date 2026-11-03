// SPDX-License-Identifier: MPL-2.0
import type {
  MethodShape,
  PromptWindow,
} from "./worker-permission-test-types.ts";
export type MemberPromptWindow = PromptWindow;
export type MemberMode = "unread" | "enumeration" | "descriptor";
export interface MemberState {
  generation: string;
  sameChrome: boolean;
  isWorker: boolean;
  hasDocument: boolean;
  phase: string;
  contains: boolean;
  error?: string;
  preAccess: {
    mode: MemberMode;
    keys?: string[];
    ownKeys?: string[];
    descriptor?: MethodShape["descriptor"];
    getSelf?: MethodShape;
    getAll?: MethodShape;
    selfId?: string;
  };
  events: { event: string; permissions: string[] }[];
}
export interface MemberRead {
  generation: string;
  sameChrome: boolean;
  namespaceType: string;
  samePreManagement: boolean | null;
  getSelf: MethodShape;
  getAll: MethodShape;
  setEnabled: MethodShape;
  selfId: string;
  listContainsSelf: boolean | null;
  extensionCount: number | null;
}
export interface MemberRevocation {
  generation: string;
  sameChrome: boolean;
  sameManagement: boolean;
  sameGetAll: boolean;
  getAll: MethodShape;
  selfId: string;
  denial: {
    synchronous: boolean;
    succeeded: boolean;
    name?: string;
    message?: string;
  } | null;
}
