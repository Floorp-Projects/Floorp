/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *		John C. Griggs <johng@corel.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDeviceContextSpecFactoryQT.h"
#include "nsDeviceContextSpecQT.h"
#include "nsRenderingContextQT.h"
#include "nsGfxCIID.h"
#include "plstr.h"
#include <qapplication.h>

//JCG #define DBG_JCG 1

#ifdef DBG_JCG
PRUint32 gDCSpecFactoryCount = 0;
PRUint32 gDCSpecFactoryID = 0;
#endif

/** -------------------------------------------------------
 *  Constructor
 *  @update   dc 2/16/98
 */
nsDeviceContextSpecFactoryQT::nsDeviceContextSpecFactoryQT()
{
#ifdef DBG_JCG
  gDCSpecFactoryCount++;
  mID = gDCSpecFactoryID++;
  printf("JCG: nsDeviceContextSpecFactoryQT CTOR (%p) ID: %d, Count: %d\n",this,mID,gDCSpecFactoryCount);
#endif
   NS_INIT_REFCNT();
}

/** -------------------------------------------------------
 *  Destructor
 *  @update   dc 2/16/98
 */
nsDeviceContextSpecFactoryQT::~nsDeviceContextSpecFactoryQT()
{
#ifdef DBG_JCG
  gDCSpecFactoryCount--;
  printf("JCG: nsDeviceContextSpecFactoryQT DTOR (%p) ID: %d, Count: %d\n",this,mID,gDCSpecFactoryCount);
#endif
}

static NS_DEFINE_IID(kIDeviceContextSpecIID, NS_IDEVICE_CONTEXT_SPEC_IID);
static NS_DEFINE_IID(kDeviceContextSpecCID, NS_DEVICE_CONTEXT_SPEC_CID);

NS_IMPL_ISUPPORTS1(nsDeviceContextSpecFactoryQT, nsIDeviceContextSpecFactory)

/** -------------------------------------------------------
 *  Initialize the device context spec factory
 *  @update   dc 2/16/98
 */
NS_IMETHODIMP nsDeviceContextSpecFactoryQT::Init(void)
{
    return NS_OK;
}

/** -------------------------------------------------------
 *  Get a device context specification
 *  @update   dc 2/16/98
 */
NS_IMETHODIMP
nsDeviceContextSpecFactoryQT::CreateDeviceContextSpec(nsIWidget *aWidget,
                                                      nsIDeviceContextSpec *&aNewSpec,
                                                      PRBool aQuiet)
{
    nsresult  rv = NS_ERROR_FAILURE;
    nsIDeviceContextSpec  *devSpec = nsnull;

    nsComponentManager::CreateInstance(kDeviceContextSpecCID,nsnull, 
                                       kIDeviceContextSpecIID, 
                                       (void **)&devSpec);

    if (nsnull != devSpec) {
        if (NS_OK == ((nsDeviceContextSpecQT*)devSpec)->Init(aQuiet)) {
            aNewSpec = devSpec;
            rv = NS_OK;
        }
    }
    return rv;
}
