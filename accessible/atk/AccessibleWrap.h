/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __NS_ACCESSIBLE_WRAP_H__
#define __NS_ACCESSIBLE_WRAP_H__

#include "nsCOMPtr.h"
#include "LocalAccessible.h"

struct _AtkObject;
typedef struct _AtkObject AtkObject;

enum AtkProperty {
  PROP_0,  // gobject convention
  PROP_NAME,
  PROP_DESCRIPTION,
  PROP_PARENT,  // ancestry has changed
  PROP_ROLE,
  PROP_LAYER,
  PROP_MDI_ZORDER,
  PROP_TABLE_CAPTION,
  PROP_TABLE_COLUMN_DESCRIPTION,
  PROP_TABLE_COLUMN_HEADER,
  PROP_TABLE_ROW_DESCRIPTION,
  PROP_TABLE_ROW_HEADER,
  PROP_TABLE_SUMMARY,
  PROP_LAST  // gobject convention
};

struct AtkPropertyChange {
  int32_t type;  // property type as listed above
  void* oldvalue;
  void* newvalue;
};

namespace mozilla {
namespace a11y {

class MaiHyperlink;

/**
 * AccessibleWrap, and its descendents in atk directory provide the
 * implementation of AtkObject.
 */
class AccessibleWrap : public LocalAccessible {
 public:
  AccessibleWrap(nsIContent* aContent, DocAccessible* aDoc);
  virtual ~AccessibleWrap();
  void ShutdownAtkObject();

  virtual void Shutdown() override;

  // return the atk object for this AccessibleWrap
  virtual void GetNativeInterface(void** aOutAccessible) override;
  virtual nsresult HandleAccEvent(AccEvent* aEvent) override;

  AtkObject* GetAtkObject(void);
  static AtkObject* GetAtkObject(LocalAccessible* aAccessible);

  bool IsValidObject();

  static const char* ReturnString(nsAString& aString) {
    static nsCString returnedString;
    CopyUTF16toUTF8(aString, returnedString);
    return returnedString.get();
  }

  static void GetKeyBinding(Accessible* aAccessible, nsAString& aResult);

  static Accessible* GetColumnHeader(TableAccessibleBase* aAccessible,
                                     int32_t aColIdx);
  static Accessible* GetRowHeader(TableAccessibleBase* aAccessible,
                                  int32_t aRowIdx);

 protected:
  nsresult FireAtkStateChangeEvent(AccEvent* aEvent, AtkObject* aObject);
  nsresult FireAtkTextChangedEvent(AccEvent* aEvent, AtkObject* aObject);

  AtkObject* mAtkObject;

 private:
  uint16_t CreateMaiInterfaces();
};

}  // namespace a11y
}  // namespace mozilla

#endif /* __NS_ACCESSIBLE_WRAP_H__ */
