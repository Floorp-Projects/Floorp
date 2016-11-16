/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_JavaScriptChild_h_
#define mozilla_jsipc_JavaScriptChild_h_

#include "JavaScriptBase.h"
#include "mozilla/jsipc/PJavaScriptChild.h"

namespace mozilla {
namespace jsipc {

class JavaScriptChild : public JavaScriptBase<PJavaScriptChild>
{
  public:
    JavaScriptChild() : strongReferenceObjIdMinimum_(0) {}
    virtual ~JavaScriptChild();

    bool init();
    void trace(JSTracer* trc);
    void updateWeakPointers();

    void drop(JSObject* obj);

    bool allowMessage(JSContext* cx) override { return true; }

  protected:
    virtual bool isParent() override { return false; }
    virtual JSObject* scopeForTargetObjects() override;

    mozilla::ipc::IPCResult RecvDropTemporaryStrongReferences(const uint64_t& upToObjId) override;

  private:
    bool fail(JSContext* cx, ReturnStatus* rs);
    bool ok(ReturnStatus* rs);

    // JavaScriptChild will keep strong references to JS objects that are
    // referenced by the parent only if their ID is >=
    // strongReferenceObjIdMinimum_.
    uint64_t strongReferenceObjIdMinimum_;
};

} // namespace jsipc
} // namespace mozilla

#endif
