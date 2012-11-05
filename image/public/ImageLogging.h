/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ImageLogging_h
#define ImageLogging_h

// In order for FORCE_PR_LOG below to work, we have to define it before the
// first time prlog is #included.
#if defined(PR_LOG)
#error "Must #include ImageLogging.h before before any IPDL-generated files or other files that #include prlog.h."
#endif

#if defined(MOZ_LOGGING)
#define FORCE_PR_LOG
#endif

#include "prlog.h"
#include "prinrval.h"
#include "nsString.h"

#if defined(PR_LOGGING)
// Declared in imgRequest.cpp.
extern PRLogModuleInfo *GetImgLog();

#define GIVE_ME_MS_NOW() PR_IntervalToMilliseconds(PR_IntervalNow())

class LogScope {
public:
  LogScope(PRLogModuleInfo *aLog, void *from, const nsACString &fn) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s {ENTER}\n",
                                   GIVE_ME_MS_NOW(),
                                   mFrom, mFunc.get()));
  }

  /* const char * constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const nsACString &fn,
           const nsDependentCString &paramName, const char *paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%s\") {ENTER}\n",
                                   GIVE_ME_MS_NOW(),
                                   mFrom, mFunc.get(),
                                   paramName.get(),
                                   paramValue));
  }

  /* void ptr constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const nsACString &fn,
           const nsDependentCString &paramName, const void *paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=%p) {ENTER}\n",
                                   GIVE_ME_MS_NOW(),
                                   mFrom, mFunc.get(),
                                   paramName.get(),
                                   paramValue));
  }

  /* int32_t constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const nsACString &fn,
           const nsDependentCString &paramName, int32_t paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%d\") {ENTER}\n",
                                   GIVE_ME_MS_NOW(),
                                   mFrom, mFunc.get(),
                                   paramName.get(),
                                   paramValue));
  }

  /* uint32_t constructor */
  LogScope(PRLogModuleInfo *aLog, void *from, const nsACString &fn,
           const nsDependentCString &paramName, uint32_t paramValue) :
    mLog(aLog), mFrom(from), mFunc(fn)
  {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%d\") {ENTER}\n",
                                   GIVE_ME_MS_NOW(),
                                   mFrom, mFunc.get(),
                                   paramName.get(),
                                   paramValue));
  }


  ~LogScope() {
    PR_LOG(mLog, PR_LOG_DEBUG, ("%d [this=%p] %s {EXIT}\n",
                                   GIVE_ME_MS_NOW(),
                                   mFrom, mFunc.get()));
  }

private:
  PRLogModuleInfo *mLog;
  void *mFrom;
  nsAutoCString mFunc;
};


class LogFunc {
public:
  LogFunc(PRLogModuleInfo *aLog, void *from, const nsDependentCString &fn)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s\n",
                                GIVE_ME_MS_NOW(), from,
                                fn.get()));
  }

  LogFunc(PRLogModuleInfo *aLog, void *from, const nsDependentCString &fn,
          const nsDependentCString &paramName, const char *paramValue)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%s\")\n",
                                GIVE_ME_MS_NOW(), from,
                                fn.get(),
                                paramName.get(), paramValue));
  }

  LogFunc(PRLogModuleInfo *aLog, void *from, const nsDependentCString &fn,
          const nsDependentCString &paramName, const void *paramValue)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%p\")\n",
                                GIVE_ME_MS_NOW(), from,
                                fn.get(),
                                paramName.get(), paramValue));
  }


  LogFunc(PRLogModuleInfo *aLog, void *from, const nsDependentCString &fn,
          const nsDependentCString &paramName, uint32_t paramValue)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s (%s=\"%d\")\n",
                                GIVE_ME_MS_NOW(), from,
                                fn.get(),
                                paramName.get(), paramValue));
  }

};


class LogMessage {
public:
  LogMessage(PRLogModuleInfo *aLog, void *from, const nsDependentCString &fn,
             const nsDependentCString &msg)
  {
    PR_LOG(aLog, PR_LOG_DEBUG, ("%d [this=%p] %s -- %s\n",
                                GIVE_ME_MS_NOW(), from,
                                fn.get(),
                                msg.get()));
  }
};

#define LOG_SCOPE_APPEND_LINE_NUMBER_PASTE(id, line) id ## line
#define LOG_SCOPE_APPEND_LINE_NUMBER_EXPAND(id, line) LOG_SCOPE_APPEND_LINE_NUMBER_PASTE(id, line)
#define LOG_SCOPE_APPEND_LINE_NUMBER(id) LOG_SCOPE_APPEND_LINE_NUMBER_EXPAND(id, __LINE__)

#define LOG_SCOPE(l, s) \
  LogScope LOG_SCOPE_APPEND_LINE_NUMBER(LOG_SCOPE_TMP_VAR) (l,                            \
                                                            static_cast<void *>(this),    \
                                                            NS_LITERAL_CSTRING(s))

#define LOG_SCOPE_WITH_PARAM(l, s, pn, pv) \
  LogScope LOG_SCOPE_APPEND_LINE_NUMBER(LOG_SCOPE_TMP_VAR) (l,                            \
                                                            static_cast<void *>(this),    \
                                                            NS_LITERAL_CSTRING(s),        \
                                                            NS_LITERAL_CSTRING(pn), pv)

#define LOG_FUNC(l, s)                  \
  LogFunc(l,                            \
          static_cast<void *>(this),    \
          NS_LITERAL_CSTRING(s))

#define LOG_FUNC_WITH_PARAM(l, s, pn, pv) \
  LogFunc(l,                              \
          static_cast<void *>(this),      \
          NS_LITERAL_CSTRING(s),          \
          NS_LITERAL_CSTRING(pn), pv)

#define LOG_STATIC_FUNC(l, s)           \
  LogFunc(l,                            \
          nullptr,                       \
          NS_LITERAL_CSTRING(s))

#define LOG_STATIC_FUNC_WITH_PARAM(l, s, pn, pv) \
  LogFunc(l,                             \
          nullptr,                        \
          NS_LITERAL_CSTRING(s),         \
          NS_LITERAL_CSTRING(pn), pv)



#define LOG_MSG(l, s, m)                   \
  LogMessage(l,                            \
             static_cast<void *>(this),    \
             NS_LITERAL_CSTRING(s),        \
             NS_LITERAL_CSTRING(m))

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
