/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
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

#ifndef _ACCESSIBLE_ACTION_H
#define _ACCESSIBLE_ACTION_H

#include "nsISupports.h"

#include "AccessibleAction.h"

class ia2AccessibleAction: public IAccessibleAction
{
public:

  // IUnknown
  STDMETHODIMP QueryInterface(REFIID, void**);

  // IAccessibleAction
  virtual HRESULT STDMETHODCALLTYPE nActions(
      /* [retval][out] */ long *nActions);

  virtual HRESULT STDMETHODCALLTYPE doAction(
      /* [in] */ long actionIndex);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_description(
      /* [in] */ long actionIndex,
      /* [retval][out] */ BSTR *description);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_keyBinding(
      /* [in] */ long actionIndex,
      /* [in] */ long nMaxBinding,
      /* [length_is][length_is][size_is][size_is][out] */ BSTR **keyBinding,
      /* [retval][out] */ long *nBinding);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_name(
      /* [in] */ long actionIndex,
      /* [retval][out] */ BSTR *name);

  virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_localizedName(
      /* [in] */ long actionIndex,
      /* [retval][out] */ BSTR *localizedName);

};


#define FORWARD_IACCESSIBLEACTION(Class)                                       \
virtual HRESULT STDMETHODCALLTYPE nActions(long *nActions)                     \
{                                                                              \
  return Class::nActions(nActions);                                            \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE doAction(long actionIndex)                   \
{                                                                              \
  return Class::doAction(actionIndex);                                         \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_description(long actionIndex,            \
                                                  BSTR *description)           \
{                                                                              \
  return Class::get_description(actionIndex, description);                     \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_keyBinding(long actionIndex,             \
                                                 long nMaxBinding,             \
                                                 BSTR **keyBinding,            \
                                                 long *nBinding)               \
{                                                                              \
  return Class::get_keyBinding(actionIndex, nMaxBinding, keyBinding, nBinding);\
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_name(long actionIndex, BSTR *name)       \
{                                                                              \
  return Class::get_name(actionIndex, name);                                   \
}                                                                              \
                                                                               \
virtual HRESULT STDMETHODCALLTYPE get_localizedName(long actionIndex,          \
                                                    BSTR *localizedName)       \
{                                                                              \
  return Class::get_localizedName(actionIndex, localizedName);                 \
}                                                                              \
                                                                               \

#endif

