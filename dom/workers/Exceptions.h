/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Web Workers.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef mozilla_dom_workers_exceptions_h__
#define mozilla_dom_workers_exceptions_h__

#include "Workers.h"

#include "jspubtd.h"

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

// FileException Codes
#define FILE_NOT_FOUND_ERR 1
#define FILE_SECURITY_ERR 2
#define FILE_ABORT_ERR 3
#define FILE_NOT_READABLE_ERR 4
#define FILE_ENCODING_ERR 5

BEGIN_WORKERS_NAMESPACE

namespace exceptions {

bool
InitClasses(JSContext* aCx, JSObject* aGlobal);

void
ThrowDOMExceptionForCode(JSContext* aCx, intN aCode);

void
ThrowFileExceptionForCode(JSContext* aCx, intN aCode);

} // namespace exceptions

END_WORKERS_NAMESPACE

#endif // mozilla_dom_workers_exceptions_h__
