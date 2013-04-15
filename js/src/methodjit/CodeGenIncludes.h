/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined jsjaeger_codegenincs_h__ && defined JS_METHODJIT
#define jsjaeger_codegenincs_h__

/* Get a label for assertion purposes. Prevent #ifdef clutter. */
#ifdef DEBUG
# define DBGLABEL(name) Label name = masm.label();
# define DBGLABEL_NOMASM(name) Label name = label();
# define DBGLABEL_ASSIGN(name) name = masm.label();
#else
# define DBGLABEL(name)
# define DBGLABEL_NOMASM(name)
# define DBGLABEL_ASSIGN(name)
#endif

#if defined JS_NUNBOX32
# include "NunboxAssembler.h"
#elif defined JS_PUNBOX64
# include "PunboxAssembler.h"
#else
# error "Neither JS_NUNBOX32 nor JS_PUNBOX64 is defined."
#endif

#include "BaseAssembler.h"

#endif /* jsjaeger_codegenincs_h__ */

