/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_ACCESSIBLE_WRAP_H__
#define __NS_ACCESSIBLE_WRAP_H__

#include "nsCOMPtr.h"
#include "Accessible.h"

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
  int32_t type;     // property type as listed above
  void *oldvalue;
  void *newvalue;
};

namespace mozilla {
namespace a11y {

class MaiHyperlink;

/**
 * AccessibleWrap, and its descendents in atk directory provide the
 * implementation of AtkObject.
 */
class AccessibleWrap : public Accessible
{
public:
  AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~AccessibleWrap();
  void ShutdownAtkObject();

  virtual void Shutdown();

  // return the atk object for this AccessibleWrap
  NS_IMETHOD GetNativeInterface(void **aOutAccessible);
  virtual nsresult HandleAccEvent(AccEvent* aEvent);

  AtkObject * GetAtkObject(void);
  static AtkObject * GetAtkObject(nsIAccessible * acc);

  bool IsValidObject();
    
  // get/set the MaiHyperlink object for this AccessibleWrap
  MaiHyperlink* GetMaiHyperlink(bool aCreate = true);
  void SetMaiHyperlink(MaiHyperlink* aMaiHyperlink);

  static const char * ReturnString(nsAString &aString) {
    static nsCString returnedString;
    returnedString = NS_ConvertUTF16toUTF8(aString);
    return returnedString.get();
  }

protected:

  nsresult FireAtkStateChangeEvent(AccEvent* aEvent, AtkObject *aObject);
  nsresult FireAtkTextChangedEvent(AccEvent* aEvent, AtkObject *aObject);
  nsresult FireAtkShowHideEvent(AccEvent* aEvent, AtkObject *aObject,
                                bool aIsAdded);

  AtkObject *mAtkObject;

private:

  /*
   * do we have text-remove and text-insert signals if not we need to use
   * text-changed see AccessibleWrap::FireAtkTextChangedEvent() and
   * bug 619002
   */
  enum EAvailableAtkSignals {
    eUnknown,
    eHaveNewAtkTextSignals,
    eNoNewAtkSignals
  };

  static EAvailableAtkSignals gAvailableAtkSignals;

  uint16_t CreateMaiInterfaces(void);
};

} // namespace a11y
} // namespace mozilla

#endif /* __NS_ACCESSIBLE_WRAP_H__ */
