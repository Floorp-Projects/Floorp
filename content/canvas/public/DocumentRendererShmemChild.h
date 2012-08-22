/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DocumentRendererShmemChild
#define mozilla_dom_DocumentRendererShmemChild

#include "mozilla/ipc/PDocumentRendererShmemChild.h"

class nsIDOMWindow;
struct gfxMatrix;

namespace mozilla {
namespace ipc {

class DocumentRendererShmemChild : public PDocumentRendererShmemChild
{
public:
    DocumentRendererShmemChild();
    virtual ~DocumentRendererShmemChild();

    bool RenderDocument(nsIDOMWindow *window, const int32_t& x,
                        const int32_t& y, const int32_t& w,
                        const int32_t& h, const nsString& aBGColor,
                        const uint32_t& flags, const bool& flush,
                        const gfxMatrix& aMatrix,
                        Shmem& data);

private:

    DISALLOW_EVIL_CONSTRUCTORS(DocumentRendererShmemChild);
};

}
}

#endif
