/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Implementations of DOM Core's nsIDOMComment node.
 */

#include "nsCOMPtr.h"
#include "mozilla/dom/Comment.h"
#include "mozilla/dom/CommentBinding.h"
#include "mozilla/IntegerPrintfMacros.h"

using namespace mozilla;
using namespace dom;

namespace mozilla {
namespace dom {

Comment::~Comment()
{
}

NS_IMPL_ISUPPORTS_INHERITED(Comment, nsGenericDOMDataNode, nsIDOMNode,
                            nsIDOMCharacterData, nsIDOMComment)

bool
Comment::IsNodeOfType(uint32_t aFlags) const
{
  return !(aFlags & ~(eCONTENT | eCOMMENT | eDATA_NODE));
}

nsGenericDOMDataNode*
Comment::CloneDataNode(mozilla::dom::NodeInfo *aNodeInfo, bool aCloneText) const
{
  nsRefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
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

  fprintf(out, "Comment@%p refcount=%" PRIuPTR "<!--", (void*)this, mRefCnt.get());

  nsAutoString tmp;
  ToCString(tmp, 0, mText.GetLength());
  fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);

  fputs("-->\n", out);
}
#endif

/* static */ already_AddRefed<Comment>
Comment::Constructor(const GlobalObject& aGlobal,
                     const nsAString& aData, ErrorResult& aRv)
{
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window || !window->GetDoc()) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  return window->GetDoc()->CreateComment(aData);
}

JSObject*
Comment::WrapNode(JSContext *aCx, JS::Handle<JSObject*> aGivenProto)
{
  return CommentBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
