// SPDX-License-Identifier: MPL-2.0

import type {
  WorkspaceBookmarksService,
  WorkspaceBookmarkTree,
} from "../WorkspaceBookmarks.sys.mts";

export interface TransferBookmark {
  guid: string;
  parentGuid: string;
  title: string;
  type: number;
  url?: { href: string };
}

export interface TransferItem {
  itemGuid: string;
  parentGuid: string;
  title: string;
  type: string;
  instanceId: string;
}

export interface TransferRuntime {
  scopes: WorkspaceBookmarksService;
  places: {
    instanceId: string;
    TYPE_X_MOZ_PLACE_CONTAINER: string;
    bookmarks: {
      TYPE_FOLDER: number;
      unfiledGuid: string;
      insert(input: {
        parentGuid: string;
        type?: number;
        title: string;
        url?: string;
      }): Promise<TransferBookmark>;
      fetch(guid: string): Promise<TransferBookmark | null>;
      remove(guid: string): Promise<void>;
    };
    promiseBookmarksTree(guid: string): Promise<WorkspaceBookmarkTree | null>;
  };
  ui: {
    handleTransferItems(
      items: TransferItem[],
      insertionPoint: {
        isTag: false;
        guid: string;
        getIndex(): Promise<number>;
      },
      doCopy: boolean,
      view: { ownerWindow: Window; result: null },
    ): Promise<string[]>;
  };
  transactions: {
    undo(): Promise<void>;
    redo(): Promise<void>;
    clearTransactionsHistory(
      undoEntries: boolean,
      redoEntries: boolean,
    ): Promise<void>;
  };
}
