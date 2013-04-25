/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementations of DOM Core's nsIDOMComment node.
 */

#include "nsCOMPtr.h"
#include "mozilla/dom/Element.h" // DOMCI_NODE_DATA
#include "mozilla/dom/Comment.h"
#include "mozilla/dom/CommentBinding.h"

using namespace mozilla;
using namespace dom;

namespace mozilla {
namespace dom {

Comment::~Comment()
{
}

NS_IMPL_ISUPPORTS_INHERITED3(Comment, nsGenericDOMDataNode, nsIDOMNode,
                             nsIDOMCharacterData, nsIDOMComment)

bool
Comment::IsNodeOfType(uint32_t aFlags) const
{
  return !(aFlags & ~(eCONTENT | eCOMMENT | eDATA_NODE));
}

nsGenericDOMDataNode*
Comment::CloneDataNode(nsINodeInfo *aNodeInfo, bool aCloneText) const
{
  nsCOMPtr<nsINodeInfo> ni = aNodeInfo;
  Comment *it = new Comment(ni.forget());
  if (it && aCloneText) {
    it->mText = mText;
  }

  return it;
}

#ifdef DEBUG
void
Comment::List(FILE* out, int32_t aIndent) const
{
  int32_t indx;
  for (indx = aIndent; --indx >= 0; ) fputs("  ", out);

  fprintf(out, "Comment@%p refcount=%d<!--", (void*)this, mRefCnt.get());

  nsAutoString tmp;
  ToCString(tmp, 0, mText.GetLength());
  fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);

  fputs("-->\n", out);
}
#endif

JSObject*
Comment::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aScope)
{
  return CommentBinding::Wrap(aCx, aScope, this);
}

} // namespace dom
} // namespace mozilla
