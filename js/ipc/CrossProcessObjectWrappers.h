/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sw=2 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_CrossProcessObjectWrappers_h__
#define mozilla_jsipc_CrossProcessObjectWrappers_h__

#include "js/TypeDecls.h"
#include "mozilla/jsipc/CpowHolder.h"
#include "mozilla/jsipc/JavaScriptTypes.h"
#include "nsID.h"
#include "nsString.h"
#include "nsTArray.h"

#ifdef XP_WIN
#  undef GetClassName
#  undef GetClassInfo
#endif

namespace mozilla {

namespace dom {
class CPOWManagerGetter;
}  // namespace dom

namespace jsipc {

class PJavaScriptParent;
class PJavaScriptChild;

class CPOWManager {
 public:
  virtual bool Unwrap(JSContext* cx, const nsTArray<CpowEntry>& aCpows,
                      JS::MutableHandleObject objp) = 0;

  virtual bool Wrap(JSContext* cx, JS::HandleObject aObj,
                    nsTArray<CpowEntry>* outCpows) = 0;
};

class CrossProcessCpowHolder : public CpowHolder {
 public:
  CrossProcessCpowHolder(dom::CPOWManagerGetter* managerGetter,
                         const nsTArray<CpowEntry>& cpows);

  ~CrossProcessCpowHolder();

  bool ToObject(JSContext* cx, JS::MutableHandleObject objp) override;

 private:
  CPOWManager* js_;
  const nsTArray<CpowEntry>& cpows_;
  bool unwrapped_;
};

CPOWManager* CPOWManagerFor(PJavaScriptParent* aParent);

CPOWManager* CPOWManagerFor(PJavaScriptChild* aChild);

bool IsCPOW(JSObject* obj);

bool IsWrappedCPOW(JSObject* obj);

nsresult InstanceOf(JSObject* obj, const nsID* id, bool* bp);

bool DOMInstanceOf(JSContext* cx, JSObject* obj, int prototypeID, int depth,
                   bool* bp);

PJavaScriptParent* NewJavaScriptParent();

void ReleaseJavaScriptParent(PJavaScriptParent* parent);

PJavaScriptChild* NewJavaScriptChild();

void ReleaseJavaScriptChild(PJavaScriptChild* child);

void AfterProcessTask();

}  // namespace jsipc
}  // namespace mozilla

#endif  // mozilla_jsipc_CrossProcessObjectWrappers_h__
