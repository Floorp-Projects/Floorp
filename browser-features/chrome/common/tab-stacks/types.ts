// SPDX-License-Identifier: MPL-2.0
import type { StackTab } from "./stack-bar.tsx";

export type StackSplitView = XULElement & {
  tabs: StackTab[];
};

export type StackDragController = {
  startTabDrag(
    event: DragEvent,
    tab: StackTab,
    options: { fromTabList: boolean },
  ): void;
  handle_dragend(event: DragEvent): void;
  getDropEffectForTabDrag(event: DragEvent): string;
};

export type StackDataTransfer = DataTransfer & {
  mozGetDataAt(format: string, index: number): unknown;
};
