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
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Kew <jfkthame@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsFontFace.h"

nsFontFace::nsFontFace(gfxFontEntry* aFontEntry)
  : mFontEntry(aFontEntry)
{
}

nsFontFace::~nsFontFace()
{
}

////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ISUPPORTS1(nsFontFace, nsIDOMFontFace)

////////////////////////////////////////////////////////////////////////
// nsIDOMFontFace

/* readonly attribute boolean fromFontGroup; */
NS_IMETHODIMP
nsFontFace::GetFromFontGroup(PRBool * aFromFontGroup)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean fromLanguagePrefs; */
NS_IMETHODIMP
nsFontFace::GetFromLanguagePrefs(PRBool * aFromLanguagePrefs)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute boolean fromSystemFallback; */
NS_IMETHODIMP
nsFontFace::GetFromSystemFallback(PRBool * aFromSystemFallback)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute DOMString name; */
NS_IMETHODIMP
nsFontFace::GetName(nsAString & aName)
{
  aName = mFontEntry->Name();
  return NS_OK;
}

/* readonly attribute DOMString CSSFamilyName; */
NS_IMETHODIMP
nsFontFace::GetCSSFamilyName(nsAString & aCSSFamilyName)
{
  aCSSFamilyName = mFontEntry->FamilyName();
  return NS_OK;
}

/* readonly attribute nsIDOMCSSFontFaceRule rule; */
NS_IMETHODIMP
nsFontFace::GetRule(nsIDOMCSSFontFaceRule **aRule)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute long srcIndex; */
NS_IMETHODIMP
nsFontFace::GetSrcIndex(PRInt32 * aSrcIndex)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute DOMString URI; */
NS_IMETHODIMP
nsFontFace::GetURI(nsAString & aURI)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute DOMString localName; */
NS_IMETHODIMP
nsFontFace::GetLocalName(nsAString & aLocalName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute DOMString format; */
NS_IMETHODIMP
nsFontFace::GetFormat(nsAString & aFormat)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* readonly attribute DOMString metadata; */
NS_IMETHODIMP
nsFontFace::GetMetadata(nsAString & aMetadata)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
