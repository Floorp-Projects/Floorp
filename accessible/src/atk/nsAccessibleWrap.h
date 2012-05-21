/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_ACCESSIBLE_WRAP_H__
#define __NS_ACCESSIBLE_WRAP_H__

#include "nsCOMPtr.h"
#include "nsAccessible.h"
#include "prlog.h"

#ifdef PR_LOGGING
#define MAI_LOGGING
#endif /* #ifdef PR_LOGGING */

struct _AtkObject;
typedef struct _AtkObject AtkObject;

enum AtkProperty {
  PROP_0,           // gobject convention
  PROP_NAME,
  PROP_DESCRIPTION,
  PROP_PARENT,      // ancestry has changed
  PROP_ROLE,
  PROP_LAYER,
  PROP_MDI_ZORDER,
  PROP_TABLE_CAPTION,
  PROP_TABLE_COLUMN_DESCRIPTION,
  PROP_TABLE_COLUMN_HEADER,
  PROP_TABLE_ROW_DESCRIPTION,
  PROP_TABLE_ROW_HEADER,
  PROP_TABLE_SUMMARY,
  PROP_LAST         // gobject convention
};

struct AtkPropertyChange {
  PRInt32 type;     // property type as listed above
  void *oldvalue;
  void *newvalue;
};

class MaiHyperlink;

/**
 * nsAccessibleWrap, and its descendents in atk directory provide the
 * implementation of AtkObject.
 */
class nsAccessibleWrap: public nsAccessible
{
public:
  nsAccessibleWrap(nsIContent* aContent, nsDocAccessible* aDoc);
    virtual ~nsAccessibleWrap();
    void ShutdownAtkObject();

    // nsAccessNode
    virtual void Shutdown();

#ifdef MAI_LOGGING
    virtual void DumpnsAccessibleWrapInfo(int aDepth) {}
    static PRInt32 mAccWrapCreated;
    static PRInt32 mAccWrapDeleted;
#endif

    // return the atk object for this nsAccessibleWrap
    NS_IMETHOD GetNativeInterface(void **aOutAccessible);
    virtual nsresult HandleAccEvent(AccEvent* aEvent);

    AtkObject * GetAtkObject(void);
    static AtkObject * GetAtkObject(nsIAccessible * acc);

    bool IsValidObject();
    
    // get/set the MaiHyperlink object for this nsAccessibleWrap
    MaiHyperlink* GetMaiHyperlink(bool aCreate = true);
    void SetMaiHyperlink(MaiHyperlink* aMaiHyperlink);

    static const char * ReturnString(nsAString &aString) {
      static nsCString returnedString;
      returnedString = NS_ConvertUTF16toUTF8(aString);
      return returnedString.get();
    }

protected:
    virtual nsresult FirePlatformEvent(AccEvent* aEvent);

    nsresult FireAtkStateChangeEvent(AccEvent* aEvent, AtkObject *aObject);
    nsresult FireAtkTextChangedEvent(AccEvent* aEvent, AtkObject *aObject);
    nsresult FireAtkShowHideEvent(AccEvent* aEvent, AtkObject *aObject,
                                  bool aIsAdded);

    AtkObject *mAtkObject;

private:

  /*
   * do we have text-remove and text-insert signals if not we need to use
   * text-changed see nsAccessibleWrap::FireAtkTextChangedEvent() and
   * bug 619002
   */
  enum EAvailableAtkSignals {
    eUnknown,
    eHaveNewAtkTextSignals,
    eNoNewAtkSignals
  };

  static EAvailableAtkSignals gAvailableAtkSignals;

    PRUint16 CreateMaiInterfaces(void);
};

#endif /* __NS_ACCESSIBLE_WRAP_H__ */
