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
#include "nsHTMLAtoms.h"
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
  PRBool mNeedToBlockParser;
};

// these two lists are used by the sanitizing fragment serializers
static nsIAtom** const kDefaultAllowedTags [] = {
  &nsHTMLAtoms::a,
  &nsHTMLAtoms::abbr,
  &nsHTMLAtoms::acronym,
  &nsHTMLAtoms::address,
  &nsHTMLAtoms::area,
  &nsHTMLAtoms::b,
  &nsHTMLAtoms::big,
  &nsHTMLAtoms::blockquote,
  &nsHTMLAtoms::br,
  &nsHTMLAtoms::button,
  &nsHTMLAtoms::caption,
  &nsHTMLAtoms::center,
  &nsHTMLAtoms::cite,
  &nsHTMLAtoms::code,
  &nsHTMLAtoms::col,
  &nsHTMLAtoms::colgroup,
  &nsHTMLAtoms::dd,
  &nsHTMLAtoms::del,
  &nsHTMLAtoms::dfn,
  &nsHTMLAtoms::dir,
  &nsHTMLAtoms::div,
  &nsHTMLAtoms::dl,
  &nsHTMLAtoms::dt,
  &nsHTMLAtoms::em,
  &nsHTMLAtoms::fieldset,
  &nsHTMLAtoms::font,
  &nsHTMLAtoms::form,
  &nsHTMLAtoms::h1,
  &nsHTMLAtoms::h2,
  &nsHTMLAtoms::h3,
  &nsHTMLAtoms::h4,
  &nsHTMLAtoms::h5,
  &nsHTMLAtoms::h6,
  &nsHTMLAtoms::hr,
  &nsHTMLAtoms::i,
  &nsHTMLAtoms::img,
  &nsHTMLAtoms::input,
  &nsHTMLAtoms::ins,
  &nsHTMLAtoms::kbd,
  &nsHTMLAtoms::label,
  &nsHTMLAtoms::legend,
  &nsHTMLAtoms::li,
  &nsHTMLAtoms::map,
  &nsHTMLAtoms::menu,
  &nsHTMLAtoms::ol,
  &nsHTMLAtoms::optgroup,
  &nsHTMLAtoms::option,
  &nsHTMLAtoms::p,
  &nsHTMLAtoms::pre,
  &nsHTMLAtoms::q,
  &nsHTMLAtoms::s,
  &nsHTMLAtoms::samp,
  &nsHTMLAtoms::select,
  &nsHTMLAtoms::small,
  &nsHTMLAtoms::span,
  &nsHTMLAtoms::strike,
  &nsHTMLAtoms::strong,
  &nsHTMLAtoms::sub,
  &nsHTMLAtoms::sup,
  &nsHTMLAtoms::table,
  &nsHTMLAtoms::tbody,
  &nsHTMLAtoms::td,
  &nsHTMLAtoms::textarea,
  &nsHTMLAtoms::tfoot,
  &nsHTMLAtoms::th,
  &nsHTMLAtoms::thead,
  &nsHTMLAtoms::tr,
  &nsHTMLAtoms::tt,
  &nsHTMLAtoms::u,
  &nsHTMLAtoms::ul
};

static nsIAtom** const kDefaultAllowedAttributes [] = {
  &nsHTMLAtoms::accept,
  &nsHTMLAtoms::acceptcharset,
  &nsHTMLAtoms::accesskey,
  &nsHTMLAtoms::action,
  &nsHTMLAtoms::align,
  &nsHTMLAtoms::alt,
  &nsHTMLAtoms::axis,
  &nsHTMLAtoms::border,
  &nsHTMLAtoms::cellpadding,
  &nsHTMLAtoms::cellspacing,
  &nsHTMLAtoms::_char,
  &nsHTMLAtoms::charoff,
  &nsHTMLAtoms::charset,
  &nsHTMLAtoms::checked,
  &nsHTMLAtoms::cite,
  &nsHTMLAtoms::_class,
  &nsHTMLAtoms::clear,
  &nsHTMLAtoms::cols,
  &nsHTMLAtoms::colspan,
  &nsHTMLAtoms::color,
  &nsHTMLAtoms::compact,
  &nsHTMLAtoms::coords,
  &nsHTMLAtoms::datetime,
  &nsHTMLAtoms::dir,
  &nsHTMLAtoms::disabled,
  &nsHTMLAtoms::enctype,
  &nsHTMLAtoms::_for,
  &nsHTMLAtoms::frame,
  &nsHTMLAtoms::headers,
  &nsHTMLAtoms::height,
  &nsHTMLAtoms::href,
  &nsHTMLAtoms::hreflang,
  &nsHTMLAtoms::hspace,
  &nsHTMLAtoms::id,
  &nsHTMLAtoms::ismap,
  &nsHTMLAtoms::label,
  &nsHTMLAtoms::lang,
  &nsHTMLAtoms::longdesc,
  &nsHTMLAtoms::maxlength,
  &nsHTMLAtoms::media,
  &nsHTMLAtoms::method,
  &nsHTMLAtoms::multiple,
  &nsHTMLAtoms::name,
  &nsHTMLAtoms::nohref,
  &nsHTMLAtoms::noshade,
  &nsHTMLAtoms::nowrap,
  &nsHTMLAtoms::prompt,
  &nsHTMLAtoms::readonly,
  &nsHTMLAtoms::rel,
  &nsHTMLAtoms::rev,
  &nsHTMLAtoms::rows,
  &nsHTMLAtoms::rowspan,
  &nsHTMLAtoms::rules,
  &nsHTMLAtoms::scope,
  &nsHTMLAtoms::selected,
  &nsHTMLAtoms::shape,
  &nsHTMLAtoms::size,
  &nsHTMLAtoms::span,
  &nsHTMLAtoms::src,
  &nsHTMLAtoms::start,
  &nsHTMLAtoms::summary,
  &nsHTMLAtoms::tabindex,
  &nsHTMLAtoms::target,
  &nsHTMLAtoms::title,
  &nsHTMLAtoms::type,
  &nsHTMLAtoms::usemap,
  &nsHTMLAtoms::valign,
  &nsHTMLAtoms::value,
  &nsHTMLAtoms::vspace,
  &nsHTMLAtoms::width
};

// URIs action, href, src, longdesc, usemap, cite
static
PRBool IsAttrURI(nsIAtom *aName)
{
  return (aName == nsHTMLAtoms::action ||
          aName == nsHTMLAtoms::href ||
          aName == nsHTMLAtoms::src ||
          aName == nsHTMLAtoms::longdesc ||
          aName == nsHTMLAtoms::usemap ||
          aName == nsHTMLAtoms::cite);
}
#endif // _nsContentSink_h_
