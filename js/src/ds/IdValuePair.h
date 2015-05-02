/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ds_IdValuePair_h
#define ds_IdValuePair_h

#include "jsapi.h"

#include "NamespaceImports.h"
#include "js/Id.h"

namespace js {

struct IdValuePair
{
    jsid id;
    Value value;

    IdValuePair()
      : id(JSID_EMPTY), value(UndefinedValue())
    {}
    explicit IdValuePair(jsid idArg)
      : id(idArg), value(UndefinedValue())
    {}
    IdValuePair(jsid idArg, Value valueArg)
      : id(idArg), value(valueArg)
    {}
};

class MOZ_STACK_CLASS AutoIdValueVector : public JS::AutoVectorRooterBase<IdValuePair>
{
  public:
    explicit AutoIdValueVector(ContextFriendFields* cx
                               MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
        : AutoVectorRooterBase<IdValuePair>(cx, IDVALVECTOR)
    {
        MOZ_GUARD_OBJECT_NOTIFIER_INIT;
    }

    MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* namespace js */

#endif /* ds_IdValuePair_h */
