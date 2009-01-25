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

#ifndef nsTextUtils_h_
#define nsTextUtils_h_

#include "nsIDOMElement.h"
#include "nsIDOMCSSStyleDeclaration.h"

#include "nsIContent.h"
#include "nsIFrame.h"

#include "nsCOMPtr.h"
#include "nsString.h"

/**
 * Base class to work with text attributes. See derived classes below.
 */
class nsTextAttr
{
public:
  /**
   * Return true if the text attribute for the given element equals with
   * predefined attribute.
   */
  virtual PRBool Equal(nsIDOMElement *aElm) = 0;
};

/**
 * Class is used for the work with 'lang' text attributes. Used in
 * nsHyperTextAccessible.
 */
class nsLangTextAttr : public nsTextAttr
{
public:
  nsLangTextAttr(nsAString& aLang, nsIContent *aRootContent) :
    mLang(aLang), mRootContent(aRootContent) { }

  virtual PRBool Equal(nsIDOMElement *aElm);

private:
  nsString mLang;
  nsCOMPtr<nsIContent> mRootContent;
};

/**
 * Class is used for the work with CSS based text attributes. Used in
 * nsHyperTextAccessible.
 */
class nsCSSTextAttr : public nsTextAttr
{
public:
  nsCSSTextAttr(PRBool aIncludeDefAttrValue, nsIDOMElement *aElm,
                nsIDOMElement *aRootElm);

  // nsTextAttr
  virtual PRBool Equal(nsIDOMElement *aElm);

  // nsCSSTextAttr
  /**
   * Interates through attributes.
   */
  PRBool Iterate();

  /**
   * Get name and value of attribute.
   */
  PRBool Get(nsACString& aName, nsAString& aValue);

private:
  PRInt32 mIndex;
  PRBool mIncludeDefAttrValue;

  nsCOMPtr<nsIDOMCSSStyleDeclaration> mStyleDecl;
  nsCOMPtr<nsIDOMCSSStyleDeclaration> mDefStyleDecl;
};

/**
 * Class is used for the work with "background-color" text attribute. It is
 * used in nsHyperTextAccessible.
 */
class nsBackgroundTextAttr : public nsTextAttr
{
public:
  nsBackgroundTextAttr(nsIFrame *aFrame, nsIFrame *aRootFrame);
  
  // nsTextAttr
  virtual PRBool Equal(nsIDOMElement *aElm);

  // nsBackgroundTextAttr

  /**
   * Retrieve the "background-color" in out param, return true if differs from
   * the default background-color.
   * 
   * @param aValue      [out] the background color in pts
   * @return            true if background color differs from default
   */
  virtual PRBool Get(nsAString& aValue);

private:
  /**
   * Return background color for the given frame.
   *
   * @note  If background color for the given frame is transparent then walk
   *        trhough the frame parents chain until we'll got either a frame with
   *        not transparent background color or the given root frame. In the
   *        last case return background color for the root frame.
   *
   * @param aFrame      [in] the given frame to calculate background-color
   * @return            background color
   */
  nscolor GetColor(nsIFrame *aFrame);

  nsIFrame *mFrame;
  nsIFrame *mRootFrame;
};

/**
 * Class is used for the work with "font-size" text attribute. It is
 * used in nsHyperTextAccessible.
 */
class nsFontSizeTextAttr : public nsTextAttr
{
public:
  nsFontSizeTextAttr(nsIFrame *aFrame, nsIFrame *aRootFrame);
  
  // nsTextAttr
  virtual PRBool Equal(nsIDOMElement *aElm);

  // nsFontSizeTextAttr

  /**
   * Retrieve the "font-size" in out param, return true if differs from
   * the default font-size.
   * 
   * @param aValue      [out] the font size in pts
   * @return            true if font size differs from default
   */
  virtual PRBool Get(nsAString& aValue);

private:
  /**
   * Return font size for the given frame.
   *
   * @param aFrame      [in] the given frame to query font-size
   * @return            font size
   */
   nscoord GetFontSize(nsIFrame *aFrame);

  nsIFrame *mFrame;
  nsIFrame *mRootFrame;
};

#endif
