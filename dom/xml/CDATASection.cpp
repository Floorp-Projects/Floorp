/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/CDATASection.h"
#include "mozilla/dom/CDATASectionBinding.h"
#include "mozilla/IntegerPrintfMacros.h"

namespace mozilla::dom {

CDATASection::~CDATASection() = default;

JSObject* CDATASection::WrapNode(JSContext* aCx,
                                 JS::Handle<JSObject*> aGivenProto) {
  return CDATASection_Binding::Wrap(aCx, this, aGivenProto);
}

already_AddRefed<CharacterData> CDATASection::CloneDataNode(
    mozilla::dom::NodeInfo* aNodeInfo, bool aCloneText) const {
  RefPtr<mozilla::dom::NodeInfo> ni = aNodeInfo;
  auto* nim = ni->NodeInfoManager();
  RefPtr<CDATASection> it = new (nim) CDATASection(ni.forget());
  if (aCloneText) {
    it->mText = mText;
  }

  return it.forget();
}

#ifdef MOZ_DOM_LIST
void CDATASection::List(FILE* out, int32_t aIndent) const {
  int32_t index;
  for (index = aIndent; --index >= 0;) fputs("  ", out);

  fprintf(out, "CDATASection refcount=%" PRIuPTR "<", mRefCnt.get());

  nsAutoString tmp;
  ToCString(tmp, 0, mText.GetLength());
  fputs(NS_LossyConvertUTF16toASCII(tmp).get(), out);

  fputs(">\n", out);
}

void CDATASection::DumpContent(FILE* out, int32_t aIndent,
                               bool aDumpAll) const {}
#endif

}  // namespace mozilla::dom
