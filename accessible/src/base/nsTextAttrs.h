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

class nsHyperTextAccessible;


#include "nsIDOMNode.h"
#include "nsIDOMElement.h"

#include "nsIContent.h"
#include "nsIFrame.h"
#include "nsIPersistentProperties2.h"

#include "nsCOMPtr.h"
#include "nsString.h"

class nsITextAttr;

/**
 * Used to expose text attributes for the hyper text accessible (see
 * nsHyperTextAccessible class). It is indended for the work with 'language' and
 * CSS based text attributes.
 *
 * @note "invalid: spelling" text attrbiute is implemented entirerly in
 *       nsHyperTextAccessible class.
 */
class nsTextAttrsMgr
{
public:
  /**
   * Constructor. If instance of the class is intended to expose default text
   * attributes then 'aIncludeDefAttrs' and 'aOffsetNode' argument must be
   * skiped.
   *
   * @param aHyperTextAcc    hyper text accessible text attributes are
   *                         calculated for
   * @param aHyperTextNode   DOM node of the given hyper text accessbile
   * @param aIncludeDefAttrs [optional] indicates whether default text
   *                         attributes should be included into list of exposed
   *                         text attributes.
   * @param oOffsetNode      [optional] DOM node represents hyper text offset
   *                         inside hyper text accessible
   */
  nsTextAttrsMgr(nsHyperTextAccessible *aHyperTextAcc,
                 bool aIncludeDefAttrs = true,
                 nsAccessible *aOffsetAcc = nsnull,
                 PRInt32 aOffsetAccIdx = -1);

  /*
   * Return text attributes and hyper text offsets where these attributes are
   * applied. Offsets are calculated in the case of non default attributes.
   *
   * @note In the case of default attributes pointers on hyper text offsets
   *       must be skiped.
   *
   * @param aAttributes    [in, out] text attributes list
   * @param aStartHTOffset [out, optional] start hyper text offset
   * @param aEndHTOffset   [out, optional] end hyper text offset
   */
  nsresult GetAttributes(nsIPersistentProperties *aAttributes,
                         PRInt32 *aStartHTOffset = nsnull,
                         PRInt32 *aEndHTOffset = nsnull);

protected:

  /**
   * Calculates range (start and end offsets) of text where the text attributes
   * are stretched. New offsets may be smaller if one of text attributes changes
   * its value before or after the given offsets.
   *
   * @param aTextAttrArray  [in] text attributes array
   * @param aStartHTOffset  [in, out] the start offset
   * @param aEndHTOffset    [in, out] the end offset
   */
   nsresult GetRange(const nsTArray<nsITextAttr*>& aTextAttrArray,
                     PRInt32 *aStartHTOffset, PRInt32 *aEndHTOffset);

private:
  nsRefPtr<nsHyperTextAccessible> mHyperTextAcc;

  bool mIncludeDefAttrs;

  nsRefPtr<nsAccessible> mOffsetAcc;
  PRInt32 mOffsetAccIdx;
};


////////////////////////////////////////////////////////////////////////////////
// Private implementation details

/**
 * Interface class of text attribute class implementations.
 */
class nsITextAttr
{
public:
  /**
   * Return the name of text attribute.
   */
  virtual nsIAtom* GetName() const = 0;

  /**
   * Retrieve the value of text attribute in out param, return true if differs
   * from the default value of text attribute or if include default attribute
   * value flag is setted.
   * 
   * @param aValue                [in, out] the value of text attribute
   * @param aIncludeDefAttrValue  [in] include default attribute value flag
   * @return                      true if text attribute value differs from
   *                              default or include default attribute value
   *                              flag is applied
   */
  virtual bool GetValue(nsAString& aValue, bool aIncludeDefAttrValue) = 0;

  /**
   * Return true if the text attribute value on the given element equals with
   * predefined attribute value.
   */
  virtual bool Equal(nsIContent *aContent) = 0;
};


/**
 * Base class to work with text attributes. See derived classes below.
 */
template<class T>
class nsTextAttr : public nsITextAttr
{
public:
  nsTextAttr(bool aGetRootValue) : mGetRootValue(aGetRootValue) {}

  // nsITextAttr
  virtual bool GetValue(nsAString& aValue, bool aIncludeDefAttrValue)
  {
    if (mGetRootValue) {
      Format(mRootNativeValue, aValue);
      return mIsRootDefined;
    }

    bool isDefined = mIsDefined;
    T* nativeValue = &mNativeValue;

    if (!isDefined) {
      if (aIncludeDefAttrValue) {
        isDefined = mIsRootDefined;
        nativeValue = &mRootNativeValue;
      }
    } else if (!aIncludeDefAttrValue) {
      isDefined = mRootNativeValue != mNativeValue;
    }

    if (!isDefined)
      return PR_FALSE;

    Format(*nativeValue, aValue);
    return PR_TRUE;
  }

  virtual bool Equal(nsIContent *aContent)
  {
    T nativeValue;
    bool isDefined = GetValueFor(aContent, &nativeValue);

    if (!mIsDefined && !isDefined)
      return PR_TRUE;

    if (mIsDefined && isDefined)
      return nativeValue == mNativeValue;

    if (mIsDefined)
      return mNativeValue == mRootNativeValue;

    return nativeValue == mRootNativeValue;
  }

protected:

  // Return native value for the given DOM element.
  virtual bool GetValueFor(nsIContent *aContent, T *aValue) = 0;

  // Format native value to text attribute value.
  virtual void Format(const T& aValue, nsAString& aFormattedValue) = 0;

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
 * Class is used for the work with 'language' text attribute in nsTextAttrsMgr
 * class.
 */
class nsLangTextAttr : public nsTextAttr<nsAutoString>
{
public:
  nsLangTextAttr(nsHyperTextAccessible *aRootAcc, nsIContent *aRootContent,
                 nsIContent *aContent);

  // nsITextAttr
  virtual nsIAtom *GetName() const { return nsGkAtoms::language; }

protected:

  // nsTextAttr
  virtual bool GetValueFor(nsIContent *aElm, nsAutoString *aValue);
  virtual void Format(const nsAutoString& aValue, nsAString& aFormattedValue);

private:
  bool GetLang(nsIContent *aContent, nsAString& aLang);
  nsCOMPtr<nsIContent> mRootContent;
};


/**
 * Class is used for the work with CSS based text attributes in nsTextAttrsMgr
 * class.
 */
class nsCSSTextAttr : public nsTextAttr<nsAutoString>
{
public:
  nsCSSTextAttr(PRUint32 aIndex, nsIContent *aRootContent,
                nsIContent *aContent);

  // nsITextAttr
  virtual nsIAtom *GetName() const;

protected:

  // nsTextAttr
  virtual bool GetValueFor(nsIContent *aContent, nsAutoString *aValue);
  virtual void Format(const nsAutoString& aValue, nsAString& aFormattedValue);

private:
  PRInt32 mIndex;
};


/**
 * Class is used for the work with 'background-color' text attribute in
 * nsTextAttrsMgr class.
 */
class nsBGColorTextAttr : public nsTextAttr<nscolor>
{
public:
  nsBGColorTextAttr(nsIFrame *aRootFrame, nsIFrame *aFrame);

  // nsITextAttr
  virtual nsIAtom *GetName() const { return nsGkAtoms::backgroundColor; }

protected:
  // nsTextAttr
  virtual bool GetValueFor(nsIContent *aContent, nscolor *aValue);
  virtual void Format(const nscolor& aValue, nsAString& aFormattedValue);

private:
  bool GetColor(nsIFrame *aFrame, nscolor *aColor);
  nsIFrame *mRootFrame;
};


/**
 * Class is used for the work with "font-size" text attribute in nsTextAttrsMgr
 * class.
 */
class nsFontSizeTextAttr : public nsTextAttr<nscoord>
{
public:
  nsFontSizeTextAttr(nsIFrame *aRootFrame, nsIFrame *aFrame);

  // nsITextAttr
  virtual nsIAtom *GetName() const { return nsGkAtoms::font_size; }

protected:

  // nsTextAttr
  virtual bool GetValueFor(nsIContent *aContent, nscoord *aValue);
  virtual void Format(const nscoord& aValue, nsAString& aFormattedValue);

private:

  /**
   * Return font size for the given frame.
   *
   * @param aFrame      [in] the given frame to query font-size
   * @return            font size
   */
   nscoord GetFontSize(nsIFrame *aFrame);

  nsDeviceContext *mDC;
};


/**
 * Class is used for the work with "font-weight" text attribute in
 * nsTextAttrsMgr class.
 */
class nsFontWeightTextAttr : public nsTextAttr<PRInt32>
{
public:
  nsFontWeightTextAttr(nsIFrame *aRootFrame, nsIFrame *aFrame);

  // nsITextAttr
  virtual nsIAtom *GetName() const { return nsGkAtoms::fontWeight; }

protected:

  // nsTextAttr
  virtual bool GetValueFor(nsIContent *aElm, PRInt32 *aValue);
  virtual void Format(const PRInt32& aValue, nsAString& aFormattedValue);

private:

  /**
   * Return font weight for the given frame.
   *
   * @param aFrame      [in] the given frame to query font weight
   * @return            font weight
   */
  PRInt32 GetFontWeight(nsIFrame *aFrame);
};

#endif
