/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2002 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Bolian Yin (bolian.yin@sun.com)
 *
 * Contributor(s): John Sun (john.sun@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#define MAI_IS_ATK_OBJECT(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                                         MAI_TYPE_ATK_OBJECT))
#define MAI_IS_ATK_OBJECT_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                                         MAI_TYPE_ATK_OBJECT))
#define MAI_ATK_OBJECT_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                                         MAI_TYPE_ATK_OBJECT, \
                                         MaiAtkObjectClass))
GType mai_atk_object_get_type(void);
nsresult CheckMaiAtkObject(AtkObject *aAtkObj);
nsAccessibleWrap *GetAccessibleWrap(AtkObject *aAtkObj);

#endif /* __NS_MAI_H__ */
