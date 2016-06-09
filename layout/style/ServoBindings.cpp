/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoBindings.h"

#include "gfxFontFamilyList.h"
#include "nsAttrValueInlines.h"
#include "nsCSSRuleProcessor.h"
#include "nsContentUtils.h"
#include "nsDOMTokenList.h"
#include "nsIDOMNode.h"
#include "nsIDocument.h"
#include "nsINode.h"
#include "nsIPrincipal.h"
#include "nsNameSpaceManager.h"
#include "nsString.h"
#include "nsStyleStruct.h"
#include "StyleStructContext.h"

#include "mozilla/EventStates.h"
#include "mozilla/dom/Element.h"

uint32_t
Gecko_ChildrenCount(RawGeckoNode* aNode)
{
  return aNode->GetChildCount();
}

bool
Gecko_NodeIsElement(RawGeckoNode* aNode)
{
  return aNode->IsElement();
}

RawGeckoNode*
Gecko_GetParentNode(RawGeckoNode* aNode)
{
  return aNode->GetParentNode();
}

RawGeckoNode*
Gecko_GetFirstChild(RawGeckoNode* aNode)
{
  return aNode->GetFirstChild();
}

RawGeckoNode*
Gecko_GetLastChild(RawGeckoNode* aNode)
{
  return aNode->GetLastChild();
}

RawGeckoNode*
Gecko_GetPrevSibling(RawGeckoNode* aNode)
{
  return aNode->GetPreviousSibling();
}

RawGeckoNode*
Gecko_GetNextSibling(RawGeckoNode* aNode)
{
  return aNode->GetNextSibling();
}

RawGeckoElement*
Gecko_GetParentElement(RawGeckoElement* aElement)
{
  return aElement->GetParentElement();
}

RawGeckoElement*
Gecko_GetFirstChildElement(RawGeckoElement* aElement)
{
  return aElement->GetFirstElementChild();
}

RawGeckoElement* Gecko_GetLastChildElement(RawGeckoElement* aElement)
{
  return aElement->GetLastElementChild();
}

RawGeckoElement*
Gecko_GetPrevSiblingElement(RawGeckoElement* aElement)
{
  return aElement->GetPreviousElementSibling();
}

RawGeckoElement*
Gecko_GetNextSiblingElement(RawGeckoElement* aElement)
{
  return aElement->GetNextElementSibling();
}

RawGeckoElement*
Gecko_GetDocumentElement(RawGeckoDocument* aDoc)
{
  return aDoc->GetDocumentElement();
}

uint8_t
Gecko_ElementState(RawGeckoElement* aElement)
{
  return aElement->StyleState().GetInternalValue() &
         ((1 << (NS_EVENT_STATE_HIGHEST_SERVO_BIT + 1)) - 1);
}

bool
Gecko_IsHTMLElementInHTMLDocument(RawGeckoElement* aElement)
{
  return aElement->IsHTMLElement() && aElement->OwnerDoc()->IsHTMLDocument();
}

bool
Gecko_IsLink(RawGeckoElement* aElement)
{
  return nsCSSRuleProcessor::IsLink(aElement);
}

bool
Gecko_IsTextNode(RawGeckoNode* aNode)
{
  return aNode->NodeInfo()->NodeType() == nsIDOMNode::TEXT_NODE;
}

bool
Gecko_IsVisitedLink(RawGeckoElement* aElement)
{
  return aElement->StyleState().HasState(NS_EVENT_STATE_VISITED);
}

bool
Gecko_IsUnvisitedLink(RawGeckoElement* aElement)
{
  return aElement->StyleState().HasState(NS_EVENT_STATE_UNVISITED);
}

bool
Gecko_IsRootElement(RawGeckoElement* aElement)
{
  return aElement->OwnerDoc()->GetRootElement() == aElement;
}

nsIAtom*
Gecko_LocalName(RawGeckoElement* aElement)
{
  return aElement->NodeInfo()->NameAtom();
}

nsIAtom*
Gecko_Namespace(RawGeckoElement* aElement)
{
  int32_t id = aElement->NodeInfo()->NamespaceID();
  return nsContentUtils::NameSpaceManager()->NameSpaceURIAtom(id);
}

nsIAtom*
Gecko_GetElementId(RawGeckoElement* aElement)
{
  const nsAttrValue* attr = aElement->GetParsedAttr(nsGkAtoms::id);
  return attr ? attr->GetAtomValue() : nullptr;
}

uint32_t
Gecko_ClassOrClassList(RawGeckoElement* aElement,
                       nsIAtom** aClass, nsIAtom*** aClassList)
{
  const nsAttrValue* attr = aElement->GetParsedAttr(nsGkAtoms::_class);
  if (!attr) {
    return 0;
  }

  // For class values with only whitespace, Gecko just stores a string. For the
  // purposes of the style system, there is no class in this case.
  if (attr->Type() == nsAttrValue::eString) {
    MOZ_ASSERT(nsContentUtils::TrimWhitespace<nsContentUtils::IsHTMLWhitespace>(
                 attr->GetStringValue()).IsEmpty());
    return 0;
  }

  // Single tokens are generally stored as an atom. Check that case.
  if (attr->Type() == nsAttrValue::eAtom) {
    *aClass = attr->GetAtomValue();
    return 1;
  }

  // At this point we should have an atom array. It is likely, but not
  // guaranteed, that we have two or more elements in the array.
  MOZ_ASSERT(attr->Type() == nsAttrValue::eAtomArray);
  nsTArray<nsCOMPtr<nsIAtom>>* atomArray = attr->GetAtomArrayValue();
  uint32_t length = atomArray->Length();

  // Special case: zero elements.
  if (length == 0) {
    return 0;
  }

  // Special case: one element.
  if (length == 1) {
    *aClass = atomArray->ElementAt(0);
    return 1;
  }

  // General case: Two or more elements.
  //
  // Note: We could also expose this array as an array of nsCOMPtrs, since
  // bindgen knows what those look like, and eliminate the reinterpret_cast.
  // But it's not obvious that that would be preferable.
  static_assert(sizeof(nsCOMPtr<nsIAtom>) == sizeof(nsIAtom*), "Bad simplification");
  static_assert(alignof(nsCOMPtr<nsIAtom>) == alignof(nsIAtom*), "Bad simplification");

  nsCOMPtr<nsIAtom>* elements = atomArray->Elements();
  nsIAtom** rawElements = reinterpret_cast<nsIAtom**>(elements);
  *aClassList = rawElements;
  return atomArray->Length();
}

ServoNodeData*
Gecko_GetNodeData(RawGeckoNode* aNode)
{
  return aNode->GetServoNodeData();
}

void
Gecko_SetNodeData(RawGeckoNode* aNode, ServoNodeData* aData)
{
  aNode->SetServoNodeData(aData);
}

nsIAtom*
Gecko_Atomize(const char* aString, uint32_t aLength)
{
  return NS_Atomize(nsDependentCSubstring(aString, aLength)).take();
}

void
Gecko_AddRefAtom(nsIAtom* aAtom)
{
  NS_ADDREF(aAtom);
}

void
Gecko_ReleaseAtom(nsIAtom* aAtom)
{
  NS_RELEASE(aAtom);
}

uint32_t
Gecko_HashAtom(nsIAtom* aAtom)
{
  return aAtom->hash();
}

const uint16_t*
Gecko_GetAtomAsUTF16(nsIAtom* aAtom, uint32_t* aLength)
{
  static_assert(sizeof(char16_t) == sizeof(uint16_t), "Servo doesn't know what a char16_t is");
  MOZ_ASSERT(aAtom);
  *aLength = aAtom->GetLength();

  // We need to manually cast from char16ptr_t to const char16_t* to handle the
  // MOZ_USE_CHAR16_WRAPPER we use on WIndows.
  return reinterpret_cast<const uint16_t*>(static_cast<const char16_t*>(aAtom->GetUTF16String()));
}

bool
Gecko_AtomEqualsUTF8(nsIAtom* aAtom, const char* aString, uint32_t aLength)
{
  // XXXbholley: We should be able to do this without converting, I just can't
  // find the right thing to call.
  nsAutoString atomStr;
  aAtom->ToString(atomStr);
  NS_ConvertUTF8toUTF16 inStr(nsDependentCSubstring(aString, aLength));
  return atomStr.Equals(inStr);
}

bool
Gecko_AtomEqualsUTF8IgnoreCase(nsIAtom* aAtom, const char* aString, uint32_t aLength)
{
  // XXXbholley: We should be able to do this without converting, I just can't
  // find the right thing to call.
  nsAutoString atomStr;
  aAtom->ToString(atomStr);
  NS_ConvertUTF8toUTF16 inStr(nsDependentCSubstring(aString, aLength));
  return nsContentUtils::EqualsIgnoreASCIICase(atomStr, inStr);
}

void
Gecko_FontFamilyList_Clear(FontFamilyList* aList) {
  aList->Clear();
}

void
Gecko_FontFamilyList_AppendNamed(FontFamilyList* aList, nsIAtom* aName)
{
  // Servo doesn't record whether the name was quoted or unquoted, so just
  // assume unquoted for now.
  FontFamilyName family;
  aName->ToString(family.mName);
  aList->Append(family);
}

void
Gecko_FontFamilyList_AppendGeneric(FontFamilyList* aList, FontFamilyType aType)
{
  aList->Append(FontFamilyName(aType));
}

void
Gecko_CopyFontFamilyFrom(nsFont* dst, const nsFont* src)
{
  dst->fontlist = src->fontlist;
}

void
Gecko_SetListStyleType(nsStyleList* style_struct, uint32_t type)
{
  // Builtin counter styles are static and use no-op refcounting, and thus are
  // safe to use off-main-thread.
  style_struct->SetCounterStyle(CounterStyleManager::GetBuiltinStyle(type));
}

void
Gecko_CopyListStyleTypeFrom(nsStyleList* dst, const nsStyleList* src)
{
  dst->SetCounterStyle(src->GetCounterStyle());
}

NS_IMPL_HOLDER_FFI_REFCOUNTING(nsIPrincipal, Principal)
NS_IMPL_HOLDER_FFI_REFCOUNTING(nsIURI, URI)

void
Gecko_SetMozBinding(nsStyleDisplay* aDisplay,
                    const uint8_t* aURLString, uint32_t aURLStringLength,
                    ThreadSafeURIHolder* aBaseURI,
                    ThreadSafeURIHolder* aReferrer,
                    ThreadSafePrincipalHolder* aPrincipal)
{
  MOZ_ASSERT(aDisplay);
  MOZ_ASSERT(aURLString);
  MOZ_ASSERT(aBaseURI);
  MOZ_ASSERT(aReferrer);
  MOZ_ASSERT(aPrincipal);

  nsString url;
  nsDependentCSubstring urlString(reinterpret_cast<const char*>(aURLString),
                                  aURLStringLength);
  AppendUTF8toUTF16(urlString, url);
  RefPtr<nsStringBuffer> urlBuffer = nsCSSValue::BufferFromString(url);

  aDisplay->mBinding =
    new css::URLValue(urlBuffer, do_AddRef(aBaseURI),
                      do_AddRef(aReferrer), do_AddRef(aPrincipal));
}

void
Gecko_CopyMozBindingFrom(nsStyleDisplay* aDest, const nsStyleDisplay* aSrc)
{
  aDest->mBinding = aSrc->mBinding;
}


void
Gecko_SetNullImageValue(nsStyleImage* aImage)
{
  MOZ_ASSERT(aImage);
  aImage->SetNull();
}

void
Gecko_SetGradientImageValue(nsStyleImage* aImage, nsStyleGradient* aGradient)
{
  MOZ_ASSERT(aImage);
  aImage->SetGradientData(aGradient);
}

void
Gecko_CopyImageValueFrom(nsStyleImage* aImage, const nsStyleImage* aOther)
{
  MOZ_ASSERT(aImage);
  MOZ_ASSERT(aOther);

  *aImage = *aOther;
}

nsStyleGradient*
Gecko_CreateGradient(uint8_t aShape,
                     uint8_t aSize,
                     bool aRepeating,
                     bool aLegacySyntax,
                     uint32_t aStopCount)
{
  nsStyleGradient* result = new nsStyleGradient();

  result->mShape = aShape;
  result->mSize = aSize;
  result->mRepeating = aRepeating;
  result->mLegacySyntax = aLegacySyntax;

  result->mAngle.SetNoneValue();
  result->mBgPosX.SetNoneValue();
  result->mBgPosY.SetNoneValue();
  result->mRadiusX.SetNoneValue();
  result->mRadiusY.SetNoneValue();

  nsStyleGradientStop dummyStop;
  dummyStop.mLocation.SetNoneValue();
  dummyStop.mColor = NS_RGB(0, 0, 0);
  dummyStop.mIsInterpolationHint = 0;

  for (uint32_t i = 0; i < aStopCount; i++) {
    result->mStops.AppendElement(dummyStop);
  }

  return result;
}

void
Gecko_SetGradientStop(nsStyleGradient* aGradient,
                      uint32_t aIndex,
                      const nsStyleCoord* aLocation,
                      nscolor aColor,
                      bool aIsInterpolationHint)
{
  MOZ_ASSERT(aGradient);
  MOZ_ASSERT(aLocation);
  MOZ_ASSERT(aIndex < aGradient->mStops.Length());

  aGradient->mStops[aIndex].mColor = aColor;
  aGradient->mStops[aIndex].mLocation = *aLocation;
  aGradient->mStops[aIndex].mIsInterpolationHint = aIsInterpolationHint;
}

#define STYLE_STRUCT(name, checkdata_cb)                                      \
                                                                              \
void                                                                          \
Gecko_Construct_nsStyle##name(nsStyle##name* ptr)                             \
{                                                                             \
  new (ptr) nsStyle##name(StyleStructContext::ServoContext());                \
}                                                                             \
                                                                              \
void                                                                          \
Gecko_CopyConstruct_nsStyle##name(nsStyle##name* ptr,                         \
                                  const nsStyle##name* other)                 \
{                                                                             \
  new (ptr) nsStyle##name(*other);                                            \
}                                                                             \
                                                                              \
void                                                                          \
Gecko_Destroy_nsStyle##name(nsStyle##name* ptr)                               \
{                                                                             \
  ptr->~nsStyle##name();                                                      \
}

#include "nsStyleStructList.h"

#undef STYLE_STRUCT

#ifndef MOZ_STYLO
void
Servo_DropNodeData(ServoNodeData* data)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_DropNodeData in a "
            "non-MOZ_STYLO build");
}

RawServoStyleSheet*
Servo_StylesheetFromUTF8Bytes(const uint8_t* bytes, uint32_t length,
                              mozilla::css::SheetParsingMode mode,
                              ThreadSafeURIHolder* base,
                              ThreadSafeURIHolder* referrer,
                              ThreadSafePrincipalHolder* principal)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_StylesheetFromUTF8Bytes in a "
            "non-MOZ_STYLO build");
}

void
Servo_AddRefStyleSheet(RawServoStyleSheet* sheet)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_AddRefStylesheet in a "
            "non-MOZ_STYLO build");
}

void
Servo_ReleaseStyleSheet(RawServoStyleSheet* sheet)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_ReleaseStylesheet in a "
            "non-MOZ_STYLO build");
}

void
Servo_AppendStyleSheet(RawServoStyleSheet* sheet, RawServoStyleSet* set)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_AppendStyleSheet in a "
            "non-MOZ_STYLO build");
}

void Servo_PrependStyleSheet(RawServoStyleSheet* sheet, RawServoStyleSet* set)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_PrependStyleSheet in a "
            "non-MOZ_STYLO build");
}

void Servo_RemoveStyleSheet(RawServoStyleSheet* sheet, RawServoStyleSet* set)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_RemoveStyleSheet in a "
            "non-MOZ_STYLO build");
}

void
Servo_InsertStyleSheetBefore(RawServoStyleSheet* sheet,
                             RawServoStyleSheet* reference,
                             RawServoStyleSet* set)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_InsertStyleSheetBefore in a "
            "non-MOZ_STYLO build");
}

bool
Servo_StyleSheetHasRules(RawServoStyleSheet* sheet)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_StyleSheetHasRules in a "
            "non-MOZ_STYLO build");
}

RawServoStyleSet*
Servo_InitStyleSet()
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_InitStyleSet in a "
            "non-MOZ_STYLO build");
}

void
Servo_DropStyleSet(RawServoStyleSet* set)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_DropStyleSet in a "
            "non-MOZ_STYLO build");
}

ServoComputedValues*
Servo_GetComputedValues(RawGeckoNode* node)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_GetComputedValues in a "
            "non-MOZ_STYLO build");
}

ServoComputedValues*
Servo_GetComputedValuesForAnonymousBox(ServoComputedValues* parentStyleOrNull,
                                       nsIAtom* pseudoTag,
                                       RawServoStyleSet* set)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_GetComputedValuesForAnonymousBox in a "
            "non-MOZ_STYLO build");
}

ServoComputedValues*
Servo_GetComputedValuesForPseudoElement(ServoComputedValues* parent_style,
                                        RawGeckoElement* match_element,
                                        nsIAtom* pseudo_tag,
                                        RawServoStyleSet* set,
                                        bool is_probe)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_GetComputedValuesForPseudoElement in a "
            "non-MOZ_STYLO build");
}

ServoComputedValues*
Servo_InheritComputedValues(ServoComputedValues* parent_style)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_InheritComputedValues in a "
            "non-MOZ_STYLO build");
}

void
Servo_AddRefComputedValues(ServoComputedValues*)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_AddRefComputedValues in a "
            "non-MOZ_STYLO build");
}

void
Servo_ReleaseComputedValues(ServoComputedValues*)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_ReleaseComputedValues in a "
            "non-MOZ_STYLO build");
}

void
Servo_Initialize()
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_Initialize in a "
            "non-MOZ_STYLO build");
}

void
Servo_RestyleDocument(RawGeckoDocument* doc, RawServoStyleSet* set)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_RestyleDocument in a "
            "non-MOZ_STYLO build");
}

void Servo_RestyleSubtree(RawGeckoNode* node, RawServoStyleSet* set)
{
  MOZ_CRASH("stylo: shouldn't be calling Servo_RestyleSubtree in a "
            "non-MOZ_STYLO build");
}

#define STYLE_STRUCT(name_, checkdata_cb_)                                     \
const nsStyle##name_*                                                          \
Servo_GetStyle##name_(ServoComputedValues*)                                    \
{                                                                              \
  MOZ_CRASH("stylo: shouldn't be calling Servo_GetStyle" #name_ " in a "       \
            "non-MOZ_STYLO build");                                            \
}
#include "nsStyleStructList.h"
#undef STYLE_STRUCT
#endif

#ifdef MOZ_STYLO
const nsStyleVariables*
Servo_GetStyleVariables(ServoComputedValues* aComputedValues)
{
  // Servo can't provide us with Variables structs yet, so instead of linking
  // to a Servo_GetStyleVariables defined in Servo we define one here that
  // always returns the same, empty struct.
  static nsStyleVariables variables(StyleStructContext::ServoContext());
  return &variables;
}
#endif
