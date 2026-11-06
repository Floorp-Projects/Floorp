// SPDX-License-Identifier: MPL-2.0
import type {
  ButtonHTMLAttributes,
  Dispatch,
  PropsWithChildren,
  RefObject,
  SetStateAction,
} from "react";
import type {
  BackgroundSettings,
  BookmarkNode,
  SafeNode,
  Sponsor,
  DeleteSummary,
  FolderView,
  NewTabSnapshot,
} from "../../../../../libs/newtab/types.ts";
export type Page = "home" | "dial" | "feed";
export type DialogState =
  | {
      type: "bookmark";
      node?: BookmarkNode;
      kind: "bookmark" | "folder";
      parentGuid: string;
    }
  | { type: "delete"; summary: DeleteSummary; title: string }
  | { type: "subscribe" }
  | { type: "unsubscribe"; id: string; title: string }
  | { type: "block"; url: string; title: string }
  | null;
export interface NewTabContextValue {
  snapshot: NewTabSnapshot;
  page: Page;
  groupGuid: string;
  folderGuid: string;
  folder: FolderView | null;
  folderLoading: boolean;
  folderError: string;
  epoch: number;
  navigate: (page: Page) => void;
  openFolder: (guid: string) => void;
  selectGroup: (guid: string) => void;
  refresh: () => Promise<void>;
  reloadFolder: () => Promise<void>;
  dialog: DialogState;
  setDialog: Dispatch<SetStateAction<DialogState>>;
  notify: (message: string) => void;
}

export type IconButtonProps = PropsWithChildren<
  ButtonHTMLAttributes<HTMLButtonElement> & { label: string }
>;
export interface ArticleStatePatch {
  isRead?: boolean;
  starred?: boolean;
}
export type TileProps = {
  node: BookmarkNode;
  index: number;
  total: number;
  sponsor?: Sponsor;
  suggestion?: boolean;
  compact: boolean;
};
export type BookmarkGridProps = { compact?: boolean };
export type DestinationPickerProps = {
  value: string;
  onChange: (guid: string) => void;
  exclude?: string;
  disabled?: boolean;
};
export type BookmarkDialogProps = {
  dialog: Extract<NonNullable<DialogState>, { type: "bookmark" }>;
  close: () => void;
};
export type SubscribeDialogProps = { close: () => void };
export type FeedImageProps = {
  articleId: string;
  imageId: string;
  alt: string;
};
export type FeedContentProps = {
  nodes: SafeNode[];
  articleId: string;
};
export type WallpaperProps = {
  settings: BackgroundSettings;
  visible: boolean;
  onLoadedChange: (loaded: boolean) => void;
};
export type FloatingMenuProps = PropsWithChildren<{
  anchor: RefObject<HTMLButtonElement | null>;
  close: () => void;
  label: string;
}>;
export type SiteIconProps = {
  url?: string;
  folder?: boolean;
};
export type ModalProps = PropsWithChildren<{
  title: string;
  closeLabel: string;
  close: () => void;
}>;
