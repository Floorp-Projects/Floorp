/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "mozilla/ModuleUtils.h"

#include "nsTextToSubURI.h"
#include "nsUTF8ConverterService.h"
#include "nsConverterInputStream.h"
#include "nsConverterOutputStream.h"
#include "nsScriptableUConv.h"
#include "nsIOutputStream.h"
#include "nsITextToSubURI.h"
#include "nsUConvCID.h"

NS_GENERIC_FACTORY_CONSTRUCTOR(nsTextToSubURI)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUTF8ConverterService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsConverterInputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsConverterOutputStream)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsScriptableUnicodeConverter)

NS_DEFINE_NAMED_CID(NS_TEXTTOSUBURI_CID);
NS_DEFINE_NAMED_CID(NS_CONVERTERINPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_CONVERTEROUTPUTSTREAM_CID);
NS_DEFINE_NAMED_CID(NS_ISCRIPTABLEUNICODECONVERTER_CID);
NS_DEFINE_NAMED_CID(NS_UTF8CONVERTERSERVICE_CID);

static const mozilla::Module::CIDEntry kUConvCIDs[] = {
  { &kNS_TEXTTOSUBURI_CID, false, nullptr, nsTextToSubURIConstructor },
  { &kNS_CONVERTERINPUTSTREAM_CID, false, nullptr, nsConverterInputStreamConstructor },
  { &kNS_CONVERTEROUTPUTSTREAM_CID, false, nullptr, nsConverterOutputStreamConstructor },
  { &kNS_ISCRIPTABLEUNICODECONVERTER_CID, false, nullptr, nsScriptableUnicodeConverterConstructor },
  { &kNS_UTF8CONVERTERSERVICE_CID, false, nullptr, nsUTF8ConverterServiceConstructor },
  { nullptr },
};

static const mozilla::Module::ContractIDEntry kUConvContracts[] = {
  { NS_ITEXTTOSUBURI_CONTRACTID, &kNS_TEXTTOSUBURI_CID },
  { NS_CONVERTERINPUTSTREAM_CONTRACTID, &kNS_CONVERTERINPUTSTREAM_CID },
  { "@mozilla.org/intl/converter-output-stream;1", &kNS_CONVERTEROUTPUTSTREAM_CID },
  { NS_ISCRIPTABLEUNICODECONVERTER_CONTRACTID, &kNS_ISCRIPTABLEUNICODECONVERTER_CID },
  { NS_UTF8CONVERTERSERVICE_CONTRACTID, &kNS_UTF8CONVERTERSERVICE_CID },
  { nullptr }
};

static const mozilla::Module kUConvModule = {
  mozilla::Module::kVersion,
  kUConvCIDs,
  kUConvContracts,
};

NSMODULE_DEFN(nsUConvModule) = &kUConvModule;
