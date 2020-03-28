/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/ThrowMsgKind.h"

#include "mozilla/Assertions.h"  // MOZ_CRASH

#include "jsfriendapi.h"  // JSErrNum, JSMSG_*

JSErrNum js::ThrowMsgKindToErrNum(ThrowMsgKind kind) {
  switch (kind) {
    case ThrowMsgKind::AssignToCall:
      return JSMSG_ASSIGN_TO_CALL;
    case ThrowMsgKind::IteratorNoThrow:
      return JSMSG_ITERATOR_NO_THROW;
    case ThrowMsgKind::CantDeleteSuper:
      return JSMSG_CANT_DELETE_SUPER;
  }

  MOZ_CRASH("Unexpected message kind");
}
