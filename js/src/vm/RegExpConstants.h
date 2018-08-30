/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_RegExpConstants_h
#define vm_RegExpConstants_h

#include "builtin/SelfHostingDefines.h"

namespace js {

enum RegExpFlag : uint8_t
{
    IgnoreCaseFlag  = 0x01,
    GlobalFlag      = 0x02,
    MultilineFlag   = 0x04,
    StickyFlag      = 0x08,
    UnicodeFlag     = 0x10,

    NoFlags         = 0x00,
    AllFlags        = 0x1f
};

static_assert(IgnoreCaseFlag == REGEXP_IGNORECASE_FLAG &&
              GlobalFlag == REGEXP_GLOBAL_FLAG &&
              MultilineFlag == REGEXP_MULTILINE_FLAG &&
              StickyFlag == REGEXP_STICKY_FLAG &&
              UnicodeFlag == REGEXP_UNICODE_FLAG,
              "Flag values should be in sync with self-hosted JS");

enum RegExpRunStatus
{
    RegExpRunStatus_Error,
    RegExpRunStatus_Success,
    RegExpRunStatus_Success_NotFound
};

} /* namespace js */

#endif /* vm_RegExpConstants_h */
