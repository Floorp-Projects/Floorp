/* -*- Mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; tab-width: 40 -*- */
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
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
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

#ifndef __NSDOMWORKERMACROS_H__
#define __NSDOMWORKERMACROS_H__

// Macro to generate nsIClassInfo methods for these threadsafe DOM classes
#define NS_IMPL_THREADSAFE_DOM_CI_GETINTERFACES(_class)                       \
NS_IMETHODIMP                                                                 \
_class::GetInterfaces(PRUint32* _count, nsIID*** _array)                      \
{                                                                             \
  return NS_CI_INTERFACE_GETTER_NAME(_class)(_count, _array);                 \
}                                                                             \

#define NS_IMPL_THREADSAFE_DOM_CI_HELPER(_class)                              \
NS_IMETHODIMP                                                                 \
_class::GetHelperForLanguage(PRUint32 _language, nsISupports** _retval)       \
{                                                                             \
  *_retval = nsnull;                                                          \
  return NS_OK;                                                               \
}

#define NS_IMPL_THREADSAFE_DOM_CI_ALL_THE_REST(_class)                        \
NS_IMETHODIMP                                                                 \
_class::GetContractID(char** _contractID)                                     \
{                                                                             \
  *_contractID = nsnull;                                                      \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetClassDescription(char** _classDescription)                         \
{                                                                             \
  *_classDescription = nsnull;                                                \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetClassID(nsCID** _classID)                                          \
{                                                                             \
  *_classID = nsnull;                                                         \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetImplementationLanguage(PRUint32* _language)                        \
{                                                                             \
  *_language = nsIProgrammingLanguage::CPLUSPLUS;                             \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetFlags(PRUint32* _flags)                                            \
{                                                                             \
  *_flags = nsIClassInfo::THREADSAFE | nsIClassInfo::DOM_OBJECT;              \
  return NS_OK;                                                               \
}                                                                             \
                                                                              \
NS_IMETHODIMP                                                                 \
_class::GetClassIDNoAlloc(nsCID* _classIDNoAlloc)                             \
{                                                                             \
  return NS_ERROR_NOT_AVAILABLE;                                              \
}

#define NS_IMPL_THREADSAFE_DOM_CI(_class)                                     \
NS_IMPL_THREADSAFE_DOM_CI_GETINTERFACES(_class)                               \
NS_IMPL_THREADSAFE_DOM_CI_HELPER(_class)                                      \
NS_IMPL_THREADSAFE_DOM_CI_ALL_THE_REST(_class)

#define NS_FORWARD_NSICLASSINFO_NOGETINTERFACES(_to)                          \
  NS_IMETHOD GetHelperForLanguage(PRUint32 aLanguage, nsISupports** _retval)  \
    { return _to GetHelperForLanguage(aLanguage, _retval); }                  \
  NS_IMETHOD GetContractID(char** aContractID)                                \
    { return _to GetContractID(aContractID); }                                \
  NS_IMETHOD GetClassDescription(char** aClassDescription)                    \
    { return _to GetClassDescription(aClassDescription); }                    \
  NS_IMETHOD GetClassID(nsCID** aClassID)                                     \
    { return _to GetClassID(aClassID); }                                      \
  NS_IMETHOD GetImplementationLanguage(PRUint32* aImplementationLanguage)     \
    { return _to GetImplementationLanguage(aImplementationLanguage); }        \
  NS_IMETHOD GetFlags(PRUint32* aFlags)                                       \
    { return _to GetFlags(aFlags); }                                          \
  NS_IMETHOD GetClassIDNoAlloc(nsCID* aClassIDNoAlloc)                        \
    { return _to GetClassIDNoAlloc(aClassIDNoAlloc); }

#define NS_DECL_NSICLASSINFO_GETINTERFACES                                    \
  NS_IMETHOD GetInterfaces(PRUint32* aCount, nsIID*** aArray);

// Don't know why nsISupports.idl defines this out...
#define NS_FORWARD_NSISUPPORTS(_to)                                           \
  NS_IMETHOD QueryInterface(const nsIID& uuid, void** result) {               \
    return _to QueryInterface(uuid, result);                                  \
  }                                                                           \
  NS_IMETHOD_(nsrefcnt) AddRef(void) { return _to AddRef(); }                 \
  NS_IMETHOD_(nsrefcnt) Release(void) { return _to Release(); }

#define JSON_PRIMITIVE_PROPNAME                                               \
  "primitive"

#endif /* __NSDOMWORKERMACROS_H__ */
