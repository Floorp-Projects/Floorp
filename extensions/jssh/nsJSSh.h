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
 * The Original Code is Mozilla JavaScript Shell project.
 *
 * The Initial Developer of the Original Code is
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com>
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

#ifndef __NS_JSSH_H__
#define __NS_JSSH_H__

#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsIRunnable.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIJSSh.h"
#include "nsIJSContextStack.h"
#include "nsIPrincipal.h"
#include "nsStringAPI.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIXPCScriptable.h"

#ifdef MOZ_LOGGING
#define FORCE_PR_LOG
#endif
#include "prlog.h"

// NSPR_LOG_MODULES=jssh
#ifdef PR_LOGGING
extern PRLogModuleInfo *gJSShLog;
#define LOG(args) PR_LOG(gJSShLog, PR_LOG_DEBUG, args)
#else
#define LOG(args)
#endif

class nsJSSh : public nsIRunnable, public nsIJSSh,
               public nsIScriptObjectPrincipal,
               public nsIXPCScriptable
{
public:
  nsJSSh(nsIInputStream* input, nsIOutputStream*output,
         const nsACString &startupURI);
  ~nsJSSh();
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRUNNABLE
  NS_DECL_NSIJSSH
  // nsIScriptObjectPrincipal:
  virtual nsIPrincipal *GetPrincipal();
  NS_DECL_NSIXPCSCRIPTABLE
  
public:
  PRBool LoadURL(const char *url, jsval* retval=nsnull);
  
  nsCOMPtr<nsIInputStream> mInput;
  nsCOMPtr<nsIOutputStream> mOutput;
  nsCOMPtr<nsIPrincipal> mPrincipal;
  nsCOMPtr<nsIJSContextStack> mContextStack;
  PRInt32 mSuspendCount; // for suspend()-resume()
  
  enum { cBufferSize=100*1024 };
  char mBuffer[cBufferSize];
  int mBufferPtr;
  JSContext* mJSContext;
  JSObject *mGlobal;
  JSObject *mContextObj;
  PRBool mQuit;
  nsCString mStartupURI;
  nsCString mPrompt;
  PRBool mEmitHeader;
  nsCString mProtocol;
};

already_AddRefed<nsIRunnable>
CreateJSSh(nsIInputStream* input, nsIOutputStream*output,
           const nsACString &startupURI);

#endif // __NS_JSSH_H__
