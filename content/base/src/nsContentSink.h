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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

/*
 * Base class for the XML and HTML content sinks, which construct a
 * DOM based on information from the parser.
 */

#ifndef _nsContentSink_h_
#define _nsContentSink_h_

// Base class for contentsink implementations.

#include "nsICSSLoaderObserver.h"
#include "nsIScriptLoaderObserver.h"
#include "nsWeakReference.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsString.h"
#include "nsAutoPtr.h"
#include "nsGkAtoms.h"
#include "nsTHashtable.h"
#include "nsHashKeys.h"

class nsIDocument;
class nsIURI;
class nsIChannel;
class nsIDocShell;
class nsICSSLoader;
class nsIParser;
class nsIAtom;
class nsIChannel;
class nsIContent;
class nsIViewManager;
class nsNodeInfoManager;

class nsContentSink : public nsICSSLoaderObserver,
                      public nsIScriptLoaderObserver,
                      public nsSupportsWeakReference
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSISCRIPTLOADEROBSERVER
  
  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aWasAlternate,
                              nsresult aStatus);

  nsresult ProcessMETATag(nsIContent* aContent);

protected:
  nsContentSink();
  virtual ~nsContentSink();

  nsresult Init(nsIDocument* aDoc, nsIURI* aURI,
                nsISupports* aContainer, nsIChannel* aChannel);

  nsresult ProcessHTTPHeaders(nsIChannel* aChannel);
  nsresult ProcessHeaderData(nsIAtom* aHeader, const nsAString& aValue,
                             nsIContent* aContent = nsnull);
  nsresult ProcessLinkHeader(nsIContent* aElement,
                             const nsAString& aLinkData);
  nsresult ProcessLink(nsIContent* aElement, const nsSubstring& aHref,
                       const nsSubstring& aRel, const nsSubstring& aTitle,
                       const nsSubstring& aType, const nsSubstring& aMedia);

  virtual nsresult ProcessStyleLink(nsIContent* aElement,
                                    const nsSubstring& aHref,
                                    PRBool aAlternate,
                                    const nsSubstring& aTitle,
                                    const nsSubstring& aType,
                                    const nsSubstring& aMedia);

  void PrefetchHref(const nsAString &aHref, PRBool aExplicit);

  PRBool ScrollToRef(PRBool aReallyScroll);
  nsresult RefreshIfEnabled(nsIViewManager* vm);
  void StartLayout(PRBool aIsFrameset);

  // Overridable hooks into script evaluation
  virtual void PreEvaluateScript()                            {return;}
  virtual void PostEvaluateScript(nsIScriptElement *aElement) {return;}

  nsCOMPtr<nsIDocument>         mDocument;
  nsCOMPtr<nsIParser>           mParser;
  nsCOMPtr<nsIURI>              mDocumentURI;
  nsCOMPtr<nsIURI>              mDocumentBaseURI;
  nsCOMPtr<nsIDocShell>         mDocShell;
  nsCOMPtr<nsICSSLoader>        mCSSLoader;
  nsRefPtr<nsNodeInfoManager>   mNodeInfoManager;

  nsCOMArray<nsIScriptElement> mScriptElements;

  nsCString mRef; // ScrollTo #ref
};


//
// these two lists are used by the sanitizing fragment serializers
// Thanks to Mark Pilgrim and Sam Ruby for the initial whitelist
//
static nsIAtom** const kDefaultAllowedTags [] = {
  &nsGkAtoms::a,
  &nsGkAtoms::abbr,
  &nsGkAtoms::acronym,
  &nsGkAtoms::address,
  &nsGkAtoms::area,
  &nsGkAtoms::b,
  &nsGkAtoms::bdo,
  &nsGkAtoms::big,
  &nsGkAtoms::blockquote,
  &nsGkAtoms::br,
  &nsGkAtoms::button,
  &nsGkAtoms::caption,
  &nsGkAtoms::center,
  &nsGkAtoms::cite,
  &nsGkAtoms::code,
  &nsGkAtoms::col,
  &nsGkAtoms::colgroup,
  &nsGkAtoms::dd,
  &nsGkAtoms::del,
  &nsGkAtoms::dfn,
  &nsGkAtoms::dir,
  &nsGkAtoms::div,
  &nsGkAtoms::dl,
  &nsGkAtoms::dt,
  &nsGkAtoms::em,
  &nsGkAtoms::fieldset,
  &nsGkAtoms::font,
  &nsGkAtoms::form,
  &nsGkAtoms::h1,
  &nsGkAtoms::h2,
  &nsGkAtoms::h3,
  &nsGkAtoms::h4,
  &nsGkAtoms::h5,
  &nsGkAtoms::h6,
  &nsGkAtoms::hr,
  &nsGkAtoms::i,
  &nsGkAtoms::img,
  &nsGkAtoms::input,
  &nsGkAtoms::ins,
  &nsGkAtoms::kbd,
  &nsGkAtoms::label,
  &nsGkAtoms::legend,
  &nsGkAtoms::li,
  &nsGkAtoms::listing,
  &nsGkAtoms::map,
  &nsGkAtoms::menu,
  &nsGkAtoms::nobr,
  &nsGkAtoms::ol,
  &nsGkAtoms::optgroup,
  &nsGkAtoms::option,
  &nsGkAtoms::p,
  &nsGkAtoms::pre,
  &nsGkAtoms::q,
  &nsGkAtoms::s,
  &nsGkAtoms::samp,
  &nsGkAtoms::select,
  &nsGkAtoms::small,
  &nsGkAtoms::span,
  &nsGkAtoms::strike,
  &nsGkAtoms::strong,
  &nsGkAtoms::sub,
  &nsGkAtoms::sup,
  &nsGkAtoms::table,
  &nsGkAtoms::tbody,
  &nsGkAtoms::td,
  &nsGkAtoms::textarea,
  &nsGkAtoms::tfoot,
  &nsGkAtoms::th,
  &nsGkAtoms::thead,
  &nsGkAtoms::tr,
  &nsGkAtoms::tt,
  &nsGkAtoms::u,
  &nsGkAtoms::ul,
  &nsGkAtoms::var
};

static nsIAtom** const kDefaultAllowedAttributes [] = {
  &nsGkAtoms::abbr,
  &nsGkAtoms::accept,
  &nsGkAtoms::acceptcharset,
  &nsGkAtoms::accesskey,
  &nsGkAtoms::action,
  &nsGkAtoms::align,
  &nsGkAtoms::alt,
  &nsGkAtoms::autocomplete,
  &nsGkAtoms::axis,
  &nsGkAtoms::background,
  &nsGkAtoms::bgcolor,
  &nsGkAtoms::border,
  &nsGkAtoms::cellpadding,
  &nsGkAtoms::cellspacing,
  &nsGkAtoms::_char,
  &nsGkAtoms::charoff,
  &nsGkAtoms::charset,
  &nsGkAtoms::checked,
  &nsGkAtoms::cite,
  &nsGkAtoms::_class,
  &nsGkAtoms::clear,
  &nsGkAtoms::cols,
  &nsGkAtoms::colspan,
  &nsGkAtoms::color,
  &nsGkAtoms::compact,
  &nsGkAtoms::coords,
  &nsGkAtoms::datetime,
  &nsGkAtoms::dir,
  &nsGkAtoms::disabled,
  &nsGkAtoms::enctype,
  &nsGkAtoms::_for,
  &nsGkAtoms::frame,
  &nsGkAtoms::headers,
  &nsGkAtoms::height,
  &nsGkAtoms::href,
  &nsGkAtoms::hreflang,
  &nsGkAtoms::hspace,
  &nsGkAtoms::id,
  &nsGkAtoms::ismap,
  &nsGkAtoms::label,
  &nsGkAtoms::lang,
  &nsGkAtoms::longdesc,
  &nsGkAtoms::maxlength,
  &nsGkAtoms::media,
  &nsGkAtoms::method,
  &nsGkAtoms::multiple,
  &nsGkAtoms::name,
  &nsGkAtoms::nohref,
  &nsGkAtoms::noshade,
  &nsGkAtoms::nowrap,
  &nsGkAtoms::pointSize,
  &nsGkAtoms::prompt,
  &nsGkAtoms::readonly,
  &nsGkAtoms::rel,
  &nsGkAtoms::rev,
  &nsGkAtoms::role,
  &nsGkAtoms::rows,
  &nsGkAtoms::rowspan,
  &nsGkAtoms::rules,
  &nsGkAtoms::scope,
  &nsGkAtoms::selected,
  &nsGkAtoms::shape,
  &nsGkAtoms::size,
  &nsGkAtoms::span,
  &nsGkAtoms::src,
  &nsGkAtoms::start,
  &nsGkAtoms::summary,
  &nsGkAtoms::tabindex,
  &nsGkAtoms::target,
  &nsGkAtoms::title,
  &nsGkAtoms::type,
  &nsGkAtoms::usemap,
  &nsGkAtoms::valign,
  &nsGkAtoms::value,
  &nsGkAtoms::vspace,
  &nsGkAtoms::width
};

// URIs action, href, src, longdesc, usemap, cite
static
PRBool IsAttrURI(nsIAtom *aName)
{
  return (aName == nsGkAtoms::action ||
          aName == nsGkAtoms::href ||
          aName == nsGkAtoms::src ||
          aName == nsGkAtoms::longdesc ||
          aName == nsGkAtoms::usemap ||
          aName == nsGkAtoms::cite ||
          aName == nsGkAtoms::background);
}
#endif // _nsContentSink_h_
