/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoSpecifiedValues.h"

namespace {

#define STYLE_STRUCT(name, checkdata_cb) | NS_STYLE_INHERIT_BIT(name)
const uint64_t ALL_SIDS = 0
#include "nsStyleStructList.h"
;
#undef STYLE_STRUCT

} // anonymous namespace

using namespace mozilla;

ServoSpecifiedValues::ServoSpecifiedValues(nsPresContext* aContext,
                                           RawServoDeclarationBlock* aDecl)

  : GenericSpecifiedValues(StyleBackendType::Servo, aContext, ALL_SIDS)
  ,  mDecl(aDecl)
{

}
