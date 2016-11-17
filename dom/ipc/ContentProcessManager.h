/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_ContentProcessManager_h
#define mozilla_dom_ContentProcessManager_h

#include <map>
#include <set>
#include "mozilla/StaticPtr.h"
#include "mozilla/dom/TabContext.h"
#include "mozilla/dom/ipc/IdType.h"
#include "nsTArray.h"

namespace mozilla {
namespace dom {
class ContentParent;

struct RemoteFrameInfo
{
  TabId mOpenerTabId;
  TabContext mContext;
};

struct ContentProcessInfo
{
  ContentParent* mCp;
  ContentParentId mParentCpId;
  std::set<ContentParentId> mChildrenCpId;
  std::map<TabId, RemoteFrameInfo> mRemoteFrames;
};

class ContentProcessManager final
{
public:
  static ContentProcessManager* GetSingleton();
  ~ContentProcessManager() {MOZ_COUNT_DTOR(ContentProcessManager);};

  /**
   * Add a new content process into the map.
   * If aParentCpId is not 0, it's a nested content process.
   */
  void AddContentProcess(ContentParent* aChildCp,
                         const ContentParentId& aParentCpId = ContentParentId(0));
  /**
   * Remove the content process by id.
   */
  void RemoveContentProcess(const ContentParentId& aChildCpId);
  /**
   * Add a grandchild content process into the map.
   * aParentCpId must be already added in the map by AddContentProcess().
   */
  bool AddGrandchildProcess(const ContentParentId& aParentCpId,
                            const ContentParentId& aChildCpId);
  /**
   * Get the parent process's id by child process's id.
   * Used to check if a child really belongs to the parent.
   */
  bool GetParentProcessId(const ContentParentId& aChildCpId,
                          /*out*/ ContentParentId* aParentCpId);
  /**
   * Return the ContentParent pointer by id.
   */
  ContentParent* GetContentProcessById(const ContentParentId& aChildCpId);

  /**
   * Return a list of all child process's id.
   */
  nsTArray<ContentParentId>
  GetAllChildProcessById(const ContentParentId& aParentCpId);

  /**
   * Allocate a tab id for the given content process's id.
   * Used when a content process wants to create a new tab. aOpenerTabId and
   * aContext are saved in RemoteFrameInfo, which is a part of
   * ContentProcessInfo.  We can use the tab id and process id to locate the
   * TabContext for future use.
   */
  TabId AllocateTabId(const TabId& aOpenerTabId,
                      const IPCTabContext& aContext,
                      const ContentParentId& aChildCpId);

  /**
   * Remove the RemoteFrameInfo by the given process and tab id.
   */
  void DeallocateTabId(const ContentParentId& aChildCpId,
                       const TabId& aChildTabId);

  /**
   * Get the TabContext by the given content process and tab id.
   */
  bool
  GetTabContextByProcessAndTabId(const ContentParentId& aChildCpId,
                                 const TabId& aChildTabId,
                                 /*out*/ TabContext* aTabContext);

  /**
   * Get all TabContext which are inside the given content process.
   */
  nsTArray<TabContext>
  GetTabContextByContentProcess(const ContentParentId& aChildCpId);

  /**
   * Query a tab's opener id by the given process and tab id.
   * XXX Currently not used. Plan to be used for bug 1020179.
   */
  bool GetRemoteFrameOpenerTabId(const ContentParentId& aChildCpId,
                                 const TabId& aChildTabId,
                                 /*out*/ TabId* aOpenerTabId);

  /**
   * Get all TabParents' Ids managed by the givent content process.
   * Return empty array when TabParent couldn't be found via aChildCpId
   */
  nsTArray<TabId>
  GetTabParentsByProcessId(const ContentParentId& aChildCpId);

  /**
   * Get the TabParent by the given content process and tab id.
   * Return nullptr when TabParent couldn't be found via aChildCpId
   * and aChildTabId.
   * (or probably because the TabParent is not in the chrome process)
   */
  already_AddRefed<TabParent>
  GetTabParentByProcessAndTabId(const ContentParentId& aChildCpId,
                                const TabId& aChildTabId);

  /**
   * Get the TabParent on top level by the given content process and tab id.
   *
   *  This function return the TabParent belong to the chrome process,
   *  called top-level TabParent here, by given aChildCpId and aChildTabId.
   *  The given aChildCpId and aChildTabId are related to a content process
   *  and a tab respectively. In nested-oop, the top-level TabParent isn't
   *  always the opener tab of the given tab in content process. This function
   *  will call GetTabParentByProcessAndTabId iteratively until the Tab returned
   *  is belong to the chrome process.
   */
  already_AddRefed<TabParent>
  GetTopLevelTabParentByProcessAndTabId(const ContentParentId& aChildCpId,
                                        const TabId& aChildTabId);

private:
  static StaticAutoPtr<ContentProcessManager> sSingleton;
  TabId mUniqueId;
  std::map<ContentParentId, ContentProcessInfo> mContentParentMap;

  ContentProcessManager() {MOZ_COUNT_CTOR(ContentProcessManager);};
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ContentProcessManager_h
