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

#ifndef __NS_ACCESSIBLE_WRAP_H__
#define __NS_ACCESSIBLE_WRAP_H__

#include "nsCOMPtr.h"
#include "nsAccessible.h"

#include "prlog.h"

extern PRLogModuleInfo *gMaiLog;

#ifdef PR_LOGGING
#define MAI_LOGGING
#endif /* #ifdef PR_LOGGING */

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

struct _AtkObject;
struct _AtkStateSet;
struct _MaiAtkObject;
struct _GHashTable;

typedef struct _AtkObject AtkObject;
typedef struct _AtkStateSet AtkStateSet;
typedef struct _MaiAtkObject MaiAtkObject;
typedef struct _GHashTable GHashTable;

/**
 * nsAccessibleWrap, and its descendents in atk directory provide the
 * implementation of AtkObject.
 */

class nsAccessibleWrap: public nsAccessible
{
public:
    nsAccessibleWrap(nsIDOMNode*, nsIWeakReference *aShell);
    virtual ~nsAccessibleWrap();

#ifdef MAI_LOGGING
    virtual void DumpnsAccessibleWrapInfo(int aDepth) {}
#endif

public:
    virtual AtkObject *GetAtkObject(void);

    static void TranslateStates(PRUint32 aAccState,
                                AtkStateSet *state_set);
protected:
    MaiAtkObject *mMaiAtkObject;

private:
    void CreateMaiInterfaces(void);

};

#ifdef MAI_LOGGING
extern PRInt32 num_created_mai_object;
extern PRInt32 num_deleted_mai_object;
#endif

#endif /* __NS_ACCESSIBLE_WRAP_H__ */
