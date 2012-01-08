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

#include "nsContentUtils.h"
#include "nsDOMEventTargetWrapperCache.h"
#include "nsIDocument.h"
#include "nsIJSContextStack.h"
#include "nsServiceManagerUtils.h"
#include "nsDOMJSUtils.h"
#include "nsWrapperCacheInlines.h"

NS_IMPL_CYCLE_COLLECTION_CLASS(nsDOMEventTargetWrapperCache)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsDOMEventTargetWrapperCache)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsDOMEventTargetWrapperCache,
                                                  nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsDOMEventTargetWrapperCache,
                                                nsDOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsDOMEventTargetWrapperCache)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(nsDOMEventTargetWrapperCache, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(nsDOMEventTargetWrapperCache, nsDOMEventTargetHelper)

void
nsDOMEventTargetWrapperCache::Init(JSContext* aCx)
{
  // Set the original mScriptContext and mPrincipal, if available
  JSContext* cx = aCx;
  if (!cx) {
    nsIJSContextStack* stack = nsContentUtils::ThreadJSContextStack();

    if (!stack)
      return;

    if (NS_FAILED(stack->Peek(&cx)) || !cx)
      return;
  }

  NS_ASSERTION(cx, "Should have returned earlier ...");
  nsIScriptContext* context = GetScriptContextFromJSContext(cx);
  if (context) {
    mScriptContext = context;
    nsCOMPtr<nsPIDOMWindow> window =
      do_QueryInterface(context->GetGlobalObject());
    if (window)
      mOwner = window->GetCurrentInnerWindow();
  }
}

nsDOMEventTargetWrapperCache::~nsDOMEventTargetWrapperCache()
{
  nsContentUtils::ReleaseWrapper(this, this);
}
