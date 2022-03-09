/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PropertyBagUtils.h"

#include "mozilla/SimpleEnumerator.h"
#include "mozilla/dom/DOMTypes.h"
#include "nsCOMPtr.h"
#include "nsHashPropertyBag.h"
#include "nsID.h"
#include "nsIProperty.h"
#include "nsIURI.h"
#include "nsVariant.h"

using namespace IPC;
using namespace mozilla::dom;

namespace mozilla::ipc {

void IPDLParamTraits<nsIVariant*>::Write(MessageWriter* aWriter,
                                         IProtocol* aActor,
                                         nsIVariant* aParam) {
  IDPLVariant variant;

  variant.type() = aParam->GetDataType();

  switch (variant.type()) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_UINT8:
    case nsIDataType::VTYPE_CHAR: {
      uint8_t value;
      MOZ_ALWAYS_SUCCEEDS(aParam->GetAsUint8(&value));
      variant.data() = value;
      break;
    }
    case nsIDataType::VTYPE_WCHAR:
    case nsIDataType::VTYPE_INT16: {
      int16_t value;
      MOZ_ALWAYS_SUCCEEDS(aParam->GetAsInt16(&value));
      variant.data() = value;
      break;
    }
    case nsIDataType::VTYPE_UINT16: {
      uint16_t value;
      MOZ_ALWAYS_SUCCEEDS(aParam->GetAsUint16(&value));
      variant.data() = value;
      break;
    }
    case nsIDataType::VTYPE_INT32: {
      int32_t value;
      MOZ_ALWAYS_SUCCEEDS(aParam->GetAsInt32(&value));
      variant.data() = value;
      break;
    }
    case nsIDataType::VTYPE_UINT32: {
      uint32_t value;
      MOZ_ALWAYS_SUCCEEDS(aParam->GetAsUint32(&value));
      variant.data() = value;
      break;
    }
    case nsIDataType::VTYPE_FLOAT: {
      float value;
      MOZ_ALWAYS_SUCCEEDS(aParam->GetAsFloat(&value));
      variant.data() = value;
      break;
    }
    case nsIDataType::VTYPE_DOUBLE: {
      double value;
      MOZ_ALWAYS_SUCCEEDS(aParam->GetAsDouble(&value));
      variant.data() = value;
      break;
    }
    case nsIDataType::VTYPE_BOOL: {
      bool value;
      MOZ_ALWAYS_SUCCEEDS(aParam->GetAsBool(&value));
      variant.data() = value;
      break;
    }
    case nsIDataType::VTYPE_ID: {
      nsID value;
      MOZ_ALWAYS_SUCCEEDS(aParam->GetAsID(&value));
      variant.data() = value;
      break;
    }
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS: {
      nsString value;
      MOZ_ALWAYS_SUCCEEDS(aParam->GetAsAString(value));
      variant.data() = value;
      break;
    }
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
    case nsIDataType::VTYPE_UTF8STRING: {
      nsCString value;
      MOZ_ALWAYS_SUCCEEDS(aParam->GetAsACString(value));
      variant.data() = value;
      break;
    }
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS: {
      nsIID* iid;
      nsCOMPtr<nsISupports> value;
      MOZ_ALWAYS_SUCCEEDS(aParam->GetAsInterface(&iid, getter_AddRefs(value)));
      free(iid);
      // We only accept nsIURI and nsIPrincipal interface types, patch welcome.
      if (nsCOMPtr<nsIURI> uri = do_QueryInterface(value)) {
        variant.data() = uri;
      } else if (nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(value)) {
        variant.data() = principal;
      } else if (value) {
        variant.type() = nsIDataType::VTYPE_EMPTY;
        variant.data() = false;  // because we need something.
      } else {
        // Let's pretend like we had a null URI, though how do we know
        // it wasn't a null principal?
        variant.data() = (nsIURI*)nullptr;
      }
      break;
    }
    case nsIDataType::VTYPE_VOID:
    case nsIDataType::VTYPE_EMPTY:
      variant.data() = false;  // because we need something.
      break;
    default:
      MOZ_CRASH("Non handled variant type, patch welcome");
      break;
  }
  WriteIPDLParam(aWriter, aActor, variant);
}

bool IPDLParamTraits<nsIVariant*>::Read(MessageReader* aReader,
                                        IProtocol* aActor,
                                        RefPtr<nsIVariant>* aResult) {
  IDPLVariant value;
  if (!ReadIPDLParam(aReader, aActor, &value)) {
    return false;
  }

  auto variant = MakeRefPtr<nsVariant>();

  switch (value.type()) {
    case nsIDataType::VTYPE_INT8:
    case nsIDataType::VTYPE_UINT8:
      if (value.type() == nsIDataType::VTYPE_INT8) {
        variant->SetAsInt8(value.data().get_uint8_t());
      } else {
        variant->SetAsUint8(value.data().get_uint8_t());
      }
      break;
    case nsIDataType::VTYPE_INT16:
      variant->SetAsInt16(value.data().get_int16_t());
      break;
    case nsIDataType::VTYPE_INT32:
      variant->SetAsInt32(value.data().get_int32_t());
      break;
    case nsIDataType::VTYPE_UINT16:
      variant->SetAsUint16(value.data().get_uint16_t());
      break;
    case nsIDataType::VTYPE_UINT32:
      variant->SetAsUint32(value.data().get_uint32_t());
      break;
    case nsIDataType::VTYPE_FLOAT:
      variant->SetAsFloat(value.data().get_float());
      break;
    case nsIDataType::VTYPE_DOUBLE:
      variant->SetAsDouble(value.data().get_double());
      break;
    case nsIDataType::VTYPE_BOOL:
      variant->SetAsBool(value.data().get_bool());
      break;
    case nsIDataType::VTYPE_CHAR:
      variant->SetAsChar(value.data().get_uint8_t());
      break;
    case nsIDataType::VTYPE_WCHAR:
      variant->SetAsWChar(value.data().get_int16_t());
      break;
    case nsIDataType::VTYPE_ID:
      variant->SetAsID(value.data().get_nsID());
      break;
    case nsIDataType::VTYPE_ASTRING:
    case nsIDataType::VTYPE_WCHAR_STR:
    case nsIDataType::VTYPE_WSTRING_SIZE_IS:
      variant->SetAsAString(value.data().get_nsString());
      break;
    case nsIDataType::VTYPE_CSTRING:
    case nsIDataType::VTYPE_CHAR_STR:
    case nsIDataType::VTYPE_STRING_SIZE_IS:
      variant->SetAsACString(value.data().get_nsCString());
      break;
    case nsIDataType::VTYPE_UTF8STRING:
      variant->SetAsAUTF8String(value.data().get_nsCString());
      break;
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      if (value.data().type() == IPDLVariantValue::TnsIURI) {
        variant->SetAsISupports(value.data().get_nsIURI());
      } else if (value.data().type() == IPDLVariantValue::TnsIPrincipal) {
        variant->SetAsISupports(value.data().get_nsIPrincipal());
      } else {
        MOZ_CRASH("Unexpected interface type");
      }
      break;
    case nsIDataType::VTYPE_VOID:
      variant->SetAsVoid();
      break;
    case nsIDataType::VTYPE_EMPTY:
      break;
    default:
      MOZ_CRASH("Non handled variant type, patch welcome");
      return false;
  }
  *aResult = std::move(variant);
  return true;
}

void IPDLParamTraits<nsIPropertyBag2*>::Write(MessageWriter* aWriter,
                                              IProtocol* aActor,
                                              nsIPropertyBag2* aParam) {
  // We send a nsIPropertyBag as an array of IPDLProperty
  nsTArray<IPDLProperty> bag;

  nsCOMPtr<nsISimpleEnumerator> enumerator;
  if (aParam &&
      NS_SUCCEEDED(aParam->GetEnumerator(getter_AddRefs(enumerator)))) {
    for (auto& property : SimpleEnumerator<nsIProperty>(enumerator)) {
      nsString name;
      nsCOMPtr<nsIVariant> value;
      MOZ_ALWAYS_SUCCEEDS(property->GetName(name));
      MOZ_ALWAYS_SUCCEEDS(property->GetValue(getter_AddRefs(value)));
      bag.AppendElement(IPDLProperty{name, value});
    }
  }
  WriteIPDLParam(aWriter, aActor, bag);
}

bool IPDLParamTraits<nsIPropertyBag2*>::Read(MessageReader* aReader,
                                             IProtocol* aActor,
                                             RefPtr<nsIPropertyBag2>* aResult) {
  nsTArray<IPDLProperty> bag;
  if (!ReadIPDLParam(aReader, aActor, &bag)) {
    return false;
  }

  auto properties = MakeRefPtr<nsHashPropertyBag>();

  for (auto& entry : bag) {
    nsCOMPtr<nsIVariant> variant = std::move(entry.value());
    MOZ_ALWAYS_SUCCEEDS(
        properties->SetProperty(std::move(entry.name()), variant));
  }
  *aResult = std::move(properties);
  return true;
}

}  // namespace mozilla::ipc
