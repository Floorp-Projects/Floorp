/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Indexed Database.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com>
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

#include "IDBDatabaseError.h"

#include "nsDOMClassInfo.h"
#include "nsDOMClassInfoID.h"

USING_INDEXEDDB_NAMESPACE

namespace {

void
GetMessageForErrorCode(PRUint16 aCode,
                       nsAString& aMessage)
{
  switch (aCode) {
    case nsIIDBDatabaseError::NON_TRANSIENT_ERR:
      aMessage.AssignLiteral("This error occurred because an operation was not "
                             "allowed on an object. A retry of the same "
                             "operation would fail unless the cause of the "
                             "error is corrected.");
      break;
    case nsIIDBDatabaseError::NOT_FOUND_ERR:
      aMessage.AssignLiteral("The operation failed because the requested "
                             "database object could not be found. For example, "
                             "an object store did not exist but was being "
                             "opened.");
      break;
    case nsIIDBDatabaseError::CONSTRAINT_ERR:
      aMessage.AssignLiteral("A mutation operation in the transaction failed "
                             "due to a because a constraint was not satisfied. "
                             "For example, an object such as an object store "
                             "or index already exists and a new one was being "
                             "attempted to be created.");
      break;
    case nsIIDBDatabaseError::DATA_ERR:
      aMessage.AssignLiteral("Data provided to an operation does not meet "
                             "requirements.");
      break;
    case nsIIDBDatabaseError::NOT_ALLOWED_ERR:
      aMessage.AssignLiteral("A mutation operation was attempted on a database "
                             "that did not allow mutations.");
      break;
    case nsIIDBDatabaseError::SERIAL_ERR:
      aMessage.AssignLiteral("The operation failed because of the size of the "
                             "data set being returned or because there was a "
                             "problem in serializing or deserializing the "
                             "object being processed.");
      break;
    case nsIIDBDatabaseError::RECOVERABLE_ERR:
      aMessage.AssignLiteral("The operation failed because the database was "
                             "prevented from taking an action. The operation "
                             "might be able to succeed if the application "
                             "performs some recovery steps and retries the "
                             "entire transaction. For example, there was not "
                             "enough remaining storage space, or the storage "
                             "quota was reached and the user declined to give "
                             "more space to the database.");
      break;
    case nsIIDBDatabaseError::TRANSIENT_ERR:
      aMessage.AssignLiteral("The operation failed because of some temporary "
                             "problems. The failed operation might be able to "
                             "succeed when the operation is retried without "
                             "any intervention by application-level "
                             "functionality.");
      break;
    case nsIIDBDatabaseError::TIMEOUT_ERR:
      aMessage.AssignLiteral("A lock for the transaction could not be obtained "
                             "in a reasonable time.");
      break;
    case nsIIDBDatabaseError::DEADLOCK_ERR:
      aMessage.AssignLiteral("The current transaction was automatically rolled "
                             "back by the database becuase of deadlock or "
                             "other transaction serialization failures.");
      break;
    case nsIIDBDatabaseError::UNKNOWN_ERR:
      // Fall through.
    default:
      aMessage.AssignLiteral("The operation failed for reasons unrelated to "
                             "the database itself and not covered by any other "
                             "error code.");
  }
}

} // anonymous namespace

IDBDatabaseError::IDBDatabaseError(PRUint16 aCode)
: mCode(aCode)
{
  GetMessageForErrorCode(mCode, mMessage);
}

NS_IMPL_ADDREF(IDBDatabaseError)
NS_IMPL_RELEASE(IDBDatabaseError)

NS_INTERFACE_MAP_BEGIN(IDBDatabaseError)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIIDBDatabaseError)
  NS_INTERFACE_MAP_ENTRY(nsIIDBDatabaseError)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(IDBDatabaseError)
NS_INTERFACE_MAP_END

DOMCI_DATA(IDBDatabaseError, IDBDatabaseError)

NS_IMETHODIMP
IDBDatabaseError::GetCode(PRUint16* aCode)
{
  *aCode = mCode;
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseError::SetCode(PRUint16 aCode)
{
  mCode = aCode;
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseError::GetMessage(nsAString& aMessage)
{
  aMessage.Assign(mMessage);
  return NS_OK;
}

NS_IMETHODIMP
IDBDatabaseError::SetMessage(const nsAString& aMessage)
{
  mMessage.Assign(aMessage);
  return NS_OK;
}
