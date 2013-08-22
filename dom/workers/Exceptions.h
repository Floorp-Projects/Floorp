/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_workers_exceptions_h__
#define mozilla_dom_workers_exceptions_h__

#include "Workers.h"

// DOMException Codes.
#define INDEX_SIZE_ERR 1
#define DOMSTRING_SIZE_ERR 2
#define HIERARCHY_REQUEST_ERR 3
#define WRONG_DOCUMENT_ERR 4
#define INVALID_CHARACTER_ERR 5
#define NO_DATA_ALLOWED_ERR 6
#define NO_MODIFICATION_ALLOWED_ERR 7
#define NOT_FOUND_ERR 8
#define NOT_SUPPORTED_ERR 9
#define INUSE_ATTRIBUTE_ERR 10
#define INVALID_STATE_ERR 11
#define SYNTAX_ERR 12
#define INVALID_MODIFICATION_ERR 13
#define NAMESPACE_ERR 14
#define INVALID_ACCESS_ERR 15
#define VALIDATION_ERR 16
#define TYPE_MISMATCH_ERR 17
#define SECURITY_ERR 18
#define NETWORK_ERR 19
#define ABORT_ERR 20
#define URL_MISMATCH_ERR 21
#define QUOTA_EXCEEDED_ERR 22
#define TIMEOUT_ERR 23
#define INVALID_NODE_TYPE_ERR 24
#define DATA_CLONE_ERR 25

BEGIN_WORKERS_NAMESPACE

namespace exceptions {

bool
InitClasses(JSContext* aCx, JSObject* aGlobal);

} // namespace exceptions

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_exceptions_h__
