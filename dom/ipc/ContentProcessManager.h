/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentProcessManager_h
#define mozilla_dom_ContentProcessManager_h

#include "mozilla/StaticPtr.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/dom/ipc/IdType.h"
#include "nsTArray.h"
#include "nsTHashMap.h"

namespace mozilla {
namespace dom {
class ContentParent;

class ContentProcessManager final {
 public:
  static ContentProcessManager* GetSingleton();
  MOZ_COUNTED_DTOR(ContentProcessManager);

  /**
   * Add a new content process into the map.
   */
  void AddContentProcess(ContentParent* aChildCp);

  /**
   * Remove the content process by id.
   */
  void RemoveContentProcess(const ContentParentId& aChildCpId);

  /**
   * Return the ContentParent pointer by id.
   */
  ContentParent* GetContentProcessById(const ContentParentId& aChildCpId);

  /**
   * Add a new browser parent into the map.
   */
  bool RegisterRemoteFrame(BrowserParent* aChildBp);

  /**
   * Remove the browser parent by the given tab id.
   */
  void UnregisterRemoteFrame(const TabId& aChildTabId);

  /**
   * Get the ContentParentId of the parent of the given tab id.
   */
  ContentParentId GetTabProcessId(const TabId& aChildTabId);

  /**
   * Get the number of BrowserParents managed by the givent content process.
   * Return 0 when ContentParent couldn't be found via aChildCpId.
   */
  uint32_t GetBrowserParentCountByProcessId(const ContentParentId& aChildCpId);

  /**
   * Get the BrowserParent by the given content process and tab id.
   * Return nullptr when BrowserParent couldn't be found via aChildCpId
   * and aChildTabId.
   */
  already_AddRefed<BrowserParent> GetBrowserParentByProcessAndTabId(
      const ContentParentId& aChildCpId, const TabId& aChildTabId);

  /**
   * Get the BrowserParent on top level by the given content process and tab id.
   *
   * This function returns the BrowserParent directly within a BrowserHost,
   * called top-level BrowserParent here, by given aChildCpId and aChildTabId.
   * The given aChildCpId and aChildTabId are related to a content process
   * and a tab respectively.
   */
  already_AddRefed<BrowserParent> GetTopLevelBrowserParentByProcessAndTabId(
      const ContentParentId& aChildCpId, const TabId& aChildTabId);

 private:
  static StaticAutoPtr<ContentProcessManager> sSingleton;

  nsTHashMap<nsUint64HashKey, ContentParent*> mContentParentMap;
  nsTHashMap<nsUint64HashKey, BrowserParent*> mBrowserParentMap;

  MOZ_COUNTED_DEFAULT_CTOR(ContentProcessManager);
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_ContentProcessManager_h
