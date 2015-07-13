/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DocumentRendererChild
#define mozilla_dom_DocumentRendererChild

#include "mozilla/ipc/PDocumentRendererChild.h"
#include "nsString.h"
#include "gfxContext.h"

class nsIDOMWindow;

namespace mozilla {
namespace ipc {

class DocumentRendererChild : public PDocumentRendererChild
{
public:
    DocumentRendererChild();
    virtual ~DocumentRendererChild();

    bool RenderDocument(nsIDOMWindow *window,
                        const nsRect& documentRect, const gfx::Matrix& transform,
                        const nsString& bgcolor,
                        uint32_t renderFlags, bool flushLayout,
                        const nsIntSize& renderSize, nsCString& data);

private:

    DISALLOW_EVIL_CONSTRUCTORS(DocumentRendererChild);
};

} // namespace ipc
} // namespace mozilla

#endif
