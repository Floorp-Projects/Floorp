import {actionCreators as ac, actionTypes as at} from "common/Actions.jsm";

const _OpenInPrivateWindow = site => ({
  id: "menu_action_open_private_window",
  icon: "new-window-private",
  action: ac.OnlyToMain({
    type: at.OPEN_PRIVATE_WINDOW,
    data: {url: site.url, referrer: site.referrer}
  }),
  userEvent: "OPEN_PRIVATE_WINDOW"
});

export const GetPlatformString = platform => {
  switch (platform) {
    case "win":
      return "menu_action_show_file_windows";
    case "macosx":
      return "menu_action_show_file_mac_os";
    case "linux":
      return "menu_action_show_file_linux";
    default:
      return "menu_action_show_file_default";
  }
};

/**
 * List of functions that return items that can be included as menu options in a
 * LinkMenu. All functions take the site as the first parameter, and optionally
 * the index of the site.
 */
export const LinkMenuOptions = {
  Separator: () => ({type: "separator"}),
  EmptyItem: () => ({type: "empty"}),
  RemoveBookmark: site => ({
    id: "menu_action_remove_bookmark",
    icon: "bookmark-added",
    action: ac.AlsoToMain({
      type: at.DELETE_BOOKMARK_BY_ID,
      data: site.bookmarkGuid
    }),
    userEvent: "BOOKMARK_DELETE"
  }),
  AddBookmark: site => ({
    id: "menu_action_bookmark",
    icon: "bookmark-hollow",
    action: ac.AlsoToMain({
      type: at.BOOKMARK_URL,
      data: {url: site.url, title: site.title, type: site.type}
    }),
    userEvent: "BOOKMARK_ADD"
  }),
  OpenInNewWindow: site => ({
    id: "menu_action_open_new_window",
    icon: "new-window",
    action: ac.AlsoToMain({
      type: at.OPEN_NEW_WINDOW,
      data: {
        referrer: site.referrer,
        typedBonus: site.typedBonus,
        url: site.url
      }
    }),
    userEvent: "OPEN_NEW_WINDOW"
  }),
  BlockUrl: (site, index, eventSource) => ({
    id: "menu_action_dismiss",
    icon: "dismiss",
    action: ac.AlsoToMain({
      type: at.BLOCK_URL,
      data: {url: site.url, pocket_id: site.pocket_id}
    }),
    impression: ac.ImpressionStats({
      source: eventSource,
      block: 0,
      tiles: [{id: site.guid, pos: index}]
    }),
    userEvent: "BLOCK"
  }),

  // This is an option for web extentions which will result in remove items from
  // memory and notify the web extenion, rather than using the built-in block list.
  WebExtDismiss: (site, index, eventSource) => ({
    id: "menu_action_webext_dismiss",
    string_id: "menu_action_dismiss",
    icon: "dismiss",
    action: ac.WebExtEvent(at.WEBEXT_DISMISS, {
      source: eventSource,
      url: site.url,
      action_position: index
    })
  }),
  DeleteUrl: (site, index, eventSource, isEnabled, siteInfo) => ({
    id: "menu_action_delete",
    icon: "delete",
    action: {
      type: at.DIALOG_OPEN,
      data: {
        onConfirm: [
          ac.AlsoToMain({type: at.DELETE_HISTORY_URL, data: {url: site.url, pocket_id: site.pocket_id, forceBlock: site.bookmarkGuid}}),
          ac.UserEvent(Object.assign({event: "DELETE", source: eventSource, action_position: index}, siteInfo))
        ],
        eventSource,
        body_string_id: ["confirm_history_delete_p1", "confirm_history_delete_notice_p2"],
        confirm_button_string_id: "menu_action_delete",
        cancel_button_string_id: "topsites_form_cancel_button",
        icon: "modal-delete"
      }
    },
    userEvent: "DIALOG_OPEN"
  }),
  ShowFile: (site, index, eventSource, isEnabled, siteInfo, platform) => ({
    id: GetPlatformString(platform),
    icon: "search",
    action: ac.OnlyToMain({
      type: at.SHOW_DOWNLOAD_FILE,
      data: {url: site.url}
    })
  }),
  OpenFile: site => ({
    id: "menu_action_open_file",
    icon: "open-file",
    action: ac.OnlyToMain({
      type: at.OPEN_DOWNLOAD_FILE,
      data: {url: site.url}
    })
  }),
  CopyDownloadLink: site => ({
    id: "menu_action_copy_download_link",
    icon: "copy",
    action: ac.OnlyToMain({
      type: at.COPY_DOWNLOAD_LINK,
      data: {url: site.url}
    })
  }),
  GoToDownloadPage: site => ({
    id: "menu_action_go_to_download_page",
    icon: "download",
    action: ac.OnlyToMain({
      type: at.OPEN_LINK,
      data: {url: site.referrer}
    }),
    disabled: !site.referrer
  }),
  RemoveDownload: site => ({
    id: "menu_action_remove_download",
    icon: "delete",
    action: ac.OnlyToMain({
      type: at.REMOVE_DOWNLOAD_FILE,
      data: {url: site.url}
    })
  }),
  PinTopSite: (site, index) => ({
    id: "menu_action_pin",
    icon: "pin",
    action: ac.AlsoToMain({
      type: at.TOP_SITES_PIN,
      data: {site: {url: site.url}, index}
    }),
    userEvent: "PIN"
  }),
  UnpinTopSite: site => ({
    id: "menu_action_unpin",
    icon: "unpin",
    action: ac.AlsoToMain({
      type: at.TOP_SITES_UNPIN,
      data: {site: {url: site.url}}
    }),
    userEvent: "UNPIN"
  }),
  SaveToPocket: (site, index, eventSource) => ({
    id: "menu_action_save_to_pocket",
    icon: "pocket",
    action: ac.AlsoToMain({
      type: at.SAVE_TO_POCKET,
      data: {site: {url: site.url, title: site.title}}
    }),
    impression: ac.ImpressionStats({
      source: eventSource,
      pocket: 0,
      tiles: [{id: site.guid, pos: index}]
    }),
    userEvent: "SAVE_TO_POCKET"
  }),
  DeleteFromPocket: site => ({
    id: "menu_action_delete_pocket",
    icon: "delete",
    action: ac.AlsoToMain({
      type: at.DELETE_FROM_POCKET,
      data: {pocket_id: site.pocket_id}
    }),
    userEvent: "DELETE_FROM_POCKET"
  }),
  ArchiveFromPocket: site => ({
    id: "menu_action_archive_pocket",
    icon: "check",
    action: ac.AlsoToMain({
      type: at.ARCHIVE_FROM_POCKET,
      data: {pocket_id: site.pocket_id}
    }),
    userEvent: "ARCHIVE_FROM_POCKET"
  }),
  EditTopSite: (site, index) => ({
    id: "edit_topsites_button_text",
    icon: "edit",
    action: {
      type: at.TOP_SITES_EDIT,
      data: {index}
    }
  }),
  CheckBookmark: site => (site.bookmarkGuid ? LinkMenuOptions.RemoveBookmark(site) : LinkMenuOptions.AddBookmark(site)),
  CheckPinTopSite: (site, index) => (site.isPinned ? LinkMenuOptions.UnpinTopSite(site) : LinkMenuOptions.PinTopSite(site, index)),
  CheckSavedToPocket: (site, index) => (site.pocket_id ? LinkMenuOptions.DeleteFromPocket(site) : LinkMenuOptions.SaveToPocket(site, index)),
  CheckBookmarkOrArchive: site => (site.pocket_id ? LinkMenuOptions.ArchiveFromPocket(site) : LinkMenuOptions.CheckBookmark(site)),
  OpenInPrivateWindow: (site, index, eventSource, isEnabled) => (isEnabled ? _OpenInPrivateWindow(site) : LinkMenuOptions.EmptyItem())
};
