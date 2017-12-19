/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShellLoadTypes_h_
#define nsDocShellLoadTypes_h_

#ifdef MOZILLA_INTERNAL_API

#include "nsDOMNavigationTiming.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIWebNavigation.h"

/**
 * Load flag for error pages. This uses one of the reserved flag
 * values from nsIWebNavigation.
 */
#define LOAD_FLAGS_ERROR_PAGE 0x0001U

#define MAKE_LOAD_TYPE(type, flags) ((type) | ((flags) << 16))
#define LOAD_TYPE_HAS_FLAGS(type, flags) ((type) & ((flags) << 16))

/**
 * These are flags that confuse ConvertLoadTypeToDocShellLoadInfo and should
 * not be passed to MAKE_LOAD_TYPE.  In particular this includes all flags
 * above 0xffff (e.g. LOAD_FLAGS_BYPASS_CLASSIFIER), since MAKE_LOAD_TYPE would
 * just shift them out anyway.
 */
#define EXTRA_LOAD_FLAGS (LOAD_FLAGS_FIRST_LOAD | \
                          LOAD_FLAGS_ALLOW_POPUPS | \
                          0xffff0000)

/* load types are legal combinations of load commands and flags
 *
 * NOTE:
 *  Remember to update the IsValidLoadType function below if you change this
 *  enum to ensure bad flag combinations will be rejected.
 */
enum LoadType
{
  LOAD_NORMAL = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_NONE),
  LOAD_NORMAL_REPLACE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY),
  LOAD_NORMAL_EXTERNAL = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_FROM_EXTERNAL),
  LOAD_HISTORY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_HISTORY, nsIWebNavigation::LOAD_FLAGS_NONE),
  LOAD_NORMAL_BYPASS_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
  LOAD_NORMAL_BYPASS_PROXY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
  LOAD_NORMAL_BYPASS_PROXY_AND_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
  LOAD_NORMAL_ALLOW_MIXED_CONTENT = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_ALLOW_MIXED_CONTENT | nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
  LOAD_RELOAD_NORMAL = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_NONE),
  LOAD_RELOAD_BYPASS_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
  LOAD_RELOAD_BYPASS_PROXY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
  LOAD_RELOAD_ALLOW_MIXED_CONTENT = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_ALLOW_MIXED_CONTENT | nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
  LOAD_RELOAD_BYPASS_PROXY_AND_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
  LOAD_LINK = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_IS_LINK),
  LOAD_REFRESH = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_IS_REFRESH),
  LOAD_RELOAD_CHARSET_CHANGE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE),
  LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE | nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
  LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE | nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
  LOAD_BYPASS_HISTORY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY),
  LOAD_STOP_CONTENT = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_STOP_CONTENT),
  LOAD_STOP_CONTENT_AND_REPLACE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_STOP_CONTENT | nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY),
  LOAD_PUSHSTATE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_PUSHSTATE, nsIWebNavigation::LOAD_FLAGS_NONE),
  LOAD_REPLACE_BYPASS_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY | nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
  /**
   * Load type for an error page. These loads are never triggered by users of
   * Docshell. Instead, Docshell triggers the load itself when a
   * consumer-triggered load failed.
   */
  LOAD_ERROR_PAGE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL,
                                   LOAD_FLAGS_ERROR_PAGE)

  // NOTE: Adding a new value? Remember to update IsValidLoadType!
};

static inline bool
IsValidLoadType(uint32_t aLoadType)
{
  switch (aLoadType) {
    case LOAD_NORMAL:
    case LOAD_NORMAL_REPLACE:
    case LOAD_NORMAL_EXTERNAL:
    case LOAD_NORMAL_BYPASS_CACHE:
    case LOAD_NORMAL_BYPASS_PROXY:
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
    case LOAD_NORMAL_ALLOW_MIXED_CONTENT:
    case LOAD_HISTORY:
    case LOAD_RELOAD_NORMAL:
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
    case LOAD_RELOAD_ALLOW_MIXED_CONTENT:
    case LOAD_LINK:
    case LOAD_REFRESH:
    case LOAD_RELOAD_CHARSET_CHANGE:
    case LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE:
    case LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE:
    case LOAD_BYPASS_HISTORY:
    case LOAD_STOP_CONTENT:
    case LOAD_STOP_CONTENT_AND_REPLACE:
    case LOAD_PUSHSTATE:
    case LOAD_REPLACE_BYPASS_CACHE:
    case LOAD_ERROR_PAGE:
      return true;
  }
  return false;
}

static inline bool
IsForceReloadType(uint32_t aLoadType) {
  switch (aLoadType) {
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
    case LOAD_RELOAD_ALLOW_MIXED_CONTENT:
      return true;
  }
  return false;
}

static inline nsDocShellInfoLoadType
ConvertLoadTypeToDocShellInfoLoadType(uint32_t aLoadType)
{
  nsDocShellInfoLoadType docShellLoadType = nsIDocShellLoadInfo::loadNormal;
  switch (aLoadType) {
    case LOAD_NORMAL:
      docShellLoadType = nsIDocShellLoadInfo::loadNormal;
      break;
    case LOAD_NORMAL_REPLACE:
      docShellLoadType = nsIDocShellLoadInfo::loadNormalReplace;
      break;
    case LOAD_NORMAL_EXTERNAL:
      docShellLoadType = nsIDocShellLoadInfo::loadNormalExternal;
      break;
    case LOAD_NORMAL_BYPASS_CACHE:
      docShellLoadType = nsIDocShellLoadInfo::loadNormalBypassCache;
      break;
    case LOAD_NORMAL_BYPASS_PROXY:
      docShellLoadType = nsIDocShellLoadInfo::loadNormalBypassProxy;
      break;
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
      docShellLoadType = nsIDocShellLoadInfo::loadNormalBypassProxyAndCache;
      break;
    case LOAD_NORMAL_ALLOW_MIXED_CONTENT:
      docShellLoadType = nsIDocShellLoadInfo::loadNormalAllowMixedContent;
      break;
    case LOAD_HISTORY:
      docShellLoadType = nsIDocShellLoadInfo::loadHistory;
      break;
    case LOAD_RELOAD_NORMAL:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadNormal;
      break;
    case LOAD_RELOAD_CHARSET_CHANGE:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadCharsetChange;
      break;
    case LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadCharsetChangeBypassCache;
      break;
    case LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadCharsetChangeBypassProxyAndCache;
      break;
    case LOAD_RELOAD_BYPASS_CACHE:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadBypassCache;
      break;
    case LOAD_RELOAD_BYPASS_PROXY:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadBypassProxy;
      break;
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadBypassProxyAndCache;
      break;
    case LOAD_LINK:
      docShellLoadType = nsIDocShellLoadInfo::loadLink;
      break;
    case LOAD_REFRESH:
      docShellLoadType = nsIDocShellLoadInfo::loadRefresh;
      break;
    case LOAD_BYPASS_HISTORY:
    case LOAD_ERROR_PAGE:
      docShellLoadType = nsIDocShellLoadInfo::loadBypassHistory;
      break;
    case LOAD_STOP_CONTENT:
      docShellLoadType = nsIDocShellLoadInfo::loadStopContent;
      break;
    case LOAD_STOP_CONTENT_AND_REPLACE:
      docShellLoadType = nsIDocShellLoadInfo::loadStopContentAndReplace;
      break;
    case LOAD_PUSHSTATE:
      docShellLoadType = nsIDocShellLoadInfo::loadPushState;
      break;
    case LOAD_REPLACE_BYPASS_CACHE:
      docShellLoadType = nsIDocShellLoadInfo::loadReplaceBypassCache;
      break;
    case LOAD_RELOAD_ALLOW_MIXED_CONTENT:
      docShellLoadType = nsIDocShellLoadInfo::loadReloadMixedContent;
      break;
    default:
      NS_NOTREACHED("Unexpected load type value");
  }

  return docShellLoadType;
}

static inline uint32_t
ConvertDocShellInfoLoadTypeToLoadType(nsDocShellInfoLoadType aDocShellLoadType)
{
  uint32_t loadType = LOAD_NORMAL;

  switch (aDocShellLoadType) {
    case nsIDocShellLoadInfo::loadNormal:
      loadType = LOAD_NORMAL;
      break;
    case nsIDocShellLoadInfo::loadNormalReplace:
      loadType = LOAD_NORMAL_REPLACE;
      break;
    case nsIDocShellLoadInfo::loadNormalExternal:
      loadType = LOAD_NORMAL_EXTERNAL;
      break;
    case nsIDocShellLoadInfo::loadHistory:
      loadType = LOAD_HISTORY;
      break;
    case nsIDocShellLoadInfo::loadNormalBypassCache:
      loadType = LOAD_NORMAL_BYPASS_CACHE;
      break;
    case nsIDocShellLoadInfo::loadNormalBypassProxy:
      loadType = LOAD_NORMAL_BYPASS_PROXY;
      break;
    case nsIDocShellLoadInfo::loadNormalBypassProxyAndCache:
      loadType = LOAD_NORMAL_BYPASS_PROXY_AND_CACHE;
      break;
    case nsIDocShellLoadInfo::loadNormalAllowMixedContent:
      loadType = LOAD_NORMAL_ALLOW_MIXED_CONTENT;
      break;
    case nsIDocShellLoadInfo::loadReloadNormal:
      loadType = LOAD_RELOAD_NORMAL;
      break;
    case nsIDocShellLoadInfo::loadReloadCharsetChange:
      loadType = LOAD_RELOAD_CHARSET_CHANGE;
      break;
    case nsIDocShellLoadInfo::loadReloadCharsetChangeBypassCache:
      loadType = LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE;
      break;
    case nsIDocShellLoadInfo::loadReloadCharsetChangeBypassProxyAndCache:
      loadType = LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE;
      break;
    case nsIDocShellLoadInfo::loadReloadBypassCache:
      loadType = LOAD_RELOAD_BYPASS_CACHE;
      break;
    case nsIDocShellLoadInfo::loadReloadBypassProxy:
      loadType = LOAD_RELOAD_BYPASS_PROXY;
      break;
    case nsIDocShellLoadInfo::loadReloadBypassProxyAndCache:
      loadType = LOAD_RELOAD_BYPASS_PROXY_AND_CACHE;
      break;
    case nsIDocShellLoadInfo::loadLink:
      loadType = LOAD_LINK;
      break;
    case nsIDocShellLoadInfo::loadRefresh:
      loadType = LOAD_REFRESH;
      break;
    case nsIDocShellLoadInfo::loadBypassHistory:
      loadType = LOAD_BYPASS_HISTORY;
      break;
    case nsIDocShellLoadInfo::loadStopContent:
      loadType = LOAD_STOP_CONTENT;
      break;
    case nsIDocShellLoadInfo::loadStopContentAndReplace:
      loadType = LOAD_STOP_CONTENT_AND_REPLACE;
      break;
    case nsIDocShellLoadInfo::loadPushState:
      loadType = LOAD_PUSHSTATE;
      break;
    case nsIDocShellLoadInfo::loadReplaceBypassCache:
      loadType = LOAD_REPLACE_BYPASS_CACHE;
      break;
    case nsIDocShellLoadInfo::loadReloadMixedContent:
      loadType = LOAD_RELOAD_ALLOW_MIXED_CONTENT;
      break;
    default:
      NS_NOTREACHED("Unexpected nsDocShellInfoLoadType value");
  }

  return loadType;
}

static inline nsDOMNavigationTiming::Type
ConvertLoadTypeToNavigationType(uint32_t aLoadType)
{
  // Not initialized, assume it's normal load.
  if (aLoadType == 0) {
    aLoadType = LOAD_NORMAL;
  }

  auto result = nsDOMNavigationTiming::TYPE_RESERVED;
  switch (aLoadType) {
    case LOAD_NORMAL:
    case LOAD_NORMAL_EXTERNAL:
    case LOAD_NORMAL_BYPASS_CACHE:
    case LOAD_NORMAL_BYPASS_PROXY:
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
    case LOAD_NORMAL_REPLACE:
    case LOAD_NORMAL_ALLOW_MIXED_CONTENT:
    case LOAD_LINK:
    case LOAD_STOP_CONTENT:
    case LOAD_REPLACE_BYPASS_CACHE:
      result = nsDOMNavigationTiming::TYPE_NAVIGATE;
      break;
    case LOAD_HISTORY:
      result = nsDOMNavigationTiming::TYPE_BACK_FORWARD;
      break;
    case LOAD_RELOAD_NORMAL:
    case LOAD_RELOAD_CHARSET_CHANGE:
    case LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE:
    case LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
    case LOAD_RELOAD_ALLOW_MIXED_CONTENT:
      result = nsDOMNavigationTiming::TYPE_RELOAD;
      break;
    case LOAD_STOP_CONTENT_AND_REPLACE:
    case LOAD_REFRESH:
    case LOAD_BYPASS_HISTORY:
    case LOAD_ERROR_PAGE:
    case LOAD_PUSHSTATE:
      result = nsDOMNavigationTiming::TYPE_RESERVED;
      break;
    default:
      result = nsDOMNavigationTiming::TYPE_RESERVED;
      break;
  }

  return result;
}

#endif // MOZILLA_INTERNAL_API
#endif
