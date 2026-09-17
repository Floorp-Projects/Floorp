// SPDX-License-Identifier: MPL-2.0
import type { StackTab } from "./stack-bar.tsx";

export type StackSplitView = XULElement & {
  tabs: StackTab[];
};
