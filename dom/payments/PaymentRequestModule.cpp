/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ModuleUtils.h"
#include "PaymentActionRequest.h"
#include "PaymentRequestService.h"

using mozilla::dom::PaymentActionRequest;
using mozilla::dom::PaymentCreateActionRequest;
using mozilla::dom::PaymentRequestService;

NS_GENERIC_FACTORY_CONSTRUCTOR(PaymentActionRequest)
NS_GENERIC_FACTORY_CONSTRUCTOR(PaymentCreateActionRequest)
NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(PaymentRequestService,
                                         PaymentRequestService::GetSingleton)

NS_DEFINE_NAMED_CID(NS_PAYMENT_ACTION_REQUEST_CID);
NS_DEFINE_NAMED_CID(NS_PAYMENT_CREATE_ACTION_REQUEST_CID);
NS_DEFINE_NAMED_CID(NS_PAYMENT_REQUEST_SERVICE_CID);

static const mozilla::Module::CIDEntry kPaymentRequestCIDs[] = {
  { &kNS_PAYMENT_ACTION_REQUEST_CID, false, nullptr, PaymentActionRequestConstructor},
  { &kNS_PAYMENT_CREATE_ACTION_REQUEST_CID, false, nullptr, PaymentCreateActionRequestConstructor},
  { &kNS_PAYMENT_REQUEST_SERVICE_CID, true, nullptr, PaymentRequestServiceConstructor },
  { nullptr }
};

static const mozilla::Module::ContractIDEntry kPaymentRequestContracts[] = {
  { NS_PAYMENT_ACTION_REQUEST_CONTRACT_ID, &kNS_PAYMENT_ACTION_REQUEST_CID },
  { NS_PAYMENT_CREATE_ACTION_REQUEST_CONTRACT_ID, &kNS_PAYMENT_CREATE_ACTION_REQUEST_CID },
  { NS_PAYMENT_REQUEST_SERVICE_CONTRACT_ID, &kNS_PAYMENT_REQUEST_SERVICE_CID },
  { nullptr }
};

static const mozilla::Module::CategoryEntry kPaymentRequestCategories[] = {
  { "payment-request", "PaymentActionRequest", NS_PAYMENT_ACTION_REQUEST_CONTRACT_ID },
  { "payment-request", "PaymentCreateActionRequest", NS_PAYMENT_CREATE_ACTION_REQUEST_CONTRACT_ID },
  { "payment-request", "PaymentRequestService", NS_PAYMENT_REQUEST_SERVICE_CONTRACT_ID },
  { nullptr }
};

static const mozilla::Module kPaymentRequestModule = {
  mozilla::Module::kVersion,
  kPaymentRequestCIDs,
  kPaymentRequestContracts,
  kPaymentRequestCategories
};

NSMODULE_DEFN(PaymentRequestModule) = &kPaymentRequestModule;
