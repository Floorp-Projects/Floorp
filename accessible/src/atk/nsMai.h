/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_MAI_H__
#define __NS_MAI_H__

#include <atk/atk.h>
#include <glib.h>
#include <glib-object.h>

#include "nsAccessibleWrap.h"

extern PRLogModuleInfo *gMaiLog;

#ifdef MAI_LOGGING
#define MAI_LOG(level, args) \
PR_BEGIN_MACRO \
    if (!gMaiLog) { \
        gMaiLog = PR_NewLogModule("Mai"); \
        PR_ASSERT(gMaiLog); \
    } \
    PR_LOG(gMaiLog, (level), args); \
PR_END_MACRO
#else
#define MAI_LOG(level, args) 
#endif

#define MAI_LOG_DEBUG(args) MAI_LOG(PR_LOG_DEBUG, args)
#define MAI_LOG_WARNING(args) MAI_LOG(PR_LOG_WARNING, args)
#define MAI_LOG_ERROR(args) MAI_LOG(PR_LOG_ERROR, args)

#define MAI_TYPE_ATK_OBJECT             (mai_atk_object_get_type ())
#define MAI_ATK_OBJECT(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                                         MAI_TYPE_ATK_OBJECT, MaiAtkObject))
#define MAI_ATK_OBJECT_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), \
                                         MAI_TYPE_ATK_OBJECT, \
                                         MaiAtkObjectClass))
#define IS_MAI_OBJECT(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                         MAI_TYPE_ATK_OBJECT))
#define IS_MAI_OBJECT_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                         MAI_TYPE_ATK_OBJECT))
#define MAI_ATK_OBJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                         MAI_TYPE_ATK_OBJECT, \
                                         MaiAtkObjectClass))
GType mai_atk_object_get_type(void);
nsAccessibleWrap *GetAccessibleWrap(AtkObject *aAtkObj);

#endif /* __NS_MAI_H__ */
