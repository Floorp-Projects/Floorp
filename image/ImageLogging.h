/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_image_ImageLogging_h
#define mozilla_image_ImageLogging_h

#include "mozilla/Logging.h"
#include "prinrval.h"

static mozilla::LazyLogModule gImgLog("imgRequest");

#define GIVE_ME_MS_NOW() PR_IntervalToMilliseconds(PR_IntervalNow())

using mozilla::LogLevel;

class LogScope {
public:

  LogScope(mozilla::LogModule* aLog, void* aFrom, const char* aFunc)
    : mLog(aLog)
    , mFrom(aFrom)
    , mFunc(aFunc)
  {
    MOZ_LOG(mLog, LogLevel::Debug, ("%d [this=%p] %s {ENTER}\n",
                                GIVE_ME_MS_NOW(), mFrom, mFunc));
  }

  /* const char * constructor */
  LogScope(mozilla::LogModule* aLog, void* from, const char* fn,
           const char* paramName, const char* paramValue)
    : mLog(aLog)
    , mFrom(from)
    , mFunc(fn)
  {
    MOZ_LOG(mLog, LogLevel::Debug, ("%d [this=%p] %s (%s=\"%s\") {ENTER}\n",
                                 GIVE_ME_MS_NOW(), mFrom, mFunc,
                                 paramName, paramValue));
  }

  /* void ptr constructor */
  LogScope(mozilla::LogModule* aLog, void* from, const char* fn,
           const char* paramName, const void* paramValue)
    : mLog(aLog)
    , mFrom(from)
    , mFunc(fn)
  {
    MOZ_LOG(mLog, LogLevel::Debug, ("%d [this=%p] %s (%s=%p) {ENTER}\n",
                                GIVE_ME_MS_NOW(), mFrom, mFunc,
                                paramName, paramValue));
  }

  /* int32_t constructor */
  LogScope(mozilla::LogModule* aLog, void* from, const char* fn,
           const char* paramName, int32_t paramValue)
    : mLog(aLog)
    , mFrom(from)
    , mFunc(fn)
  {
    MOZ_LOG(mLog, LogLevel::Debug, ("%d [this=%p] %s (%s=\"%d\") {ENTER}\n",
                                GIVE_ME_MS_NOW(), mFrom, mFunc,
                                paramName, paramValue));
  }

  /* uint32_t constructor */
  LogScope(mozilla::LogModule* aLog, void* from, const char* fn,
           const char* paramName, uint32_t paramValue)
    : mLog(aLog)
    , mFrom(from)
    , mFunc(fn)
  {
    MOZ_LOG(mLog, LogLevel::Debug, ("%d [this=%p] %s (%s=\"%d\") {ENTER}\n",
                                GIVE_ME_MS_NOW(), mFrom, mFunc,
                                paramName, paramValue));
  }

  ~LogScope()
  {
    MOZ_LOG(mLog, LogLevel::Debug, ("%d [this=%p] %s {EXIT}\n",
                                GIVE_ME_MS_NOW(), mFrom, mFunc));
  }

private:
  mozilla::LogModule* mLog;
  void* mFrom;
  const char* mFunc;
};

class LogFunc {
public:
  LogFunc(mozilla::LogModule* aLog, void* from, const char* fn)
  {
    MOZ_LOG(aLog, LogLevel::Debug, ("%d [this=%p] %s\n",
                                GIVE_ME_MS_NOW(), from, fn));
  }

  LogFunc(mozilla::LogModule* aLog, void* from, const char* fn,
          const char* paramName, const char* paramValue)
  {
    MOZ_LOG(aLog, LogLevel::Debug, ("%d [this=%p] %s (%s=\"%s\")\n",
                                GIVE_ME_MS_NOW(), from, fn,
                                paramName, paramValue));
  }

  LogFunc(mozilla::LogModule* aLog, void* from, const char* fn,
          const char* paramName, const void* paramValue)
  {
    MOZ_LOG(aLog, LogLevel::Debug, ("%d [this=%p] %s (%s=\"%p\")\n",
                                GIVE_ME_MS_NOW(), from, fn,
                                paramName, paramValue));
  }


  LogFunc(mozilla::LogModule* aLog, void* from, const char* fn,
          const char* paramName, uint32_t paramValue)
  {
    MOZ_LOG(aLog, LogLevel::Debug, ("%d [this=%p] %s (%s=\"%d\")\n",
                                GIVE_ME_MS_NOW(), from, fn,
                                paramName, paramValue));
  }

};


class LogMessage {
public:
  LogMessage(mozilla::LogModule* aLog, void* from, const char* fn,
             const char* msg)
  {
    MOZ_LOG(aLog, LogLevel::Debug, ("%d [this=%p] %s -- %s\n",
                                GIVE_ME_MS_NOW(), from, fn, msg));
  }
};

#define LOG_SCOPE_APPEND_LINE_NUMBER_PASTE(id, line) id ## line
#define LOG_SCOPE_APPEND_LINE_NUMBER_EXPAND(id, line) \
        LOG_SCOPE_APPEND_LINE_NUMBER_PASTE(id, line)
#define LOG_SCOPE_APPEND_LINE_NUMBER(id) \
        LOG_SCOPE_APPEND_LINE_NUMBER_EXPAND(id, __LINE__)

#define LOG_SCOPE(l, s) \
  LogScope LOG_SCOPE_APPEND_LINE_NUMBER(LOG_SCOPE_TMP_VAR) (l, this, s)

#define LOG_SCOPE_WITH_PARAM(l, s, pn, pv) \
  LogScope LOG_SCOPE_APPEND_LINE_NUMBER(LOG_SCOPE_TMP_VAR) (l, this, s, pn, pv)

#define LOG_FUNC(l, s) LogFunc(l, this, s)

#define LOG_FUNC_WITH_PARAM(l, s, pn, pv) LogFunc(l, this, s, pn, pv)

#define LOG_STATIC_FUNC(l, s) LogFunc(l, nullptr, s)

#define LOG_STATIC_FUNC_WITH_PARAM(l, s, pn, pv) LogFunc(l, nullptr, s, pn, pv)

#define LOG_MSG(l, s, m) LogMessage(l, this, s, m)

#define LOG_MSG_WITH_PARAM LOG_FUNC_WITH_PARAM

#endif // mozilla_image_ImageLogging_h
