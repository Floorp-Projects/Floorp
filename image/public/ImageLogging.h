/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ImageLogging_h
#define ImageLogging_h

#include "prlog.h"
#include "prinrval.h"
#include "nsString.h"

#if defined(PR_LOGGING)
// Declared in imgRequest.cpp.
extern PRLogModuleInfo *GetImgLog();

#define GIVE_ME_MS_NOW() PR_IntervalToMilliseconds(PR_IntervalNow())

class LogScope {
public:
  LogScope(PRLogModuleInfo *aLog, void *from, const char *fn) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s {ENTER}\n",
                                   GIVE_ME_MS_NOW(), mFrom, mFunc));
  }

  /* const char * constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const char *fn,
           const char *paramName, const char *paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%s\") {ENTER}\n",
                                   GIVE_ME_MS_NOW(), mFrom, mFunc,
                                   paramName, paramValue));
  }

  /* void ptr constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const char *fn,
           const char *paramName, const void *paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=%p) {ENTER}\n",
                                   GIVE_ME_MS_NOW(), mFrom, mFunc,
                                   paramName, paramValue));
  }

  /* int32_t constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const char *fn,
           const char *paramName, int32_t paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%d\") {ENTER}\n",
                                   GIVE_ME_MS_NOW(), mFrom, mFunc,
                                   paramName, paramValue));
  }

  /* uint32_t constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const char *fn,
           const char *paramName, uint32_t paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%d\") {ENTER}\n",
                                   GIVE_ME_MS_NOW(), mFrom, mFunc,
                                   paramName, paramValue));
  }


  ~LogScope() {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s {EXIT}\n",
                                   GIVE_ME_MS_NOW(), mFrom, mFunc));
  }

private:
  PRLogModuleInfo *mLog;
  void *mFrom;
  const char *mFunc;
};


class LogFunc {
public:
  LogFunc(PRLogModuleInfo *aLog, void *from, const char *fn)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s\n",
                                GIVE_ME_MS_NOW(), from, fn));
  }

  LogFunc(PRLogModuleInfo *aLog, void *from, const char *fn,
          const char *paramName, const char *paramValue)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%s\")\n",
                                GIVE_ME_MS_NOW(), from, fn,
                                paramName, paramValue));
  }

  LogFunc(PRLogModuleInfo *aLog, void *from, const char *fn,
          const char *paramName, const void *paramValue)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%p\")\n",
                                GIVE_ME_MS_NOW(), from, fn,
                                paramName, paramValue));
  }


  LogFunc(PRLogModuleInfo *aLog, void *from, const char *fn,
          const char *paramName, uint32_t paramValue)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%d\")\n",
                                GIVE_ME_MS_NOW(), from,
                                fn,
                                paramName, paramValue));
  }

};


class LogMessage {
public:
  LogMessage(PRLogModuleInfo *aLog, void *from, const char *fn,
             const char *msg)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s -- %s\n",
                                GIVE_ME_MS_NOW(), from, fn, msg));
  }
};

#define LOG_SCOPE_APPEND_LINE_NUMBER_PASTE(id, line) id ## line
#define LOG_SCOPE_APPEND_LINE_NUMBER_EXPAND(id, line) LOG_SCOPE_APPEND_LINE_NUMBER_PASTE(id, line)
#define LOG_SCOPE_APPEND_LINE_NUMBER(id) LOG_SCOPE_APPEND_LINE_NUMBER_EXPAND(id, __LINE__)

#define LOG_SCOPE(l, s) \
  LogScope LOG_SCOPE_APPEND_LINE_NUMBER(LOG_SCOPE_TMP_VAR) (l, this, s)

#define LOG_SCOPE_WITH_PARAM(l, s, pn, pv) \
  LogScope LOG_SCOPE_APPEND_LINE_NUMBER(LOG_SCOPE_TMP_VAR) (l, this, s, pn, pv)

#define LOG_FUNC(l, s) LogFunc(l, this, s)

#define LOG_FUNC_WITH_PARAM(l, s, pn, pv) LogFunc(l, this, s, pn, pv)

#define LOG_STATIC_FUNC(l, s) LogFunc(l, nullptr, s)

#define LOG_STATIC_FUNC_WITH_PARAM(l, s, pn, pv) LogFunc(l, nullptr, s, pn, pv)



#define LOG_MSG(l, s, m) LogMessage(l, this, s, m)

#else

#define LOG_SCOPE(l, s)
#define LOG_SCOPE_WITH_PARAM(l, s, pn, pv)
#define LOG_FUNC(l, s)
#define LOG_FUNC_WITH_PARAM(l, s, pn, pv)
#define LOG_STATIC_FUNC(l, s)
#define LOG_STATIC_FUNC_WITH_PARAM(l, s, pn, pv)
#define LOG_MSG(l, s, m)

#endif // if defined(PR_LOGGING)

#define LOG_MSG_WITH_PARAM LOG_FUNC_WITH_PARAM

#endif // ifndef ImageLogging_h
