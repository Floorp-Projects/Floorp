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
 * Portions created by the Initial Developer are Copyright (C) 2001
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

#ifndef nsSOAPEncoding_h__
#define nsSOAPEncoding_h__

#include "nsString.h"
#include "nsIDOMElement.h"
#include "nsISOAPEncoding.h"  // contains nsISOAPEncodingRegistry too
#include "nsISOAPEncoder.h"
#include "nsISOAPDecoder.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsISchema.h"

// Notes regarding the ownership model between the nsSOAPEncoding (encoding)
//   and the nsSOAPEncodingRegsitry (registry). To avoid cyclic referencing
//   and thereby leaking encodings and the registry holding them the registry 
//   was given ownership of the encodings. In the encoding specialized 
//   AddRef() and Release() functions were written to pass the refcounting
//   to the registry. Instead of ref couting the encodings all refs are
//   accumulated in the registry. When the registry is the only thing holding
//   on to the encodings it's own refcount will hit zero causing it to 
//   go away and take with it any encodings stored in its hashtable. To make
//   this work the registry had to be a weak reference (*-style pointer)
//   in each encoding and the storage of the encodings had to be weak as
//   well (*-style pointers in the nsObjectHashtable).

class nsSOAPEncodingRegistry : public nsISOAPEncodingRegistry
{

public:
  NS_DECL_ISUPPORTS 
  NS_DECL_NSISOAPENCODINGREGISTRY

  nsSOAPEncodingRegistry(nsISOAPEncoding *aEncoding);

  // the nsObjectHashtable has a callback to delete all the encodings
  //    the callback is defined in nsSOAPEncoding.cpp
  virtual ~nsSOAPEncodingRegistry() {}

protected:
  nsObjectHashtable mEncodings;
  nsCOMPtr < nsISchemaCollection > mSchemaCollection;

};


class nsSOAPEncoding : public nsISOAPEncoding
{

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISOAPENCODING 
  
  nsSOAPEncoding();
  nsSOAPEncoding(const nsAString &aStyleURI,
                 nsSOAPEncodingRegistry *aRegistry,
                 nsISOAPEncoding *aDefaultEncoding);
  virtual ~nsSOAPEncoding() {}

protected:
  nsString mStyleURI;
  nsSupportsHashtable mEncoders;
  nsSupportsHashtable mDecoders;

  // Weak Reference to avoid cyclic refs and leaks. See notes above.
  nsISOAPEncodingRegistry *mRegistry;

  nsCOMPtr < nsISOAPEncoding > mDefaultEncoding;
  nsCOMPtr < nsISOAPEncoder > mDefaultEncoder;
  nsCOMPtr < nsISOAPDecoder > mDefaultDecoder;
  nsSupportsHashtable mMappedInternal;
  nsSupportsHashtable mMappedExternal;

};
#endif
