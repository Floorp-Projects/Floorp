/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDocShellLoadTypes_h_
#define nsDocShellLoadTypes_h_

#ifdef MOZILLA_INTERNAL_API

#  include "nsDOMNavigationTiming.h"
#  include "nsIDocShell.h"
#  include "nsIWebNavigation.h"

/**
 * Load flag for error pages. This uses one of the reserved flag
 * values from nsIWebNavigation.
 */
#  define LOAD_FLAGS_ERROR_PAGE 0x0001U

#  define MAKE_LOAD_TYPE(type, flags) ((type) | ((flags) << 16))
#  define LOAD_TYPE_HAS_FLAGS(type, flags) ((type) & ((flags) << 16))

/**
 * These are flags that confuse ConvertLoadTypeToDocShellLoadInfo and should
 * not be passed to MAKE_LOAD_TYPE.  In particular this includes all flags
 * above 0xffff (e.g. LOAD_FLAGS_BYPASS_CLASSIFIER), since MAKE_LOAD_TYPE would
 * just shift them out anyway.
 */
#  define EXTRA_LOAD_FLAGS                        \
    (nsIWebNavigation::LOAD_FLAGS_FROM_EXTERNAL | \
     nsIWebNavigation::LOAD_FLAGS_FIRST_LOAD |    \
     nsIWebNavigation::LOAD_FLAGS_ALLOW_POPUPS | 0xffff0000)

/* load types are legal combinations of load commands and flags
 *
 * NOTE:
 *  Remember to update the IsValidLoadType function below if you change this
 *  enum to ensure bad flag combinations will be rejected.
 */
enum LoadType : uint32_t {
  LOAD_NORMAL = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL,
                               nsIWebNavigation::LOAD_FLAGS_NONE),
  LOAD_NORMAL_REPLACE =
      MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL,
                     nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY),
  LOAD_HISTORY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_HISTORY,
                                nsIWebNavigation::LOAD_FLAGS_NONE),
  LOAD_NORMAL_BYPASS_CACHE = MAKE_LOAD_TYPE(
      nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
  LOAD_NORMAL_BYPASS_PROXY = MAKE_LOAD_TYPE(
      nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
  LOAD_NORMAL_BYPASS_PROXY_AND_CACHE =
      MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL,
                     nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE |
                         nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
  LOAD_RELOAD_NORMAL = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD,
                                      nsIWebNavigation::LOAD_FLAGS_NONE),
  LOAD_RELOAD_BYPASS_CACHE = MAKE_LOAD_TYPE(
      nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
  LOAD_RELOAD_BYPASS_PROXY = MAKE_LOAD_TYPE(
      nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
  LOAD_RELOAD_BYPASS_PROXY_AND_CACHE =
      MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD,
                     nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE |
                         nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
  LOAD_LINK = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL,
                             nsIWebNavigation::LOAD_FLAGS_IS_LINK),
  LOAD_REFRESH = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL,
                                nsIWebNavigation::LOAD_FLAGS_IS_REFRESH),
  LOAD_REFRESH_REPLACE =
      MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL,
                     nsIWebNavigation::LOAD_FLAGS_IS_REFRESH |
                         nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY),
  LOAD_RELOAD_CHARSET_CHANGE =
      MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD,
                     nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE),
  LOAD_RELOAD_CHARSET_CHANGE_BYPASS_PROXY_AND_CACHE =
      MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD,
                     nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE |
                         nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE |
                         nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
  LOAD_RELOAD_CHARSET_CHANGE_BYPASS_CACHE =
      MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD,
                     nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE |
                         nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
  LOAD_BYPASS_HISTORY =
      MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL,
                     nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY),
  LOAD_STOP_CONTENT = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL,
                                     nsIWebNavigation::LOAD_FLAGS_STOP_CONTENT),
  LOAD_STOP_CONTENT_AND_REPLACE =
      MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL,
                     nsIWebNavigation::LOAD_FLAGS_STOP_CONTENT |
                         nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY),
  LOAD_PUSHSTATE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_PUSHSTATE,
                                  nsIWebNavigation::LOAD_FLAGS_NONE),
  LOAD_REPLACE_BYPASS_CACHE =
      MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL,
                     nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY |
                         nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
  /**
   * Load type for an error page. These loads are never triggered by users of
   * Docshell. Instead, Docshell triggers the load itself when a
   * consumer-triggered load failed.
   */
  LOAD_ERROR_PAGE =
      MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, LOAD_FLAGS_ERROR_PAGE)

  // NOTE: Adding a new value? Remember to update IsValidLoadType!
};

static inline bool IsForceReloadType(uint32_t aLoadType) {
  switch (aLoadType) {
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
      return true;
  }
  return false;
}

static inline bool IsValidLoadType(uint32_t aLoadType) {
  switch (aLoadType) {
    case LOAD_NORMAL:
    case LOAD_NORMAL_REPLACE:
    case LOAD_NORMAL_BYPASS_CACHE:
    case LOAD_NORMAL_BYPASS_PROXY:
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
    case LOAD_HISTORY:
    case LOAD_RELOAD_NORMAL:
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
    case LOAD_LINK:
    case LOAD_REFRESH:
    case LOAD_REFRESH_REPLACE:
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

inline nsDOMNavigationTiming::Type ConvertLoadTypeToNavigationType(
    uint32_t aLoadType) {
  // Not initialized, assume it's normal load.
  if (aLoadType == 0) {
    aLoadType = LOAD_NORMAL;
  }

  auto result = nsDOMNavigationTiming::TYPE_RESERVED;
  switch (aLoadType) {
    case LOAD_NORMAL:
    case LOAD_NORMAL_BYPASS_CACHE:
    case LOAD_NORMAL_BYPASS_PROXY:
    case LOAD_NORMAL_BYPASS_PROXY_AND_CACHE:
    case LOAD_NORMAL_REPLACE:
    case LOAD_LINK:
    case LOAD_STOP_CONTENT:
    // FIXME: It isn't clear that LOAD_REFRESH_REPLACE should have a different
    // navigation type than LOAD_REFRESH. Those loads historically used the
    // LOAD_NORMAL_REPLACE type, and therefore wound up with TYPE_NAVIGATE by
    // default.
    case LOAD_REFRESH_REPLACE:
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

#endif  // MOZILLA_INTERNAL_API
#endif
