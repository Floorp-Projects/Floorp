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
 * Netscape Communiactions Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Boris Zbarsky <bzbarsky@mit.edu>  (original author)
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

#ifndef nsLoadSaveContentSink_h__
#define nsLoadSaveContentSink_h__

#include "nsIExpatSink.h"
#include "nsIXMLContentSink.h"
#include "nsILoadSaveContentSink.h"
#include "nsCOMPtr.h"

/**
 * This class implements the core of the DOMBuilder for DOM3
 * Load/Save.  It holds a reference to an actual content sink that
 * constructs the content model.
 */
class nsLoadSaveContentSink : public nsILoadSaveContentSink,
                              public nsIExpatSink
{
public:
  nsLoadSaveContentSink();
  virtual ~nsLoadSaveContentSink();

  /**
   * Initializes the sink.  This will return an error if the arguments
   * do not satisfy some basic sanity-checks.
   * @param aBaseSink a "real" sink that the LoadSave sink can use to
   *                  build the document.  This must be non-null and
   *                  must also implement nsIExpatSink.
   */
  nsresult Init(nsIXMLContentSink* aBaseSink);
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXPATSINK

  // nsILoadSaveContentSink
  NS_IMETHOD WillBuildModel(void);
  NS_IMETHOD DidBuildModel(void);
  NS_IMETHOD WillInterrupt(void);
  NS_IMETHOD WillResume(void);
  NS_IMETHOD SetParser(nsIParser* aParser);  
  virtual void FlushContent(PRBool aNotify);
  NS_IMETHOD SetDocumentCharset(nsAString& aCharset);

private:
  /**
   * Pointers to the "real" sink.  We hold on to both just for
   * convenience sake.
   */
  nsCOMPtr<nsIXMLContentSink> mBaseSink;
  nsCOMPtr<nsIExpatSink> mExpatSink;
};

#endif // nsLoadSaveContentSink_h__
