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
 * The Original Code is mozilla.org HTML Sanitizer code.
 *
 * The Initial Developer of the Original Code is
 * Ben Bucksch <mozilla@bucksch.org>.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Netscape
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

/* Cleans up HTML source from unwanted tags/attributes

   This class implements a content sink, which takes a parsed HTML document
   and removes all tags and attributes that are not explicitly allowed.

   This may improve the viewing experience of the user and/or the
   security/privacy.

   What is allowed is defined by a string (format described before the
   implementation of |mozHTMLSanitizer::ParsePrefs()|). The sytnax of the
   definition is not very rich - you can only (dis)allow certain tags and
   attributes, but not where they may appear. (This makes the implementation
   much more simple.) E.g. it is impossible to disallow ordinary text as a
   direct child of the <head> node or to disallow multiple <head> nodes.

   We also remove some known bad attribute values like javascript: URLs.
   Draconian attitude.

   Currently, the output of this class is unparsed (!) HTML source, which
   means that each document has to go through the parser twice. Of course,
   that is a performance killer. There are some reasons for for me doing it
   that way:
   * There is, to my knowledge, no interface to hook up such modifiers
     in the document display data flow. We have a nice interface for doing
     the modifications (the DOM), but no place to get the DOM and to invoke
     this code. As I don't want to hack this directly into the html sink,
     I'd have to create a generic interface first, which is too much work for
     me at the moment.
   * It is quite easy to hook up modifiers for the (unparsed) data stream,
     both in netwerk (for the browser) and esp. in libmime (for Mailnews).
   * It seems like the safest method - it is easier to debug (you have the
     HTML source output to check) and is less prone to security-relevant bugs
     and regressions, because in the case of a bug, it will probably fall back
     to not outputting, which is safer than erring on the side of letting
     something slip through (most of the alternative approaches listed below
     are probably vulnerable to the latter).
   * It should be possible to later change this class to output a parsed HTML
     document.
   So, in other words, I had the choice between better design and better
   performance. I choose design. Bad performance has an effect on the users
   of this class only, while bad design has an effect on all users and 
   programmers.

   That being said, I have some ideas, how do make it much more efficient, but
   they involve hacking core code.
   * At some point when we have DOM, but didn't do anything with it yet
     (in particular, didn't load any external objects or ran any javascript),
     walk the DOM and delete everything the user doesn't explicitly like.
   * There's this nice GetPref() in the HTMLContentSink. It isn't used exactly
     as I would like to, but that should be doable. Bascially, before
     processing any tag (e.g. in OpenContainer or AddLeaf), ask that
     function, if the tag is allowed. If not, just return.
   In any case, there's the problem, how the users of the renderer
   (e.g. Mailnews) can tell it to use the sanitizer and which tags are
   allowed (the browser may want to allow more tags than Mailnews).
   That probably means that I have to hack into the docshell (incl. its
   interface) or similar, which I would really like to avoid.
   Any ideas appreciated.
*/
#ifndef _mozISanitizingSerializer_h__
#define _mozISanitizingSerializer_h__

#include "nsISupports.h"

class nsAString;

#define MOZ_SANITIZINGHTMLSERIALIZER_CONTRACTID "@mozilla.org/layout/htmlsanitizer;1"

/* starting interface:    nsIContentSerializer */
#define MOZ_ISANITIZINGHTMLSERIALIZER_IID_STR "feca3c34-205e-4ae5-bd1c-03c686ff012b"

#define MOZ_ISANITIZINGHTMLSERIALIZER_IID \
  {0xfeca3c34, 0x205e, 0x4ae5, \
    { 0xbd, 0x1c, 0x03, 0xc6, 0x86, 0xff, 0x01, 0x2b }}

class mozISanitizingHTMLSerializer : public nsISupports {
 public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(MOZ_ISANITIZINGHTMLSERIALIZER_IID)

  NS_IMETHOD Initialize(nsAString* aOutString,
                        PRUint32 aFlags,
                        const nsAString& allowedTags) = 0;
  // This function violates string ownership rules, see impl.
};

NS_DEFINE_STATIC_IID_ACCESSOR(mozISanitizingHTMLSerializer,
                              MOZ_ISANITIZINGHTMLSERIALIZER_IID)

#endif
