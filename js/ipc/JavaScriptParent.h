/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_JavaScriptParent__
#define mozilla_jsipc_JavaScriptParent__

#include "JavaScriptBase.h"
#include "mozilla/jsipc/PJavaScriptParent.h"

namespace mozilla {
namespace jsipc {

class JavaScriptParent : public JavaScriptBase<PJavaScriptParent>
{
  public:
    explicit JavaScriptParent(JSRuntime *rt);
    virtual ~JavaScriptParent();

    bool init();
    void trace(JSTracer *trc);

    void drop(JSObject *obj);

    mozilla::ipc::IProtocol*
    CloneProtocol(Channel* aChannel, ProtocolCloneContext* aCtx) MOZ_OVERRIDE;

  protected:
    virtual bool isParent() { return true; }
    virtual JSObject *defaultScope() MOZ_OVERRIDE;
};

} // jsipc
} // mozilla

#endif // mozilla_jsipc_JavaScriptWrapper_h__

