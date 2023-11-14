/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set expandtab shiftwidth=2 tabstop=2: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CacheConstants_h_
#define _CacheConstants_h_

#include "nsGkAtoms.h"
#include "mozilla/a11y/RelationType.h"

namespace mozilla {
namespace a11y {

class CacheDomain {
 public:
  static constexpr uint64_t NameAndDescription = ((uint64_t)0x1) << 0;
  static constexpr uint64_t Value = ((uint64_t)0x1) << 1;
  static constexpr uint64_t Bounds = ((uint64_t)0x1) << 2;
  static constexpr uint64_t Resolution = ((uint64_t)0x1) << 3;
  static constexpr uint64_t Text = ((uint64_t)0x1) << 4;
  static constexpr uint64_t DOMNodeIDAndClass = ((uint64_t)0x1) << 5;
  static constexpr uint64_t State = ((uint64_t)0x1) << 6;
  static constexpr uint64_t GroupInfo = ((uint64_t)0x1) << 7;
  static constexpr uint64_t Actions = ((uint64_t)0x1) << 8;
  static constexpr uint64_t Style = ((uint64_t)0x1) << 9;
  static constexpr uint64_t TransformMatrix = ((uint64_t)0x1) << 10;
  static constexpr uint64_t ScrollPosition = ((uint64_t)0x1) << 11;
  static constexpr uint64_t Table = ((uint64_t)0x1) << 12;
  static constexpr uint64_t Spelling = ((uint64_t)0x1) << 13;
  static constexpr uint64_t Viewport = ((uint64_t)0x1) << 14;
  static constexpr uint64_t ARIA = ((uint64_t)0x1) << 15;
  static constexpr uint64_t Relations = ((uint64_t)0x1) << 16;
#ifdef XP_WIN
  // Used for MathML.
  static constexpr uint64_t InnerHTML = ((uint64_t)0x1) << 17;
#endif
  static constexpr uint64_t All = ~((uint64_t)0x0);
};

enum class CacheUpdateType {
  /*
   * An initial cache push of a loaded document or inserted subtree.
   */
  Initial,

  /*
   * An incremental cache push of one or more fields that have changed.
   */
  Update,
};

struct RelationData {
  nsStaticAtom* const mAtom;
  nsStaticAtom* const mValidTag;
  RelationType mType;
  RelationType mReverseType;
};

/**
 * This array of RelationData lists our relation types (explicit and reverse)
 * and the cache attribute atoms that store their targets. Attributes may
 * describe different kinds of relations, depending on the element they
 * originate on. For example, an <output> element's `for` attribute describes a
 * CONTROLLER_FOR relation, while the `for` attribute of a <label> describes a
 * LABEL_FOR relation. To ensure we process these attributes appropriately,
 * RelationData.mValidTag contains the atom for the tag this attribute/relation
 * type pairing is valid on. If the pairing is valid for all tag types, this
 * field is null.
 */
static constexpr RelationData kRelationTypeAtoms[] = {
    {nsGkAtoms::aria_labelledby, nullptr, RelationType::LABELLED_BY,
     RelationType::LABEL_FOR},
    {nsGkAtoms::_for, nsGkAtoms::label, RelationType::LABEL_FOR,
     RelationType::LABELLED_BY},
    {nsGkAtoms::aria_controls, nullptr, RelationType::CONTROLLER_FOR,
     RelationType::CONTROLLED_BY},
    {nsGkAtoms::_for, nsGkAtoms::output, RelationType::CONTROLLED_BY,
     RelationType::CONTROLLER_FOR},
    {nsGkAtoms::aria_describedby, nullptr, RelationType::DESCRIBED_BY,
     RelationType::DESCRIPTION_FOR},
    {nsGkAtoms::aria_flowto, nullptr, RelationType::FLOWS_TO,
     RelationType::FLOWS_FROM},
    {nsGkAtoms::aria_details, nullptr, RelationType::DETAILS,
     RelationType::DETAILS_FOR},
    {nsGkAtoms::aria_errormessage, nullptr, RelationType::ERRORMSG,
     RelationType::ERRORMSG_FOR},
};

// The count of numbers needed to serialize an nsRect. This is used when
// flattening character rects into an array of ints.
constexpr int32_t kNumbersInRect = 4;

/**
 * RemoteAccessible cache keys.
 * Cache keys are nsAtoms, but this is mostly an implementation detail. Rather
 * than creating new atoms specific to the RemoteAccessible cache, we often
 * reuse existing atoms which are a reasonably close match for the value we're
 * caching, though the choices aren't always clear or intuitive. For clarity, we
 * alias the cache keys to atoms below. Code dealing with the RemoteAccessible
 * cache should generally use these aliases rather than using nsAtoms directly.
 * There are two exceptions:
 * 1. Some ARIA attributes are copied directly from the DOM node, so these
 * aren't aliased. Specifically,   aria-level, aria-posinset and aria-setsize
 * are copied as separate cache keys as part of CacheDomain::GroupInfo.
 * 2. Keys for relations are defined in kRelationTypeAtoms above.
 */
class CacheKey {
 public:
  // uint64_t, CacheDomain::Actions
  // As returned by Accessible::AccessKey.
  static constexpr nsStaticAtom* AccessKey = nsGkAtoms::accesskey;
  // int32_t, no domain
  static constexpr nsStaticAtom* AppUnitsPerDevPixel =
      nsGkAtoms::_moz_device_pixel_ratio;
  // AccAttributes, CacheDomain::ARIA
  // ARIA attributes that are exposed as object attributes; i.e. returned in
  // Accessible::Attributes.
  static constexpr nsStaticAtom* ARIAAttributes = nsGkAtoms::aria;
  // nsString, CacheUpdateType::Initial
  // The ARIA role attribute if the role is unknown or if there are multiple
  // roles.
  static constexpr nsStaticAtom* ARIARole = nsGkAtoms::role;
  // bool, CacheDomain::State
  // The aria-selected attribute.
  static constexpr nsStaticAtom* ARIASelected = nsGkAtoms::aria_selected;
  // nsTArray<uint64_t>, CacheDomain::Table
  // The explicit headers of an HTML table cell.
  static constexpr nsStaticAtom* CellHeaders = nsGkAtoms::headers;
  // int32_t, CacheDomain::Table
  // The colspan of an HTML table cell.
  static constexpr nsStaticAtom* ColSpan = nsGkAtoms::colspan;
  // nsTArray<int32_t, 2>, CacheDomain::Bounds
  // The offset from an OuterDocAccessible (iframe) to its embedded document.
  static constexpr nsStaticAtom* CrossDocOffset = nsGkAtoms::crossorigin;
  // nsAtom, CacheDomain::Style
  // CSS display; block, inline, etc.
  static constexpr nsStaticAtom* CSSDisplay = nsGkAtoms::display;
  // nsAtom, CacheDomain::Style
  // CSS overflow; e.g. hidden.
  static constexpr nsStaticAtom* CSSOverflow = nsGkAtoms::overflow;
  // nsAtom, CacheDomain::Style
  // CSS position; e.g. fixed.
  static constexpr nsStaticAtom* CssPosition = nsGkAtoms::position;
  // nsString, CacheDomain::NameAndDescription
  static constexpr nsStaticAtom* Description = nsGkAtoms::description;
  // nsString, CacheDomain::Relations
  // The "name" DOM attribute.
  static constexpr nsStaticAtom* DOMName = nsGkAtoms::attributeName;
  // nsAtom, CacheDomain::DOMNodeIDAndClass
  // The "class" DOM attribute.
  static constexpr nsStaticAtom* DOMNodeClass = nsGkAtoms::_class;
  // nsAtom, CacheDomain::DOMNodeIDAndClass
  static constexpr nsStaticAtom* DOMNodeID = nsGkAtoms::id;
  // AccGroupInfo, no domain
  static constexpr nsStaticAtom* GroupInfo = nsGkAtoms::group;
  // nsTArray<int32_t>, no domain
  // As returned by HyperTextAccessibleBase::CachedHyperTextOffsets.
  static constexpr nsStaticAtom* HyperTextOffsets = nsGkAtoms::offset;
  // bool, CacheDomain::Actions
  // Whether this image has a longdesc.
  static constexpr nsStaticAtom* HasLongdesc = nsGkAtoms::longdesc;
  // nsString, CacheDomain::NameAndDescription
  static constexpr nsStaticAtom* HTMLPlaceholder = nsGkAtoms::placeholder;
#ifdef XP_WIN
  // nsString, CacheDomain::InnerHTML
  static constexpr nsStaticAtom* InnerHTML = nsGkAtoms::html;
#endif
  // nsAtom, CacheUpdateType::Initial
  // The type of an <input> element; tel, email, etc.
  static constexpr nsStaticAtom* InputType = nsGkAtoms::textInputType;
  // bool, CacheDomain::Bounds
  // Whether the Accessible is fully clipped.
  static constexpr nsStaticAtom* IsClipped = nsGkAtoms::clip_rule;
  // nsString, CacheUpdateType::Initial
  static constexpr nsStaticAtom* MimeType = nsGkAtoms::headerContentType;
  // double, CacheDomain::Value
  static constexpr nsStaticAtom* MaxValue = nsGkAtoms::max;
  // double, CacheDomain::Value
  static constexpr nsStaticAtom* MinValue = nsGkAtoms::min;
  // nsString, CacheDomain::NameAndDescription
  static constexpr nsStaticAtom* Name = nsGkAtoms::name;
  // ENameValueFlag, CacheDomain::NameAndDescription
  // Returned by Accessible::Name.
  static constexpr nsStaticAtom* NameValueFlag = nsGkAtoms::explicit_name;
  // double, CacheDomain::Value
  // The numeric value returned by Accessible::CurValue.
  static constexpr nsStaticAtom* NumericValue = nsGkAtoms::value;
  // float, CacheDomain::Style
  static constexpr nsStaticAtom* Opacity = nsGkAtoms::opacity;
  // nsTArray<int32_t, 4>, CacheDomain::Bounds
  // The screen bounds relative  to the parent Accessible
  // as returned by LocalAccessible::ParentRelativeBounds.
  static constexpr nsStaticAtom* ParentRelativeBounds =
      nsGkAtoms::relativeBounds;
  // nsAtom, CacheDomain::Actions
  static constexpr nsStaticAtom* PrimaryAction = nsGkAtoms::action;
  // float, no domain
  // Document resolution.
  static constexpr nsStaticAtom* Resolution = nsGkAtoms::resolution;
  // int32_t, CacheDomain::Table
  // The rowspan of an HTML table cell.
  static constexpr nsStaticAtom* RowSpan = nsGkAtoms::rowspan;
  // nsTArray<int32_t, 2>, CacheDomain::ScrollPosition
  static constexpr nsStaticAtom* ScrollPosition = nsGkAtoms::scrollPosition;
  // nsTArray<int32_t>, CacheDomain::Spelling | CacheDomain::Text
  // The offsets of spelling errors.
  static constexpr nsStaticAtom* SpellingErrors = nsGkAtoms::spelling;
  // nsString, CacheDomain::Value
  // The src URL of images.
  static constexpr nsStaticAtom* SrcURL = nsGkAtoms::src;
  // uint64_t, CacheDomain::State
  // As returned by Accessible::State.
  static constexpr nsStaticAtom* State = nsGkAtoms::state;
  // double, CacheDomain::Value
  // The value step returned by Accessible::Step.
  static constexpr nsStaticAtom* Step = nsGkAtoms::step;
  // nsAtom, CacheUpdateType::Initial
  // The tag name of the element.
  static constexpr nsStaticAtom* TagName = nsGkAtoms::tag;
  // bool, CacheDomain::Table
  // Whether this is a layout table.
  static constexpr nsStaticAtom* TableLayoutGuess = nsGkAtoms::layout_guess;
  // nsString, CacheDomain::Text
  // The text of TextLeafAccessibles.
  static constexpr nsStaticAtom* Text = nsGkAtoms::text;
  // AccAttributes, CacheDomain::Text
  // Text attributes; font, etc.
  static constexpr nsStaticAtom* TextAttributes = nsGkAtoms::style;
  // nsTArray<int32_t, 4 * n>, CacheDomain::Text | CacheDomain::Bounds
  // The bounds of each character in a text leaf.
  static constexpr nsStaticAtom* TextBounds = nsGkAtoms::characterData;
  // nsTArray<int32_t>, CacheDomain::Text | CacheDomain::Bounds
  // The text offsets where new lines start.
  static constexpr nsStaticAtom* TextLineStarts = nsGkAtoms::line;
  // nsString, CacheDomain::Value
  // The textual value returned by Accessible::Value (as opposed to
  // the numeric value returned by Accessible::CurValue).
  static constexpr nsStaticAtom* TextValue = nsGkAtoms::aria_valuetext;
  // gfx::Matrix4x4, CacheDomain::TransformMatrix
  static constexpr nsStaticAtom* TransformMatrix = nsGkAtoms::transform;
  // nsTArray<uint64_t>, CacheDomain::Viewport
  // The list of Accessibles in the viewport used for hit testing and on-screen
  // determination.
  static constexpr nsStaticAtom* Viewport = nsGkAtoms::viewport;
};

}  // namespace a11y
}  // namespace mozilla

#endif
