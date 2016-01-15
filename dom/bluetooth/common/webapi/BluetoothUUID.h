/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BluetoothUUID_h
#define mozilla_dom_BluetoothUUID_h

#include "mozilla/dom/BluetoothUUIDBinding.h"
#include "mozilla/StaticPtr.h"
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsIObserver.h"
#include "nsPIDOMWindow.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class StringOrUnsignedLong;

class BluetoothUUID final : public nsISupports
                          , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(BluetoothUUID)

  nsPIDOMWindowInner* GetParentObject() const
  {
    return mOwner;
  }

  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  /**
   * Delete sUUIDServiceTable, sUUIDCharacteristicTable, sUUIDDescriptorTable
   * used by ResolveUUIDName(). To be called from nsLayoutStatics only.
   */
  static void HandleShutdown();

  static void GetService(const GlobalObject& aGlobal,
                         const StringOrUnsignedLong& aName,
                         nsAString& aReturn, ErrorResult& aRv);
  static void GetCharacteristic(const GlobalObject& aGlobal,
                                const StringOrUnsignedLong& aName,
                                nsAString& aReturn, ErrorResult& aRv);
  static void GetDescriptor(const GlobalObject& aGlobal,
                            const StringOrUnsignedLong& aName,
                            nsAString& aReturn, ErrorResult& aRv);

  static void CanonicalUUID(const GlobalObject& aGlobal, uint32_t aAlias,
                            nsAString& aReturn);

private:
  BluetoothUUID(nsPIDOMWindowInner* aOwner);
  ~BluetoothUUID();

  static void InitServiceTable();
  static void InitCharacteristicTable();
  static void InitDescriptorTable();

  enum GattAttribute {
    SERVICE,
    CHARACTERISTIC,
    DESCRIPTOR
  };

  /**
   * Convert an UUID string or 16-bit/32-bit UUID alias to a 128-bit UUID based
   * on its bluetooth attribute.
   *
   * @param aGlobal [in] a Global object for static attributes. We have this
   *                     argument because it needs to call |CanonicalUUID|.
   * @param aName   [in] an UUID string or 16-bit/32-bit UUID alias.
   * @param aAttr   [in] a GATT Attribute type.
   * @param aReturn [out] a 128-bit UUID.
   */
  static void ResolveUUIDName(const GlobalObject& aGlobal,
                              const StringOrUnsignedLong& aName,
                              GattAttribute aAttr, nsAString& aReturn,
                              ErrorResult& aRv);

  /**
   * Get the GATT attribute table based on the GATT attribute and check whether
   * the UUID string can be mapped to a known alias.
   *
   * @param aAttr   [in] a GATT Attribute type.
   * @param aString [in] an UUID string.
   * @param aAlias  [out] a known UUID alias if success.
   * @return success if a UUID string can be mapped in table, fail if it
   *         cannot.
   */
  static bool GetTable(GattAttribute aAttr, const nsAString& aString,
                       uint32_t& aAlias);

  /****************************************************************************
   * Variables
   ***************************************************************************/
  nsCOMPtr<nsPIDOMWindowInner> mOwner;

  static bool sInShutdown;

  /**
   * Hash Table of services for mapping service names to its UUID's prefix.
   */
  static nsDataHashtable<nsStringHashKey, uint32_t>* sUUIDServiceTable;

  /**
   * Hash Table of characteristics for mapping characteristic names to its UUID's prefix.
   */
  static nsDataHashtable<nsStringHashKey, uint32_t>* sUUIDCharacteristicTable;

  /**
   * Hash Table of descriptors for mapping descriptor names to its UUID's prefix.
   */
  static nsDataHashtable<nsStringHashKey, uint32_t>* sUUIDDescriptorTable;
};

} // dom
} // mozilla

#endif // mozilla_dom_BluetoothUUID_h
