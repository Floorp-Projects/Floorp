/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Morten Nilsen <morten@nilsen.com>
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

#include "nsUCRegUtils.h"
#include "nsICategoryManager.h"
#include "nsCOMPtr.h"
#include "nsIServiceManagerUtils.h"
#include "nsXPIDLString.h"
#include "nsICharsetConverterManager.h"
#include "nsEncoderDecoderUtils.h"
#include "nsCRT.h"
#include "prprf.h"

static const char gIDFormat[] = 
  "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}";

static void
make_cid_string(char* buf, size_t size, const nsCID& cid)
{
    PR_snprintf(buf, size, gIDFormat,
                cid.m0, (PRUint32) cid.m1, (PRUint32) cid.m2,
                (PRUint32) cid.m3[0], (PRUint32) cid.m3[1],
                (PRUint32) cid.m3[2], (PRUint32) cid.m3[3],
                (PRUint32) cid.m3[4], (PRUint32) cid.m3[5],
                (PRUint32) cid.m3[6], (PRUint32) cid.m3[7]);
}

nsresult nsRegisterConverters(const nsConverterInfo* regInfo,
                              PRUint32 aCount)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString previous;
  PRUint32 i;
  for (i=0; i<aCount; i++) {
    const nsConverterInfo& entry = regInfo[i];
    const char *category;
    const char *key;

    if (entry.isEncoder) {
      category = NS_UNICODEENCODER_NAME;
    } else {
      category = NS_UNICODEDECODER_NAME;
    }
    key = entry.charset;

    // stolen from nsID::ToString, making it stack-based
    char value[40];
    make_cid_string(value, 40, entry.cid);
    rv = catman->AddCategoryEntry(category, key, value,
                                  PR_TRUE,
                                  PR_TRUE,
                                  getter_Copies(previous));
  }
  return rv;
}

nsresult nsUnregisterConverters(const nsConverterInfo* regInfo,
                                PRUint32 aCount)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman =
      do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
                                                                        \
  nsXPIDLCString previous;
  PRUint32 i;
  for (i=0; i<aCount; i++) {
    const nsConverterInfo& entry = regInfo[i];
    const char *category;
    const char *key;

    if (entry.isEncoder) {
      category = NS_UNICODEDECODER_NAME;
    } else {
      category = NS_UNICODEENCODER_NAME;
    }
    key = entry.charset;

    rv = catman->DeleteCategoryEntry(category, key, PR_TRUE);
  }
  return rv;
}
