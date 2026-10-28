import type { Dispatch, SetStateAction } from "react";
import type {
  FeedFilter,
  DialFolder,
  GroupId,
  HubTab,
  PrototypeState,
  Route,
  Site,
  WorkspaceId,
} from "./types.ts";

export type DialogState =
  | {
      kind: "site";
      site?: Site;
      initialGroup?: GroupId;
      initialFolderId?: string | null;
    }
  | { kind: "feed" }
  | { kind: "folder" }
  | {
      kind: "edit-folder";
      folder?: DialFolder;
      initialFolderId?: string | null;
    }
  | { kind: "delete-folder"; folder: DialFolder }
  | { kind: "session" }
  | null;

export interface PrototypeContextValue {
  data: PrototypeState;
  setData: Dispatch<SetStateAction<PrototypeState>>;
  route: Route;
  navigate: (route: Route, tab?: HubTab) => void;
  workspaceId: WorkspaceId;
  setWorkspaceId: (id: WorkspaceId) => void;
  groupId: GroupId;
  setGroupId: (id: GroupId) => void;
  folderId: string | null;
  openFolder: (id: string | null) => void;
  hubTab: HubTab;
  setHubTab: (tab: HubTab) => void;
  selectedArticleId: string | null;
  openArticle: (id: string) => void;
  feedFilter: FeedFilter;
  setFeedFilter: Dispatch<SetStateAction<FeedFilter>>;
  selectedFeedId: string | null;
  setSelectedFeedId: Dispatch<SetStateAction<string | null>>;
  feedQuery: string;
  setFeedQuery: Dispatch<SetStateAction<string>>;
  dialog: DialogState;
  setDialog: Dispatch<SetStateAction<DialogState>>;
  toast: string;
  showToast: (message: string) => void;
}
