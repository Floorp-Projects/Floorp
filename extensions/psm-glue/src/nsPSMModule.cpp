/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
*/

#include "nsIModule.h"
#include "nsIGenericFactory.h"

#include "nsPSMUICallbacks.h"
#include "nsPSMComponent.h"

#include "nsISecureBrowserUI.h"
#include "nsSecureBrowserUIImpl.h"

#include "nsSSLSocketProvider.h"
#include "nsTLSSocketProvider.h"

#include "nsSDR.h"
#include "nsFSDR.h"
#include "nsCrypto.h"
#include "nsKeygenHandler.h"
//For the NS_CRYPTO_PROGID define
#include "nsDOMCID.h"

#include "nsCURILoader.h"
#include "nsISupportsUtils.h"

// Define SDR object constructor
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_CID(kFormProcessorCID, NS_FORMPROCESSOR_CID); 

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSecretDecoderRing, init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsFSecretDecoderRing, init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCrypto, init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsPkcs11, init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(CertContentListener, init);

static nsModuleComponentInfo components[] =
{
    { 
        PSM_COMPONENT_CLASSNAME,  
        NS_PSMCOMPONENT_CID, 
        PSM_COMPONENT_PROGID,   
        nsPSMComponent::CreatePSMComponent
    },
    
    { 
        "PSM Content Handler - application/x-x509-ca-cert",  
        NS_PSMCOMPONENT_CID, 
        NS_CONTENT_HANDLER_PROGID_PREFIX"application/x-x509-ca-cert",   
        nsPSMComponent::CreatePSMComponent
    },
    
    { 
        "PSM Content Handler - application/x-x509-server-cert",  
        NS_PSMCOMPONENT_CID, 
        NS_CONTENT_HANDLER_PROGID_PREFIX"application/x-x509-server-cert",   
        nsPSMComponent::CreatePSMComponent
    },

    { 
        "PSM Content Handler - application/x-x509-user-cert",  
        NS_PSMCOMPONENT_CID, 
        NS_CONTENT_HANDLER_PROGID_PREFIX"application/x-x509-user-cert",   
        nsPSMComponent::CreatePSMComponent
    },
    
    { 
        "PSM Content Handler - application/x-x509-email-cert",  
        NS_PSMCOMPONENT_CID, 
        NS_CONTENT_HANDLER_PROGID_PREFIX"application/x-x509-email-cert",   
        nsPSMComponent::CreatePSMComponent
    },


    { 
        PSM_UI_HANLDER_CLASSNAME, 
        NS_PSMUIHANDLER_CID, 
        PSM_UI_HANLDER_PROGID,  
        nsPSMUIHandlerImpl::CreatePSMUIHandler
    },

    { 
        NS_SECURE_BROWSER_UI_CLASSNAME, 
        NS_SECURE_BROWSER_UI_CID, 
        NS_SECURE_BROWSER_UI_PROGID, 
        nsSecureBrowserUIImpl::Create 
    },

    { 
        NS_SECURE_BROWSER_DOCOBSERVER_CLASSNAME, 
        NS_SECURE_BROWSER_DOCOBSERVER_CID, 
        NS_SECURE_BROWSER_DOCOBSERVER_PROGID, 
        nsSecureBrowserUIImpl::Create
    },

    { 
        NS_ISSLSOCKETPROVIDER_CLASSNAME,
        NS_SSLSOCKETPROVIDER_CID,
        NS_ISSLSOCKETPROVIDER_PROGID,
        nsSSLSocketProvider::Create 
    },

    { 
        NS_ISSLFHSOCKETPROVIDER_CLASSNAME,
        NS_SSLSOCKETPROVIDER_CID,
        NS_ISSLFHSOCKETPROVIDER_PROGID,
        nsSSLSocketProvider::Create 
    },

    { 
        NS_TLSSOCKETPROVIDER_CLASSNAME,
        NS_TLSSOCKETPROVIDER_CID,
        NS_TLSSOCKETPROVIDER_PROGID,
        nsTLSSocketProvider::Create 
    },

    {
        NS_SDR_CLASSNAME,
        NS_SDR_CID,
        NS_SDR_PROGID,
        nsSecretDecoderRingConstructor
    },

    {
        NS_FSDR_CLASSNAME,
        NS_FSDR_CID,
        NS_FSDR_PROGID,
        nsFSecretDecoderRingConstructor
    },
    
    {
        NS_CRYPTO_CLASSNAME,
        NS_CRYPTO_CID,
        NS_CRYPTO_PROGID,
        nsCryptoConstructor
    },
    {
        NS_PKCS11_CLASSNAME,
        NS_PKCS11_CID,
        NS_PKCS11_PROGID,
        nsPkcs11Constructor
    },
    {
      "Generic Certificate Content Handler",
      NS_CERTCONTENTLISTEN_CID,
      NS_CERTCONTENTLISTEN_PROGID,
      CertContentListenerConstructor
    },
    {
      "Form Processor",
      NS_FORMPROCESSOR_CID,
      NS_FORMPROCESSOR_PROGID,
      nsKeygenFormProcessor::Create
    }
};

NS_IMPL_NSGETMODULE("PSMComponent", components);
