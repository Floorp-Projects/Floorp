/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Peter Van der Beken <peterv@netscape.com> (original author)
 *
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

#include "nsCOMPtr.h"
#include "nsIBaseDOMException.h"
#include "nsIException.h"

class nsBaseDOMException : public nsIException,
                           public nsIBaseDOMException
{
public:
  nsBaseDOMException();
  virtual ~nsBaseDOMException();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXCEPTION
  NS_IMETHOD Init(nsresult aNSResult, const char* aName,
                  const char* aMessage,
                  nsIException* aDefaultException);

protected:
  nsresult mResult;
  const char* mName;
  const char* mMessage;
  nsCOMPtr<nsIException> mInner;
};

#define IMPL_INTERNAL_DOM_EXCEPTION_HEAD(classname, ifname)                  \
class classname : public nsBaseDOMException,                                 \
                  public ifname                                              \
{                                                                            \
public:                                                                      \
  classname();                                                               \
  virtual ~classname();                                                      \
                                                                             \
  NS_DECL_ISUPPORTS_INHERITED

#define IMPL_INTERNAL_DOM_EXCEPTION_TAIL(classname, ifname, domname, module, \
                                         mapping_function)                   \
};                                                                           \
                                                                             \
classname::classname() {}                                                    \
classname::~classname() {}                                                   \
                                                                             \
NS_IMPL_ADDREF_INHERITED(classname, nsBaseDOMException)                      \
NS_IMPL_RELEASE_INHERITED(classname, nsBaseDOMException)                     \
NS_CLASSINFO_MAP_BEGIN(domname)                                              \
  NS_CLASSINFO_MAP_ENTRY(nsIException)                                       \
  NS_CLASSINFO_MAP_ENTRY(ifname)                                             \
NS_CLASSINFO_MAP_END                                                         \
NS_INTERFACE_MAP_BEGIN(classname)                                            \
  NS_INTERFACE_MAP_ENTRY(ifname)                                             \
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(domname)                              \
NS_INTERFACE_MAP_END_INHERITING(nsBaseDOMException)                          \
                                                                             \
nsresult                                                                     \
NS_New##domname(nsresult aNSResult, nsIException* aDefaultException,         \
                nsIException** aException);                                  \
nsresult                                                                     \
NS_New##domname(nsresult aNSResult, nsIException* aDefaultException,         \
                nsIException** aException)                                   \
{                                                                            \
  if (!(NS_ERROR_GET_MODULE(aNSResult) == module)) {                         \
    NS_WARNING("Trying to create an exception for the wrong error module."); \
    return NS_ERROR_FAILURE;                                                 \
  }                                                                          \
  const char* name;                                                          \
  const char* message;                                                       \
  mapping_function(aNSResult, &name, &message);                              \
  classname* inst = new classname();                                         \
  NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);                              \
  inst->Init(aNSResult, name, message, aDefaultException);                   \
  *aException = inst;                                                        \
  NS_ADDREF(*aException);                                                    \
  return NS_OK;                                                              \
}
