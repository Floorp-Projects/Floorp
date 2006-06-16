/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bolian Yin (bolian.yin@sun.com)
 *   John Sun (john.sun@sun.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef __NS_ACCESSIBLE_WRAP_H__
#define __NS_ACCESSIBLE_WRAP_H__

#include "nsCOMPtr.h"
#include "nsAccessible.h"

#include "prlog.h"

#ifdef PR_LOGGING
#define MAI_LOGGING
#endif /* #ifdef PR_LOGGING */

// ATK 1.3.3 or later
#if ATK_MAJOR_VERSION >=2 || \
    (ATK_MAJOR_VERSION == 1 && ATK_MINOR_VERSION >= 4) || \
    (ATK_MAJOR_VERSION == 1 && ATK_MINOR_VERSION == 3 && ATK_REV_VERSION >=3)
#define USE_ATK_ROLE_AUTOCOMPLETE
#endif

// ATK 1.10.1 or later
#if ATK_MAJOR_VERSION >=2 || \
    (ATK_MAJOR_VERSION == 1 && ATK_MINOR_VERSION >= 11) || \
    (ATK_MAJOR_VERSION == 1 && ATK_MINOR_VERSION == 10 && ATK_REV_VERSION >=1)
#define USE_ATK_STATE_REQUIRED
#endif

// ATK 1.11.0 or later
#if ATK_MAJOR_VERSION >=2 || \
    (ATK_MAJOR_VERSION == 1 && ATK_MINOR_VERSION >= 11)
#define USE_ATK_ROLE_CAPTION
#define USE_ATK_ROLE_ENTRY
#define USE_ATK_ROLE_CHART             // XXX not currently used
#define USE_ATK_ROLE_DOCUMENT_FRAME    // XXX not currently used
#define USE_ATK_ROLE_HEADING           // XXX not currently used
#define USE_ATK_ROLE_PAGE              // XXX not currently used
#define USE_ATK_ROLE_SECTION           // XXX not currently used
#define USE_ATK_ROLE_REDUNDANT_OBJECT  // XXX not currently used
#define USE_ATK_OBJECT_ATTRIBUTES      // XXX not currently used
#define USE_ATK_STATE_INVALID_ENTRY    // XXX not currently used
#define USE_ATK_STATE_SUPPORTS_AUTOCOMPLETION   // XXX not currently used
#define USE_ATK_STATE_SELECTABLE_TEXT  // XXX not currently used
#define USE_ATK_DOCUMENT               // XXX not currently used
#endif
  
// ATK 1.12.0 or later
#if ATK_MAJOR_VERSION >=2 || \
    (ATK_MAJOR_VERSION == 1 && ATK_MINOR_VERSION >= 12)
#define USE_ATK_VALUE_MINIMUMINCREMENT // XXX not currently used
#define USE_ATK_STATE_DEFAULT          // XXX not currently used
#define USE_ATK_STATE_VISITED          // XXX not currently used
#define USE_ATK_STATE_ANIMATED         // XXX not currently used
#define USE_ATK_ROLE_FORM              // XXX not currently used
#define USE_ATK_DESCRIPTION_RELATIONS
#endif

struct _AtkObject;
typedef struct _AtkObject AtkObject;

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
    static PRInt32 mAccWrapCreated;
    static PRInt32 mAccWrapDeleted;
#endif

public:
    // return the atk object for this nsAccessibleWrap
    NS_IMETHOD GetNativeInterface(void **aOutAccessible);
    NS_IMETHOD GetExtState(PRUint32 *aState);

    AtkObject * GetAtkObject(void);

    PRBool IsValidObject();

    static void TranslateStates(PRUint32 aState,
                                PRUint32 aExtState,
                                void *aAtkStateSet);

    static const char * ReturnString(nsAString &aString) {
      static nsCString returnedString;
      returnedString = NS_ConvertUTF16toUTF8(aString);
      return returnedString.get();
    }
    
protected:
    AtkObject *mMaiAtkObject;

private:
    PRUint16 CreateMaiInterfaces(void);
};

#endif /* __NS_ACCESSIBLE_WRAP_H__ */
