/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/InspectorCSSParser.h"

#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/UniquePtr.h"

namespace mozilla::dom {

InspectorCSSParser::InspectorCSSParser(const nsACString& aText)
    : mInput(aText) {
  mParserState = Servo_CSSParser_create(&mInput);
}

UniquePtr<InspectorCSSParser> InspectorCSSParser::Constructor(
    const GlobalObject& aGlobal, const nsACString& aText) {
  return MakeUnique<InspectorCSSParser>(aText);
}

InspectorCSSParser::~InspectorCSSParser() {
  Servo_CSSParser_destroy(mParserState);
  mParserState = nullptr;
}

uint32_t InspectorCSSParser::LineNumber() const { return mLineNumber; }

uint32_t InspectorCSSParser::ColumnNumber() const {
  // mColumnNumber is 1-based, but consumers expect 0-based.
  return mColumnNumber - 1;
}

void InspectorCSSParser::NextToken(Nullable<InspectorCSSToken>& aResult) {
  StyleCSSToken cssToken;
  if (!Servo_CSSParser_NextToken(&mInput, mParserState, &cssToken)) {
    aResult.SetNull();

    mLineNumber = Servo_CSSParser_GetCurrentLine(mParserState);
    mColumnNumber = Servo_CSSParser_GetCurrentColumn(mParserState);

    return;
  }

  InspectorCSSToken& inspectorCssToken = aResult.SetValue();
  inspectorCssToken.mText.Append(cssToken.text);
  inspectorCssToken.mTokenType.Append(cssToken.token_type);
  if (cssToken.has_value) {
    inspectorCssToken.mValue.Append(cssToken.value);
  } else {
    inspectorCssToken.mValue.SetIsVoid(true);
  }
  if (cssToken.has_unit) {
    inspectorCssToken.mUnit.Append(cssToken.unit);
  } else {
    inspectorCssToken.mUnit.SetIsVoid(true);
  }
  if (cssToken.has_number) {
    // Reduce precision to avoid floating point inprecision
    inspectorCssToken.mNumber = round(cssToken.number * 100) / 100.0;
  }

  mLineNumber = cssToken.line;
  mColumnNumber = cssToken.column;
}

}  // namespace mozilla::dom
