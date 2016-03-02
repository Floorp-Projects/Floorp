/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/bluetooth/BluetoothCommon.h"
#include "mozilla/dom/bluetooth/BluetoothGattUUIDName.h"
#include "mozilla/dom/BluetoothUUID.h"
#include "mozilla/dom/UnionTypes.h" /* StringOrUnsignedLong */


using namespace mozilla;
using namespace mozilla::dom;

bool BluetoothUUID::sInShutdown = false;
// static
nsDataHashtable<nsStringHashKey, uint32_t>*
  BluetoothUUID::sUUIDServiceTable;
// static
nsDataHashtable<nsStringHashKey, uint32_t>*
  BluetoothUUID::sUUIDCharacteristicTable;
// static
nsDataHashtable<nsStringHashKey, uint32_t>*
  BluetoothUUID::sUUIDDescriptorTable;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(BluetoothUUID, mOwner)
NS_IMPL_CYCLE_COLLECTING_ADDREF(BluetoothUUID)
NS_IMPL_CYCLE_COLLECTING_RELEASE(BluetoothUUID)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(BluetoothUUID)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

BluetoothUUID::BluetoothUUID(nsPIDOMWindowInner* aOwner)
  : mOwner(aOwner)
{
  MOZ_ASSERT(aOwner);
}

BluetoothUUID::~BluetoothUUID()
{
}

JSObject*
BluetoothUUID::WrapObject(JSContext* aCx,
                          JS::Handle<JSObject*> aGivenProto)
{
  return BluetoothUUIDBinding::Wrap(aCx, this, aGivenProto);
}

//static
void BluetoothUUID::HandleShutdown()
{
  sInShutdown = true;
  delete sUUIDServiceTable;
  delete sUUIDCharacteristicTable;
  delete sUUIDDescriptorTable;
  sUUIDServiceTable = nullptr;
  sUUIDCharacteristicTable = nullptr;
  sUUIDDescriptorTable = nullptr;
}

// static
void
BluetoothUUID::InitServiceTable()
{
  size_t length = sizeof(ServiceTable) / sizeof(BluetoothGattUUIDName);
  for (size_t i = 0; i < length; ++i) {
    sUUIDServiceTable->Put(NS_ConvertUTF8toUTF16(ServiceTable[i].name),
                           ServiceTable[i].uuid);
  }
}

// static
void
BluetoothUUID::InitCharacteristicTable()
{
  size_t length = sizeof(CharacteristicTable) / sizeof(BluetoothGattUUIDName);
  for (size_t i = 0; i < length; ++i) {
    sUUIDCharacteristicTable->Put(NS_ConvertUTF8toUTF16(
                                    CharacteristicTable[i].name),
                                  CharacteristicTable[i].uuid);
  }
}

// static
void
BluetoothUUID::InitDescriptorTable()
{
  size_t length = sizeof(DescriptorTable) / sizeof(BluetoothGattUUIDName);
  for (size_t i = 0; i < length; ++i) {
    sUUIDDescriptorTable->Put(NS_ConvertUTF8toUTF16(DescriptorTable[i].name),
                              DescriptorTable[i].uuid);
  }
}

/**
 * Check whether the UUID(aString) is valid or not
 */
bool IsValidUUID(const nsAString& aString)
{
  size_t length = aString.Length();
  if (length != 36) {
    return false;
  }

  const char16_t* uuid = aString.BeginReading();

  for (size_t i = 0; i < length; ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (uuid[i] != '-') {
        return false;
      }
    } else if ((uuid[i] < '0' || uuid[i] > '9') &&
               (uuid[i] < 'a' || uuid[i] > 'f')) {
      return false;
    }
  }

  return true;
}

// static
void
BluetoothUUID::ResolveUUIDName(const GlobalObject& aGlobal,
                               const StringOrUnsignedLong& aName,
                               GattAttribute aAttr, nsAString& aReturn,
                               ErrorResult& aRv)
{
  aReturn.Truncate();

  if (aName.IsUnsignedLong()) {
    // aName is a 16-bit or 32-bit UUID alias.
    CanonicalUUID(aGlobal, aName.GetAsUnsignedLong(), aReturn);
  } else if (aName.IsString()) {
    uint32_t alias = 0;
    nsString aString(aName.GetAsString());

    if (IsValidUUID(aString)) {
      // aName is a valid UUID String.
      aReturn.Assign(aString);
    } else if (GetTable(aAttr, aString, alias)) {
      // The UUID string can be mapped to a known UUID alias.
      CanonicalUUID(aGlobal, alias, aReturn);
    } else {
      // Exception, Syntax error, assign aReturn the error message
      aRv.ThrowDOMException(NS_ERROR_DOM_SYNTAX_ERR,
                            NS_LITERAL_CSTRING("Invalid name: It can be a"
                              " 32-bit UUID alias, 128-bit valid UUID "
                              "(lower-case hex characters) or known "
                              "Service/Characteristic/Descriptor name."));

      return ;
    }
  } else {
    MOZ_ASSERT(false, "Invalid name: It can be a 32-bit UUID alias, 128-bit "
                      "valid UUID (lower-case hex characters) or known "
                      "Service/Characteristic/Descriptor name.");
  }
}

// static
bool
BluetoothUUID::GetTable(GattAttribute aAttr, const nsAString& aString,
                        uint32_t& aAlias)
{
  // If we're in shutdown, don't create a new instance.
  NS_ENSURE_FALSE(sInShutdown, false);

  nsDataHashtable<nsStringHashKey, uint32_t>** tableSlot;

  if (aAttr == SERVICE) {
    tableSlot = &sUUIDServiceTable;
  } else if (aAttr == CHARACTERISTIC) {
    tableSlot = &sUUIDCharacteristicTable;
  } else if (aAttr == DESCRIPTOR) {
    tableSlot = &sUUIDDescriptorTable;
  }

  if (!*tableSlot) {
    (*tableSlot) = new nsDataHashtable<nsStringHashKey, uint32_t>;
    if (aAttr == SERVICE) {
      InitServiceTable();
    } else if (aAttr == CHARACTERISTIC) {
      InitCharacteristicTable();
    } else if (aAttr == DESCRIPTOR) {
      InitDescriptorTable();
    }
  }

  return (*tableSlot)->Get(aString, &aAlias);
}

// static
void
BluetoothUUID::GetService(const GlobalObject& aGlobal,
                          const StringOrUnsignedLong& aName,
                          nsAString& aReturn, ErrorResult& aRv)
{
  ResolveUUIDName(aGlobal, aName, SERVICE, aReturn, aRv);
}

// static
void
BluetoothUUID::GetCharacteristic(const GlobalObject& aGlobal,
                                 const StringOrUnsignedLong& aName,
                                 nsAString& aReturn, ErrorResult& aRv)
{
  ResolveUUIDName(aGlobal, aName, CHARACTERISTIC, aReturn, aRv);
}

// static
void
BluetoothUUID::GetDescriptor(const GlobalObject& aGlobal,
                             const StringOrUnsignedLong& aName,
                             nsAString& aReturn, ErrorResult& aRv)
{
  ResolveUUIDName(aGlobal, aName, DESCRIPTOR, aReturn, aRv);
}

// static
void
BluetoothUUID::CanonicalUUID(const GlobalObject& aGlobal, uint32_t aAlias,
                             nsAString& aReturn)
{
  char uuidStr[37];

  // Convert to 128-bit UUID: alias + "-0000-1000-8000-00805f9b34fb".
  snprintf(uuidStr, sizeof(uuidStr), "%.8x-0000-1000-8000-00805f9b34fb",
           aAlias);

  aReturn.AssignLiteral(uuidStr);
}
