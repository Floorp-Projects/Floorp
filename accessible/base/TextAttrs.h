/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTextAttrs_h_
#define nsTextAttrs_h_

#include "mozilla/FontPropertyTypes.h"
#include "nsCOMPtr.h"
#include "nsColor.h"
#include "nsString.h"
#include "nsStyleConsts.h"

class nsIFrame;
class nsIContent;
class nsDeviceContext;

namespace mozilla {
namespace a11y {

class AccAttributes;
class LocalAccessible;
class HyperTextAccessible;

/**
 * Used to expose text attributes for the hyper text accessible (see
 * HyperTextAccessible class).
 *
 * @note "invalid: spelling" text attribute is implemented entirely in
 *       HyperTextAccessible class.
 */
class TextAttrsMgr {
 public:
  /**
   * Constructor. Used to expose default text attributes.
   */
  explicit TextAttrsMgr(HyperTextAccessible* aHyperTextAcc)
      : mOffsetAcc(nullptr),
        mHyperTextAcc(aHyperTextAcc),
        mOffsetAccIdx(-1),
        mIncludeDefAttrs(true) {}

  /**
   * Constructor. Used to expose text attributes at the given offset.
   *
   * @param aHyperTextAcc    [in] hyper text accessible text attributes are
   *                          calculated for
   * @param aIncludeDefAttrs [optional] indicates whether default text
   *                          attributes should be included into list of exposed
   *                          text attributes
   * @param oOffsetAcc       [optional] offset an accessible the text attributes
   *                          should be calculated for
   * @param oOffsetAccIdx    [optional] index in parent of offset accessible
   */
  TextAttrsMgr(HyperTextAccessible* aHyperTextAcc, bool aIncludeDefAttrs,
               LocalAccessible* aOffsetAcc, int32_t aOffsetAccIdx)
      : mOffsetAcc(aOffsetAcc),
        mHyperTextAcc(aHyperTextAcc),
        mOffsetAccIdx(aOffsetAccIdx),
        mIncludeDefAttrs(aIncludeDefAttrs) {}

  /*
   * Return text attributes and hyper text offsets where these attributes are
   * applied. Offsets are calculated in the case of non default attributes.
   *
   * @note In the case of default attributes pointers on hyper text offsets
   *       must be skipped.
   *
   * @param aAttributes    [in, out] text attributes list
   * @param aStartHTOffset [out, optional] start hyper text offset
   * @param aEndHTOffset   [out, optional] end hyper text offset
   */
  void GetAttributes(AccAttributes* aAttributes,
                     uint32_t* aStartHTOffset = nullptr,
                     uint32_t* aEndHTOffset = nullptr);

 protected:
  /**
   * Calculates range (start and end offsets) of text where the text attributes
   * are stretched. New offsets may be smaller if one of text attributes changes
   * its value before or after the given offsets.
   *
   * @param aTextAttrArray  [in] text attributes array
   * @param aAttrArrayLen   [in] text attributes array length
   * @param aStartHTOffset  [in, out] the start offset
   * @param aEndHTOffset    [in, out] the end offset
   */
  class TextAttr;
  void GetRange(TextAttr* aAttrArray[], uint32_t aAttrArrayLen,
                uint32_t* aStartOffset, uint32_t* aEndOffset);

 private:
  LocalAccessible* mOffsetAcc;
  HyperTextAccessible* mHyperTextAcc;
  int32_t mOffsetAccIdx;
  bool mIncludeDefAttrs;

 protected:
  /**
   * Interface class of text attribute class implementations.
   */
  class TextAttr {
   public:
    /**
     * Expose the text attribute to the given attribute set.
     *
     * @param aAttributes           [in] the given attribute set
     * @param aIncludeDefAttrValue  [in] if true then attribute is exposed even
     *                               if its value is the same as default one
     */
    virtual void Expose(AccAttributes* aAttributes,
                        bool aIncludeDefAttrValue) = 0;

    /**
     * Return true if the text attribute value on the given element equals with
     * predefined attribute value.
     */
    virtual bool Equal(LocalAccessible* aAccessible) = 0;
  };

  /**
   * Base class to work with text attributes. See derived classes below.
   */
  template <class T>
  class TTextAttr : public TextAttr {
   public:
    explicit TTextAttr(bool aGetRootValue) : mGetRootValue(aGetRootValue) {}

    // TextAttr
    virtual void Expose(AccAttributes* aAttributes,
                        bool aIncludeDefAttrValue) override {
      if (mGetRootValue) {
        if (mIsRootDefined) ExposeValue(aAttributes, mRootNativeValue);
        return;
      }

      if (mIsDefined) {
        if (aIncludeDefAttrValue || mRootNativeValue != mNativeValue) {
          ExposeValue(aAttributes, mNativeValue);
        }
        return;
      }

      if (aIncludeDefAttrValue && mIsRootDefined) {
        ExposeValue(aAttributes, mRootNativeValue);
      }
    }

    virtual bool Equal(LocalAccessible* aAccessible) override {
      T nativeValue;
      bool isDefined = GetValueFor(aAccessible, &nativeValue);

      if (!mIsDefined && !isDefined) return true;

      if (mIsDefined && isDefined) return nativeValue == mNativeValue;

      if (mIsDefined) return mNativeValue == mRootNativeValue;

      return nativeValue == mRootNativeValue;
    }

   protected:
    // Expose the text attribute with the given value to attribute set.
    virtual void ExposeValue(AccAttributes* aAttributes, const T& aValue) = 0;

    // Return native value for the given DOM element.
    virtual bool GetValueFor(LocalAccessible* aAccessible, T* aValue) = 0;

    // Indicates if root value should be exposed.
    bool mGetRootValue;

    // Native value and flag indicating if the value is defined (initialized in
    // derived classes). Note, undefined native value means it is inherited
    // from root.
    MOZ_INIT_OUTSIDE_CTOR T mNativeValue;
    MOZ_INIT_OUTSIDE_CTOR bool mIsDefined;

    // Native root value and flag indicating if the value is defined
    // (initialized in derived classes).
    MOZ_INIT_OUTSIDE_CTOR T mRootNativeValue;
    MOZ_INIT_OUTSIDE_CTOR bool mIsRootDefined;
  };

  /**
   * Class is used for the work with 'language' text attribute.
   */
  class LangTextAttr : public TTextAttr<nsString> {
   public:
    LangTextAttr(HyperTextAccessible* aRoot, nsIContent* aRootElm,
                 nsIContent* aElm);
    virtual ~LangTextAttr();

   protected:
    // TextAttr
    virtual bool GetValueFor(LocalAccessible* aAccessible,
                             nsString* aValue) override;
    virtual void ExposeValue(AccAttributes* aAttributes,
                             const nsString& aValue) override;

   private:
    nsCOMPtr<nsIContent> mRootContent;
  };

  /**
   * Class is used for the 'invalid' text attribute. Note, it calculated
   * the attribute from aria-invalid attribute only; invalid:spelling attribute
   * calculated from misspelled text in the editor is managed by
   * HyperTextAccessible and applied on top of the value from aria-invalid.
   */
  class InvalidTextAttr : public TTextAttr<uint32_t> {
   public:
    InvalidTextAttr(nsIContent* aRootElm, nsIContent* aElm);
    virtual ~InvalidTextAttr(){};

   protected:
    enum { eFalse, eGrammar, eSpelling, eTrue };

    // TextAttr
    virtual bool GetValueFor(LocalAccessible* aAccessible,
                             uint32_t* aValue) override;
    virtual void ExposeValue(AccAttributes* aAttributes,
                             const uint32_t& aValue) override;

   private:
    bool GetValue(nsIContent* aElm, uint32_t* aValue);
    nsIContent* mRootElm;
  };

  /**
   * Class is used for the work with 'background-color' text attribute.
   */
  class BGColorTextAttr : public TTextAttr<nscolor> {
   public:
    BGColorTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~BGColorTextAttr() {}

   protected:
    // TextAttr
    virtual bool GetValueFor(LocalAccessible* aAccessible,
                             nscolor* aValue) override;
    virtual void ExposeValue(AccAttributes* aAttributes,
                             const nscolor& aValue) override;

   private:
    bool GetColor(nsIFrame* aFrame, nscolor* aColor);
    nsIFrame* mRootFrame;
  };

  /**
   * Class is used for the work with 'color' text attribute.
   */
  class ColorTextAttr : public TTextAttr<nscolor> {
   public:
    ColorTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~ColorTextAttr() {}

   protected:
    // TTextAttr
    virtual bool GetValueFor(LocalAccessible* aAccessible,
                             nscolor* aValue) override;
    virtual void ExposeValue(AccAttributes* aAttributes,
                             const nscolor& aValue) override;
  };

  /**
   * Class is used for the work with "font-family" text attribute.
   */
  class FontFamilyTextAttr : public TTextAttr<nsString> {
   public:
    FontFamilyTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~FontFamilyTextAttr() {}

   protected:
    // TTextAttr
    virtual bool GetValueFor(LocalAccessible* aAccessible,
                             nsString* aValue) override;
    virtual void ExposeValue(AccAttributes* aAttributes,
                             const nsString& aValue) override;

   private:
    bool GetFontFamily(nsIFrame* aFrame, nsString& aFamily);
  };

  /**
   * Class is used for the work with "font-size" text attribute.
   */
  class FontSizeTextAttr : public TTextAttr<nscoord> {
   public:
    FontSizeTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~FontSizeTextAttr() {}

   protected:
    // TTextAttr
    virtual bool GetValueFor(LocalAccessible* aAccessible,
                             nscoord* aValue) override;
    virtual void ExposeValue(AccAttributes* aAttributes,
                             const nscoord& aValue) override;

   private:
    nsDeviceContext* mDC;
  };

  /**
   * Class is used for the work with "font-style" text attribute.
   */
  class FontStyleTextAttr : public TTextAttr<mozilla::FontSlantStyle> {
   public:
    FontStyleTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~FontStyleTextAttr() {}

   protected:
    // TTextAttr
    virtual bool GetValueFor(LocalAccessible* aContent,
                             mozilla::FontSlantStyle* aValue) override;
    virtual void ExposeValue(AccAttributes* aAttributes,
                             const mozilla::FontSlantStyle& aValue) override;
  };

  /**
   * Class is used for the work with "font-weight" text attribute.
   */
  class FontWeightTextAttr : public TTextAttr<mozilla::FontWeight> {
   public:
    FontWeightTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~FontWeightTextAttr() {}

   protected:
    // TTextAttr
    virtual bool GetValueFor(LocalAccessible* aAccessible,
                             mozilla::FontWeight* aValue) override;
    virtual void ExposeValue(AccAttributes* aAttributes,
                             const mozilla::FontWeight& aValue) override;

   private:
    mozilla::FontWeight GetFontWeight(nsIFrame* aFrame);
  };

  /**
   * Class is used for the work with 'auto-generated' text attribute.
   */
  class AutoGeneratedTextAttr : public TTextAttr<bool> {
   public:
    AutoGeneratedTextAttr(HyperTextAccessible* aHyperTextAcc,
                          LocalAccessible* aAccessible);
    virtual ~AutoGeneratedTextAttr() {}

   protected:
    // TextAttr
    virtual bool GetValueFor(LocalAccessible* aAccessible,
                             bool* aValue) override;
    virtual void ExposeValue(AccAttributes* aAttributes,
                             const bool& aValue) override;
  };

  /**
   * TextDecorTextAttr class is used for the work with
   * "text-line-through-style", "text-line-through-color",
   * "text-underline-style" and "text-underline-color" text attributes.
   */

  class TextDecorValue {
   public:
    TextDecorValue()
        : mColor{0},
          mLine{StyleTextDecorationLine::NONE},
          mStyle{NS_STYLE_TEXT_DECORATION_STYLE_NONE} {}
    explicit TextDecorValue(nsIFrame* aFrame);

    nscolor Color() const { return mColor; }
    uint8_t Style() const { return mStyle; }

    bool IsDefined() const { return IsUnderline() || IsLineThrough(); }
    bool IsUnderline() const {
      return bool(mLine & mozilla::StyleTextDecorationLine::UNDERLINE);
    }
    bool IsLineThrough() const {
      return bool(mLine & mozilla::StyleTextDecorationLine::LINE_THROUGH);
    }

    bool operator==(const TextDecorValue& aValue) {
      return mColor == aValue.mColor && mLine == aValue.mLine &&
             mStyle == aValue.mStyle;
    }
    bool operator!=(const TextDecorValue& aValue) { return !(*this == aValue); }

   private:
    nscolor mColor;
    mozilla::StyleTextDecorationLine mLine;
    uint8_t mStyle;
  };

  class TextDecorTextAttr : public TTextAttr<TextDecorValue> {
   public:
    TextDecorTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~TextDecorTextAttr() {}

   protected:
    // TextAttr
    virtual bool GetValueFor(LocalAccessible* aAccessible,
                             TextDecorValue* aValue) override;
    virtual void ExposeValue(AccAttributes* aAttributes,
                             const TextDecorValue& aValue) override;
  };

  /**
   * Class is used for the work with "text-position" text attribute.
   */

  enum TextPosValue {
    eTextPosNone = 0,
    eTextPosBaseline,
    eTextPosSub,
    eTextPosSuper
  };

  class TextPosTextAttr : public TTextAttr<TextPosValue> {
   public:
    TextPosTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~TextPosTextAttr() {}

   protected:
    // TextAttr
    virtual bool GetValueFor(LocalAccessible* aAccessible,
                             TextPosValue* aValue) override;
    virtual void ExposeValue(AccAttributes* aAttributes,
                             const TextPosValue& aValue) override;

   private:
    TextPosValue GetTextPosValue(nsIFrame* aFrame) const;
  };

};  // TextAttrMgr

}  // namespace a11y
}  // namespace mozilla

#endif
