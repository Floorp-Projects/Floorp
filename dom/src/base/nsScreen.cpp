/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nscore.h"
#include "nsScreen.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebShell.h"
#include "nsIDeviceContext.h"
#include "nsIPresContext.h"
#include "nsCOMPtr.h"
#include "nsIDocumentViewer.h"
#include "nsIDocumentLoader.h"

static NS_DEFINE_IID(kIDocumentViewerIID,     NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kIDocumentLoaderIID,     NS_IDOCUMENTLOADER_IID);

static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIDOMScreenIID, NS_IDOMSCREEN_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

//
//  Screen class implementation 
//
ScreenImpl::ScreenImpl( nsIWebShell* aWebShell): mWebShell( aWebShell )
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  NS_IF_ADDREF( mWebShell );
}

ScreenImpl::~ScreenImpl()
{
	NS_IF_RELEASE( mWebShell );
}

NS_IMPL_ADDREF(ScreenImpl)
NS_IMPL_RELEASE(ScreenImpl)

nsresult 
ScreenImpl::QueryInterface(const nsIID& aIID,
                              void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIScriptObjectOwnerIID)) {
    *aInstancePtrResult = (void*) ((nsIScriptObjectOwner*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMScreenIID)) {
    *aInstancePtrResult = (void*) ((nsIDOMScreen*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*)(nsISupports*)(nsIScriptObjectOwner*)this;
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
ScreenImpl::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

NS_IMETHODIMP
ScreenImpl::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) {
    nsIScriptGlobalObject *global = aContext->GetGlobalObject();
    res = NS_NewScriptScreen(aContext, (nsIDOMScreen*)this, (nsIDOMWindow*)global, &mScriptObject);
    NS_IF_RELEASE(global);
  }
  
  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP
ScreenImpl::GetWidth(PRInt32* aWidth)
{
	nsIDeviceContext* context = GetDeviceContext();
	if ( context )
	{
		PRInt32 height;
		context->GetDeviceSurfaceDimensions( *aWidth, height  );
		float devUnits;
		context->GetDevUnitsToAppUnits(devUnits);
		*aWidth = NSToIntRound(float( *aWidth) / devUnits );
		NS_RELEASE( context );
		return NS_OK;
	}

  *aWidth = -1;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
ScreenImpl::GetHeight(PRInt32* aHeight)
{
	nsIDeviceContext* context = GetDeviceContext();
	if ( context )
	{
		PRInt32 width;
		context->GetDeviceSurfaceDimensions( width, *aHeight  );
		float devUnits;
		context->GetDevUnitsToAppUnits(devUnits);
		*aHeight = NSToIntRound(float( *aHeight) / devUnits );
		NS_RELEASE( context );
		return NS_OK;
	}
	
  *aHeight = -1;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
ScreenImpl::GetPixelDepth(PRInt32* aPixelDepth)
{
	nsIDeviceContext* context = GetDeviceContext();
	if ( context )
	{
		PRUint32 depth;
		context->GetDepth( depth  );
		*aPixelDepth = depth;
		NS_RELEASE( context );
		return NS_OK;
	}
  //XXX not implmented
  *aPixelDepth = -1;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
ScreenImpl::GetColorDepth(PRInt32* aColorDepth)
{
	nsIDeviceContext* context = GetDeviceContext();
	if ( context )
	{
		PRUint32 depth;
		context->GetDepth( depth  );
		*aColorDepth = depth;
		NS_RELEASE( context );
		return NS_OK;
	}
  
  *aColorDepth = -1;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
ScreenImpl::GetAvailWidth(PRInt32* aAvailWidth)
{

  
  nsIDeviceContext* context = GetDeviceContext();
  if ( context )
  {
	nsRect rect;
	context->GetClientRect( rect  );
	float devUnits;
	context->GetDevUnitsToAppUnits(devUnits);
	*aAvailWidth = NSToIntRound(float( rect.width) / devUnits );
	NS_RELEASE( context );
	return NS_OK;
 }

  *aAvailWidth = -1;
  return NS_ERROR_FAILURE;

}

NS_IMETHODIMP
ScreenImpl::GetAvailHeight(PRInt32* aAvailHeight)
{
 nsIDeviceContext* context = GetDeviceContext();
  if ( context )
  {
	nsRect rect;
	context->GetClientRect( rect  );
	float devUnits;
	context->GetDevUnitsToAppUnits(devUnits);
	*aAvailHeight = NSToIntRound(float( rect.height) / devUnits );
	NS_RELEASE( context );
	return NS_OK;
 }

  *aAvailHeight = -1;
  return NS_ERROR_FAILURE;

}

NS_IMETHODIMP
ScreenImpl::GetAvailLeft(PRInt32* aAvailLeft)
{
 	 nsIDeviceContext* context = GetDeviceContext();
  if ( context )
  {
	nsRect rect;
	context->GetClientRect( rect  );
	float devUnits;
	context->GetDevUnitsToAppUnits(devUnits);
	*aAvailLeft = NSToIntRound(float( rect.x) / devUnits );
	NS_RELEASE( context );
	return NS_OK;
 }

  *aAvailLeft = -1;
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
ScreenImpl::GetAvailTop(PRInt32* aAvailTop)
{
  	 nsIDeviceContext* context = GetDeviceContext();
  if ( context )
  {
	nsRect rect;
	context->GetClientRect( rect  );
	float devUnits;
	context->GetDevUnitsToAppUnits(devUnits);
	*aAvailTop = NSToIntRound(float( rect.y) / devUnits );
	NS_RELEASE( context );
	return NS_OK;
 }

  *aAvailTop = -1;
  return NS_ERROR_FAILURE;
}

nsIDeviceContext* ScreenImpl::GetDeviceContext()
{
	nsCOMPtr<nsIDocumentViewer> docViewer;
	nsCOMPtr<nsIContentViewer> contentViewer;
	nsIPresContext* presContext = nsnull;
	nsIDeviceContext* context = nsnull;
  
	if( mWebShell &&  NS_SUCCEEDED( mWebShell->GetContentViewer( getter_AddRefs(contentViewer) )) )
	{
		if ( NS_SUCCEEDED( contentViewer->QueryInterface(kIDocumentViewerIID, getter_AddRefs(docViewer) ) ) )
		{   
				if (NS_SUCCEEDED( docViewer->GetPresContext( presContext ) ) )
			  {  
			  	presContext->GetDeviceContext( &context );
			 	}
			 	NS_IF_RELEASE( presContext );	
		 }
	}
  return context;
}


