/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alexander Surkov <surkov.alexander@gmail.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsTextAttrs_h_
#define nsTextAttrs_h_

#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIPersistentProperties2.h"
#include "nsStyleConsts.h"

class nsHyperTextAccessible;

namespace mozilla {
namespace a11y {

/**
 * Used to expose text attributes for the hyper text accessible (see
 * nsHyperTextAccessible class).
 *
 * @note "invalid: spelling" text attribute is implemented entirely in
 *       nsHyperTextAccessible class.
 */
class TextAttrsMgr
{
public:
  /**
   * Constructor. Used to expose default text attributes.
   */
  TextAttrsMgr(nsHyperTextAccessible* aHyperTextAcc) :
    mHyperTextAcc(aHyperTextAcc), mIncludeDefAttrs(true),
    mOffsetAcc(nsnull), mOffsetAccIdx(-1) { }

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
  TextAttrsMgr(nsHyperTextAccessible* aHyperTextAcc,
               bool aIncludeDefAttrs,
               nsAccessible* aOffsetAcc,
               PRInt32 aOffsetAccIdx) :
    mHyperTextAcc(aHyperTextAcc), mIncludeDefAttrs(aIncludeDefAttrs),
    mOffsetAcc(aOffsetAcc), mOffsetAccIdx(aOffsetAccIdx) { }

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
  void GetAttributes(nsIPersistentProperties* aAttributes,
                     PRInt32* aStartHTOffset = nsnull,
                     PRInt32* aEndHTOffset = nsnull);

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
  void GetRange(TextAttr* aAttrArray[], PRUint32 aAttrArrayLen,
                PRInt32* aStartHTOffset, PRInt32* aEndHTOffset);

private:
  nsAccessible* mOffsetAcc;
  nsHyperTextAccessible* mHyperTextAcc;
  PRInt32 mOffsetAccIdx;
  bool mIncludeDefAttrs;

protected:

  /**
   * Interface class of text attribute class implementations.
   */
  class TextAttr
  {
  public:
    /**
     * Expose the text attribute to the given attribute set.
     *
     * @param aAttributes           [in] the given attribute set
     * @param aIncludeDefAttrValue  [in] if true then attribute is exposed even
     *                               if its value is the same as default one
     */
    virtual void Expose(nsIPersistentProperties* aAttributes,
                        bool aIncludeDefAttrValue) = 0;

    /**
     * Return true if the text attribute value on the given element equals with
     * predefined attribute value.
     */
    virtual bool Equal(nsIContent* aElm) = 0;
  };


  /**
   * Base class to work with text attributes. See derived classes below.
   */
  template<class T>
  class TTextAttr : public TextAttr
  {
  public:
    TTextAttr(bool aGetRootValue) : mGetRootValue(aGetRootValue) {}

    // TextAttr
    virtual void Expose(nsIPersistentProperties* aAttributes,
                        bool aIncludeDefAttrValue)
    {
      if (mGetRootValue) {
        if (mIsRootDefined)
          ExposeValue(aAttributes, mRootNativeValue);
        return;
      }

      if (mIsDefined) {
        if (aIncludeDefAttrValue || mRootNativeValue != mNativeValue)
          ExposeValue(aAttributes, mNativeValue);
        return;
      }

      if (aIncludeDefAttrValue && mIsRootDefined)
        ExposeValue(aAttributes, mRootNativeValue);
    }

    virtual bool Equal(nsIContent* aElm)
    {
      T nativeValue;
      bool isDefined = GetValueFor(aElm, &nativeValue);

      if (!mIsDefined && !isDefined)
        return true;

      if (mIsDefined && isDefined)
        return nativeValue == mNativeValue;

      if (mIsDefined)
        return mNativeValue == mRootNativeValue;

      return nativeValue == mRootNativeValue;
    }

  protected:

    // Expose the text attribute with the given value to attribute set.
    virtual void ExposeValue(nsIPersistentProperties* aAttributes,
                             const T& aValue) = 0;

    // Return native value for the given DOM element.
    virtual bool GetValueFor(nsIContent* aElm, T* aValue) = 0;

    // Indicates if root value should be exposed.
    bool mGetRootValue;

    // Native value and flag indicating if the value is defined (initialized in
    // derived classes). Note, undefined native value means it is inherited
    // from root.
    T mNativeValue;
    bool mIsDefined;

    // Native root value and flag indicating if the value is defined  (initialized
    // in derived classes).
    T mRootNativeValue;
    bool mIsRootDefined;
  };


  /**
   * Class is used for the work with 'language' text attribute.
   */
  class LangTextAttr : public TTextAttr<nsString>
  {
  public:
    LangTextAttr(nsHyperTextAccessible* aRoot, nsIContent* aRootElm,
                 nsIContent* aElm);
    virtual ~LangTextAttr() { }

  protected:

    // TextAttr
    virtual bool GetValueFor(nsIContent* aElm, nsString* aValue);
    virtual void ExposeValue(nsIPersistentProperties* aAttributes,
                             const nsString& aValue);

  private:
    bool GetLang(nsIContent* aElm, nsAString& aLang);
    nsCOMPtr<nsIContent> mRootContent;
  };


  /**
   * Class is used for the work with 'background-color' text attribute.
   */
  class BGColorTextAttr : public TTextAttr<nscolor>
  {
  public:
    BGColorTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~BGColorTextAttr() { }

  protected:

    // TextAttr
    virtual bool GetValueFor(nsIContent* aElm, nscolor* aValue);
    virtual void ExposeValue(nsIPersistentProperties* aAttributes,
                             const nscolor& aValue);

  private:
    bool GetColor(nsIFrame* aFrame, nscolor* aColor);
    nsIFrame* mRootFrame;
  };


  /**
   * Class is used for the work with 'color' text attribute.
   */
  class ColorTextAttr : public TTextAttr<nscolor>
  {
  public:
    ColorTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~ColorTextAttr() { }

  protected:

    // TTextAttr
    virtual bool GetValueFor(nsIContent* aElm, nscolor* aValue);
    virtual void ExposeValue(nsIPersistentProperties* aAttributes,
                             const nscolor& aValue);
  };


  /**
   * Class is used for the work with "font-family" text attribute.
   */
  class FontFamilyTextAttr : public TTextAttr<nsString>
  {
  public:
    FontFamilyTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~FontFamilyTextAttr() { }

  protected:

    // TTextAttr
    virtual bool GetValueFor(nsIContent* aElm, nsString* aValue);
    virtual void ExposeValue(nsIPersistentProperties* aAttributes,
                             const nsString& aValue);

  private:

    bool GetFontFamily(nsIFrame* aFrame, nsString& aFamily);
  };


  /**
   * Class is used for the work with "font-size" text attribute.
   */
  class FontSizeTextAttr : public TTextAttr<nscoord>
  {
  public:
    FontSizeTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~FontSizeTextAttr() { }

  protected:

    // TTextAttr
    virtual bool GetValueFor(nsIContent* aElm, nscoord* aValue);
    virtual void ExposeValue(nsIPersistentProperties* aAttributes,
                             const nscoord& aValue);

  private:
    nsDeviceContext* mDC;
  };


  /**
   * Class is used for the work with "font-style" text attribute.
   */
  class FontStyleTextAttr : public TTextAttr<nscoord>
  {
  public:
    FontStyleTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~FontStyleTextAttr() { }

  protected:

    // TTextAttr
    virtual bool GetValueFor(nsIContent* aContent, nscoord* aValue);
    virtual void ExposeValue(nsIPersistentProperties* aAttributes,
                             const nscoord& aValue);
  };


  /**
   * Class is used for the work with "font-weight" text attribute.
   */
  class FontWeightTextAttr : public TTextAttr<PRInt32>
  {
  public:
    FontWeightTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~FontWeightTextAttr() { }

  protected:

    // TTextAttr
    virtual bool GetValueFor(nsIContent* aElm, PRInt32* aValue);
    virtual void ExposeValue(nsIPersistentProperties* aAttributes,
                             const PRInt32& aValue);

  private:
    PRInt32 GetFontWeight(nsIFrame* aFrame);
  };


  /**
   * TextDecorTextAttr class is used for the work with
   * "text-line-through-style", "text-line-through-color",
   * "text-underline-style" and "text-underline-color" text attributes.
   */

  class TextDecorValue
  {
  public:
    TextDecorValue() { }
    TextDecorValue(nsIFrame* aFrame);

    nscolor Color() const { return mColor; }
    PRUint8 Style() const { return mStyle; }

    bool IsDefined() const
      { return IsUnderline() || IsLineThrough(); }
    bool IsUnderline() const
      { return mLine & NS_STYLE_TEXT_DECORATION_LINE_UNDERLINE; }
    bool IsLineThrough() const
      { return mLine & NS_STYLE_TEXT_DECORATION_LINE_LINE_THROUGH; }

    bool operator ==(const TextDecorValue& aValue)
    {
      return mColor == aValue.mColor && mLine == aValue.mLine &&
        mStyle == aValue.mStyle;
    }
    bool operator !=(const TextDecorValue& aValue)
      { return !(*this == aValue); }

  private:
    nscolor mColor;
    PRUint8 mLine;
    PRUint8 mStyle;
  };

  class TextDecorTextAttr : public TTextAttr<TextDecorValue>
  {
  public:
    TextDecorTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~TextDecorTextAttr() { }

  protected:

    // TextAttr
    virtual bool GetValueFor(nsIContent* aElm, TextDecorValue* aValue);
    virtual void ExposeValue(nsIPersistentProperties* aAttributes,
                             const TextDecorValue& aValue);
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

  class TextPosTextAttr : public TTextAttr<TextPosValue>
  {
  public:
    TextPosTextAttr(nsIFrame* aRootFrame, nsIFrame* aFrame);
    virtual ~TextPosTextAttr() { }

  protected:

    // TextAttr
    virtual bool GetValueFor(nsIContent* aElm, TextPosValue* aValue);
    virtual void ExposeValue(nsIPersistentProperties* aAttributes,
                             const TextPosValue& aValue);

  private:
    TextPosValue GetTextPosValue(nsIFrame* aFrame) const;
  };

}; // TextAttrMgr

} // namespace a11y
} // namespace mozilla

#endif
