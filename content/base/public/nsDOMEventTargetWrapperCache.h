/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
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
 * Wellington Fernando de Macedo.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Wellington Fernando de Macedo <wfernandom2004@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsDOMEventTargetWrapperCache_h__
#define nsDOMEventTargetWrapperCache_h__

#include "nsDOMEventTargetHelper.h"
#include "nsWrapperCache.h"
#include "nsIScriptContext.h"


// Base class intended to be used for objets like XMLHttpRequest,
// EventSource and WebSocket.

class nsDOMEventTargetWrapperCache : public nsDOMEventTargetHelper,
                                     public nsWrapperCache
{
public:  
  NS_DECL_ISUPPORTS_INHERITED

  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(nsDOMEventTargetWrapperCache,
                                                         nsDOMEventTargetHelper)
  
  void GetParentObject(nsIScriptGlobalObject **aParentObject)
  {
    if (mOwner) {
      CallQueryInterface(mOwner, aParentObject);
    }
    else {
      *aParentObject = nsnull;
    }
  }

  static nsDOMEventTargetWrapperCache* FromSupports(nsISupports* aSupports)
  {
    nsIDOMEventTarget* target =
      static_cast<nsIDOMEventTarget*>(aSupports);
#ifdef DEBUG
    {
      nsCOMPtr<nsIDOMEventTarget> target_qi =
        do_QueryInterface(aSupports);

      // If this assertion fires the QI implementation for the object in
      // question doesn't use the nsIDOMEventTarget pointer as the
      // nsISupports pointer. That must be fixed, or we'll crash...
      NS_ASSERTION(target_qi == target, "Uh, fix QI!");
    }
#endif

    return static_cast<nsDOMEventTargetWrapperCache*>(target);
  }

  void Init(JSContext* aCx = nsnull);

protected:
  nsDOMEventTargetWrapperCache() : nsDOMEventTargetHelper(), nsWrapperCache() {}
  virtual ~nsDOMEventTargetWrapperCache();
};

#define NS_DECL_EVENT_HANDLER(_event)                                         \
  protected:                                                                  \
    nsRefPtr<nsDOMEventListenerWrapper> mOn##_event##Listener;                \
  public:

#define NS_DECL_AND_IMPL_EVENT_HANDLER(_event)                                \
  protected:                                                                  \
    nsRefPtr<nsDOMEventListenerWrapper> mOn##_event##Listener;                \
  public:                                                                     \
    NS_IMETHOD GetOn##_event(nsIDOMEventListener** a##_event)                 \
    {                                                                         \
      return GetInnerEventListener(mOn##_event##Listener, a##_event);         \
    }                                                                         \
    NS_IMETHOD SetOn##_event(nsIDOMEventListener* a##_event)                  \
    {                                                                         \
      return RemoveAddEventListener(NS_LITERAL_STRING(#_event),               \
                                    mOn##_event##Listener, a##_event);        \
    }

#define NS_IMPL_EVENT_HANDLER(_class, _event)                                 \
  NS_IMETHODIMP                                                               \
  _class::GetOn##_event(nsIDOMEventListener** a##_event)                      \
  {                                                                           \
    return GetInnerEventListener(mOn##_event##Listener, a##_event);           \
  }                                                                           \
  NS_IMETHODIMP                                                               \
  _class::SetOn##_event(nsIDOMEventListener* a##_event)                       \
  {                                                                           \
    return RemoveAddEventListener(NS_LITERAL_STRING(#_event),                 \
                                  mOn##_event##Listener, a##_event);          \
  }

#define NS_IMPL_FORWARD_EVENT_HANDLER(_class, _event, _baseclass)             \
    NS_IMETHODIMP                                                             \
    _class::GetOn##_event(nsIDOMEventListener** a##_event)                    \
    {                                                                         \
      return _baseclass::GetOn##_event(a##_event);                            \
    }                                                                         \
    NS_IMETHODIMP                                                             \
    _class::SetOn##_event(nsIDOMEventListener* a##_event)                     \
    {                                                                         \
      return _baseclass::SetOn##_event(a##_event);                            \
    }

#define NS_CYCLE_COLLECTION_TRAVERSE_EVENT_HANDLER(_event)                    \
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR(mOn##_event##Listener)

#define NS_CYCLE_COLLECTION_UNLINK_EVENT_HANDLER(_event)                      \
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mOn##_event##Listener)
                                    

#endif  // nsDOMEventTargetWrapperCache_h__
