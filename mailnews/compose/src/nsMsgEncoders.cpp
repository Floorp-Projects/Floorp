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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
#include "prprf.h"
#include "prmem.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"
#include "nsMsgComposeStringBundle.h"
#include "nsIServiceManager.h"
#include "nsMsgMimeCID.h"
#include "nsIMimeConverter.h"

extern "C" MimeEncoderData *
MIME_B64EncoderInit(nsresult (*output_fn) (const char *buf, PRInt32 size, void *closure), void *closure) 
{
  MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  nsresult res = CallCreateInstance(NS_MIME_CONVERTER_CONTRACTID, &converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) 
  {
    res = converter->B64EncoderInit(output_fn, closure, &returnEncoderData);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? returnEncoderData : nsnull;
}

extern "C" MimeEncoderData *	
MIME_QPEncoderInit(nsresult (*output_fn) (const char *buf, PRInt32 size, void *closure), void *closure) 
{
  MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  nsresult res = CallCreateInstance(NS_MIME_CONVERTER_CONTRACTID, &converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) 
  {
    res = converter->QPEncoderInit(output_fn, closure, &returnEncoderData);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? returnEncoderData : nsnull;
}

extern "C" MimeEncoderData *	
MIME_UUEncoderInit(char *filename, nsresult (*output_fn) (const char *buf, PRInt32 size, void *closure), void *closure) 
{
  MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  nsresult res = CallCreateInstance(NS_MIME_CONVERTER_CONTRACTID, &converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) 
  {
    res = converter->UUEncoderInit(filename, output_fn, closure, &returnEncoderData);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? returnEncoderData : nsnull;
}

extern "C" nsresult
MIME_EncoderDestroy(MimeEncoderData *data, PRBool abort_p) 
{
  //MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  nsresult res = CallCreateInstance(NS_MIME_CONVERTER_CONTRACTID, &converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) 
  {
    res = converter->EncoderDestroy(data, abort_p);
    NS_RELEASE(converter);
  }

  return NS_SUCCEEDED(res) ? 0 : -1;
}

extern "C" nsresult
MIME_EncoderWrite(MimeEncoderData *data, const char *buffer, PRInt32 size) 
{
  //  MimeEncoderData *returnEncoderData = nsnull;
  nsIMimeConverter *converter;
  PRInt32 written = 0;
  nsresult res = CallCreateInstance(NS_MIME_CONVERTER_CONTRACTID, &converter);
  if (NS_SUCCEEDED(res) && nsnull != converter) {
    res = converter->EncoderWrite(data, buffer, size, &written);
    NS_RELEASE(converter);
  }
  return NS_SUCCEEDED(res) ? 0 : -1;
}
