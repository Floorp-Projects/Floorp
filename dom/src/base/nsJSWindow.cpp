/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 */
/* AUTO-GENERATED. DO NOT EDIT!!! */

#include "jsapi.h"
#include "nsJSUtils.h"
#include "nsDOMError.h"
#include "nscore.h"
#include "nsIServiceManager.h"
#include "nsIScriptContext.h"
#include "nsIScriptSecurityManager.h"
#include "nsIJSScriptObject.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsCOMPtr.h"
#include "nsDOMPropEnums.h"
#include "nsString.h"
#include "nsIDOMNavigator.h"
#include "nsIPrompt.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsIDOMDocument.h"
#include "nsIDOMBarProp.h"
#include "nsIDOMAbstractView.h"
#include "nsIDOMScreen.h"
#include "nsIDOMHistory.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMEvent.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMEventTarget.h"
#include "nsISidebar.h"
#include "nsIDOMPkcs11.h"
#include "nsIDOMViewCSS.h"
#include "nsISelection.h"
#include "nsIDOMCrypto.h"
#include "nsIDOMWindow.h"
#include "nsIControllers.h"
#include "nsIDOMWindowEventOwner.h"


static NS_DEFINE_IID(kIScriptObjectOwnerIID, NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kIJSScriptObjectIID, NS_IJSSCRIPTOBJECT_IID);
static NS_DEFINE_IID(kIScriptGlobalObjectIID, NS_ISCRIPTGLOBALOBJECT_IID);
static NS_DEFINE_IID(kINavigatorIID, NS_IDOMNAVIGATOR_IID);
static NS_DEFINE_IID(kIPromptIID, NS_IPROMPT_IID);
static NS_DEFINE_IID(kIElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDocumentViewIID, NS_IDOMDOCUMENTVIEW_IID);
static NS_DEFINE_IID(kICSSStyleDeclarationIID, NS_IDOMCSSSTYLEDECLARATION_IID);
static NS_DEFINE_IID(kIDocumentIID, NS_IDOMDOCUMENT_IID);
static NS_DEFINE_IID(kIBarPropIID, NS_IDOMBARPROP_IID);
static NS_DEFINE_IID(kIAbstractViewIID, NS_IDOMABSTRACTVIEW_IID);
static NS_DEFINE_IID(kIScreenIID, NS_IDOMSCREEN_IID);
static NS_DEFINE_IID(kIHistoryIID, NS_IDOMHISTORY_IID);
static NS_DEFINE_IID(kIWindowInternalIID, NS_IDOMWINDOWINTERNAL_IID);
static NS_DEFINE_IID(kIEventIID, NS_IDOMEVENT_IID);
static NS_DEFINE_IID(kIWindowCollectionIID, NS_IDOMWINDOWCOLLECTION_IID);
static NS_DEFINE_IID(kIEventListenerIID, NS_IDOMEVENTLISTENER_IID);
static NS_DEFINE_IID(kIEventTargetIID, NS_IDOMEVENTTARGET_IID);
static NS_DEFINE_IID(kISidebarIID, NS_ISIDEBAR_IID);
static NS_DEFINE_IID(kIPkcs11IID, NS_IDOMPKCS11_IID);
static NS_DEFINE_IID(kIViewCSSIID, NS_IDOMVIEWCSS_IID);
static NS_DEFINE_IID(kISelectionIID, NS_ISELECTION_IID);
static NS_DEFINE_IID(kICryptoIID, NS_IDOMCRYPTO_IID);
static NS_DEFINE_IID(kIWindowIID, NS_IDOMWINDOW_IID);
static NS_DEFINE_IID(kIControllersIID, NS_ICONTROLLERS_IID);
static NS_DEFINE_IID(kIWindowEventOwnerIID, NS_IDOMWINDOWEVENTOWNER_IID);

//
// Window property ids
//
enum Window_slots {
  WINDOW_DOCUMENT = -1,
  WINDOW_PARENT = -2,
  WINDOW_TOP = -3,
  WINDOW_SCROLLBARS = -4,
  WINDOW_FRAMES = -5,
  WINDOW_NAME = -6,
  WINDOW_SCROLLX = -7,
  WINDOW_SCROLLY = -8,
  WINDOWINTERNAL_WINDOW = -9,
  WINDOWINTERNAL_SELF = -10,
  WINDOWINTERNAL_NAVIGATOR = -11,
  WINDOWINTERNAL_SCREEN = -12,
  WINDOWINTERNAL_HISTORY = -13,
  WINDOWINTERNAL__CONTENT = -14,
  WINDOWINTERNAL_SIDEBAR = -15,
  WINDOWINTERNAL_PROMPTER = -16,
  WINDOWINTERNAL_MENUBAR = -17,
  WINDOWINTERNAL_TOOLBAR = -18,
  WINDOWINTERNAL_LOCATIONBAR = -19,
  WINDOWINTERNAL_PERSONALBAR = -20,
  WINDOWINTERNAL_STATUSBAR = -21,
  WINDOWINTERNAL_DIRECTORIES = -22,
  WINDOWINTERNAL_CLOSED = -23,
  WINDOWINTERNAL_CRYPTO = -24,
  WINDOWINTERNAL_PKCS11 = -25,
  WINDOWINTERNAL_CONTROLLERS = -26,
  WINDOWINTERNAL_OPENER = -27,
  WINDOWINTERNAL_STATUS = -28,
  WINDOWINTERNAL_DEFAULTSTATUS = -29,
  WINDOWINTERNAL_LOCATION = -30,
  WINDOWINTERNAL_TITLE = -31,
  WINDOWINTERNAL_INNERWIDTH = -32,
  WINDOWINTERNAL_INNERHEIGHT = -33,
  WINDOWINTERNAL_OUTERWIDTH = -34,
  WINDOWINTERNAL_OUTERHEIGHT = -35,
  WINDOWINTERNAL_SCREENX = -36,
  WINDOWINTERNAL_SCREENY = -37,
  WINDOWINTERNAL_PAGEXOFFSET = -38,
  WINDOWINTERNAL_PAGEYOFFSET = -39,
  WINDOWINTERNAL_LENGTH = -40,
  WINDOWEVENTOWNER_ONMOUSEDOWN = -41,
  WINDOWEVENTOWNER_ONMOUSEUP = -42,
  WINDOWEVENTOWNER_ONCLICK = -43,
  WINDOWEVENTOWNER_ONMOUSEOVER = -44,
  WINDOWEVENTOWNER_ONMOUSEOUT = -45,
  WINDOWEVENTOWNER_ONKEYDOWN = -46,
  WINDOWEVENTOWNER_ONKEYUP = -47,
  WINDOWEVENTOWNER_ONKEYPRESS = -48,
  WINDOWEVENTOWNER_ONMOUSEMOVE = -49,
  WINDOWEVENTOWNER_ONFOCUS = -50,
  WINDOWEVENTOWNER_ONBLUR = -51,
  WINDOWEVENTOWNER_ONSUBMIT = -52,
  WINDOWEVENTOWNER_ONRESET = -53,
  WINDOWEVENTOWNER_ONCHANGE = -54,
  WINDOWEVENTOWNER_ONSELECT = -55,
  WINDOWEVENTOWNER_ONLOAD = -56,
  WINDOWEVENTOWNER_ONUNLOAD = -57,
  WINDOWEVENTOWNER_ONCLOSE = -58,
  WINDOWEVENTOWNER_ONABORT = -59,
  WINDOWEVENTOWNER_ONERROR = -60,
  WINDOWEVENTOWNER_ONPAINT = -61,
  WINDOWEVENTOWNER_ONDRAGDROP = -62,
  WINDOWEVENTOWNER_ONRESIZE = -63,
  WINDOWEVENTOWNER_ONSCROLL = -64,
  ABSTRACTVIEW_DOCUMENT = -65
};

/***********************************************************************/
//
// Window Properties Getter
//
PR_STATIC_CALLBACK(JSBool)
GetWindowProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

    // If there's no private data, this must be the prototype, so ignore
    if (nsnull == a) {
      return JS_TRUE;
    }

    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case WINDOW_DOCUMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_DOCUMENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMDocument* prop;
          rv = a->GetDocument(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_PARENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_PARENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindow* prop;
          rv = a->GetParent(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_SCROLLBARS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCROLLBARS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMBarProp* prop;
          rv = a->GetScrollbars(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_FRAMES:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_FRAMES, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowCollection* prop;
          rv = a->GetFrames(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }
        }
        break;
      }
      case WINDOW_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_NAME, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          rv = a->GetName(prop);
          if (NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
          }
        }
        break;
      }
      case WINDOW_SCROLLX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCROLLX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetScrollX(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case WINDOW_SCROLLY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCROLLY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          rv = a->GetScrollY(&prop);
          if (NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
          }
        }
        break;
      }
      case WINDOWINTERNAL_WINDOW:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_WINDOW, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowInternal* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetWindow(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_SELF:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_SELF, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowInternal* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetSelf(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_NAVIGATOR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_NAVIGATOR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMNavigator* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetNavigator(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_SCREEN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_SCREEN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMScreen* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetScreen(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_HISTORY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_HISTORY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMHistory* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetHistory(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_STATUSBAR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_STATUSBAR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMBarProp* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetStatusbar(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_DIRECTORIES:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_DIRECTORIES, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMBarProp* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetDirectories(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_CLOSED:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_CLOSED, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRBool prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetClosed(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = BOOLEAN_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_CRYPTO:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_CRYPTO, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMCrypto* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetCrypto(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_PKCS11:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_PKCS11, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMPkcs11* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetPkcs11(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_OPENER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_OPENER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowInternal* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetOpener(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_STATUS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_STATUS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetStatus(prop);
            if(NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_DEFAULTSTATUS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_DEFAULTSTATUS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetDefaultStatus(prop);
            if(NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_LOCATION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_LOCATION, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetLocation(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_TITLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_TITLE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetTitle(prop);
            if(NS_SUCCEEDED(rv)) {
            nsJSUtils::nsConvertStringToJSVal(prop, cx, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_INNERWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_INNERWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetInnerWidth(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_INNERHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_INNERHEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetInnerHeight(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_OUTERWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_OUTERWIDTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetOuterWidth(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_OUTERHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_OUTERHEIGHT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetOuterHeight(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_SCREENX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_SCREENX, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetScreenX(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_SCREENY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_SCREENY, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetScreenY(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_PAGEXOFFSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_PAGEXOFFSET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetPageXOffset(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_PAGEYOFFSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_PAGEYOFFSET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetPageYOffset(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWINTERNAL_LENGTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_LENGTH, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          PRUint32 prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetLength(&prop);
            if(NS_SUCCEEDED(rv)) {
            *vp = INT_TO_JSVAL(prop);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONMOUSEDOWN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONMOUSEDOWN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnmousedown(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONMOUSEUP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONMOUSEUP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnmouseup(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONCLICK:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONCLICK, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnclick(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONMOUSEOVER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONMOUSEOVER, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnmouseover(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONMOUSEOUT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONMOUSEOUT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnmouseout(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONKEYDOWN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONKEYDOWN, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnkeydown(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONKEYUP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONKEYUP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnkeyup(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONKEYPRESS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONKEYPRESS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnkeypress(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONMOUSEMOVE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONMOUSEMOVE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnmousemove(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONFOCUS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONFOCUS, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnfocus(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONBLUR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONBLUR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnblur(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONSUBMIT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONSUBMIT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnsubmit(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONRESET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONRESET, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnreset(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONCHANGE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONCHANGE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnchange(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONSELECT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONSELECT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnselect(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONLOAD:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONLOAD, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnload(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONUNLOAD:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONUNLOAD, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnunload(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONCLOSE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONCLOSE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnclose(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONABORT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONABORT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnabort(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONERROR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONERROR, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnerror(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONPAINT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONPAINT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnpaint(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONDRAGDROP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONDRAGDROP, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOndragdrop(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONRESIZE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONRESIZE, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnresize(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case WINDOWEVENTOWNER_ONSCROLL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONSCROLL, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowEventOwner* b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            rv = b->GetOnscroll(vp);
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      case ABSTRACTVIEW_DOCUMENT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_ABSTRACTVIEW_DOCUMENT, PR_FALSE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMDocumentView* prop;
          nsIDOMAbstractView* b;
          if (NS_OK == a->QueryInterface(kIAbstractViewIID, (void **)&b)) {
            rv = b->GetDocument(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
        }
        break;
      }
      default:
      {
        JSObject* global = JS_GetGlobalObject(cx);
        if (global != obj) {
          nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
          rv = secMan->CheckScriptAccess(cx, obj,
                                         NS_DOM_PROP_WINDOW_SCRIPTGLOBALS,
                                         PR_FALSE);
        }
      }
    }
  }
  else {
    JSObject* global = JS_GetGlobalObject(cx);
    if (global != obj) {
      nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
      rv = secMan->CheckScriptAccess(cx, obj,
                                     NS_DOM_PROP_WINDOW_SCRIPTGLOBALS,
                                     PR_FALSE);
    }
  }

  if (NS_FAILED(rv))
      return nsJSUtils::nsReportError(cx, obj, rv);
  return PR_TRUE;
}

/***********************************************************************/
//
// Window Properties Setter
//
PR_STATIC_CALLBACK(JSBool)
SetWindowProperty(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{

  nsresult rv = NS_OK;
  if (JSVAL_IS_INT(id)) {
    nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

    // If there's no private data, this must be the prototype, so ignore
    if (nsnull == a) {
      return JS_TRUE;
    }

    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    switch(JSVAL_TO_INT(id)) {
      case WINDOW_NAME:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_NAME, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          rv = a->SetName(prop);
          
        }
        break;
      }
      case WINDOWINTERNAL_OPENER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_OPENER, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsIDOMWindowInternal* prop;
          if (PR_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)&prop,
                                                  kIWindowInternalIID, NS_ConvertASCIItoUCS2("WindowInternal"),
                                                  cx, *vp)) {
            rv = NS_ERROR_DOM_NOT_OBJECT_ERR;
            break;
          }
      
          nsIDOMWindowInternal *b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            b->SetOpener(prop);
            NS_RELEASE(b);
          }
          else {
             NS_IF_RELEASE(prop);
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          NS_IF_RELEASE(prop);
        }
        break;
      }
      case WINDOWINTERNAL_STATUS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_STATUS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          nsIDOMWindowInternal *b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            b->SetStatus(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWINTERNAL_DEFAULTSTATUS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_DEFAULTSTATUS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          nsIDOMWindowInternal *b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            b->SetDefaultStatus(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWINTERNAL_LOCATION:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_LOCATION, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowInternal *b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            b->SetLocation(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWINTERNAL_TITLE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_TITLE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          nsAutoString prop;
          nsJSUtils::nsConvertJSValToString(prop, cx, *vp);
      
          nsIDOMWindowInternal *b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            b->SetTitle(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWINTERNAL_INNERWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_INNERWIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          int32 temp;
          if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
            prop = (PRInt32)temp;
          }
          else {
            rv = NS_ERROR_DOM_NOT_NUMBER_ERR;
            break;
          }
      
          nsIDOMWindowInternal *b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            b->SetInnerWidth(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWINTERNAL_INNERHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_INNERHEIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          int32 temp;
          if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
            prop = (PRInt32)temp;
          }
          else {
            rv = NS_ERROR_DOM_NOT_NUMBER_ERR;
            break;
          }
      
          nsIDOMWindowInternal *b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            b->SetInnerHeight(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWINTERNAL_OUTERWIDTH:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_OUTERWIDTH, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          int32 temp;
          if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
            prop = (PRInt32)temp;
          }
          else {
            rv = NS_ERROR_DOM_NOT_NUMBER_ERR;
            break;
          }
      
          nsIDOMWindowInternal *b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            b->SetOuterWidth(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWINTERNAL_OUTERHEIGHT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_OUTERHEIGHT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          int32 temp;
          if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
            prop = (PRInt32)temp;
          }
          else {
            rv = NS_ERROR_DOM_NOT_NUMBER_ERR;
            break;
          }
      
          nsIDOMWindowInternal *b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            b->SetOuterHeight(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWINTERNAL_SCREENX:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_SCREENX, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          int32 temp;
          if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
            prop = (PRInt32)temp;
          }
          else {
            rv = NS_ERROR_DOM_NOT_NUMBER_ERR;
            break;
          }
      
          nsIDOMWindowInternal *b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            b->SetScreenX(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWINTERNAL_SCREENY:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_SCREENY, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          int32 temp;
          if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
            prop = (PRInt32)temp;
          }
          else {
            rv = NS_ERROR_DOM_NOT_NUMBER_ERR;
            break;
          }
      
          nsIDOMWindowInternal *b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            b->SetScreenY(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWINTERNAL_PAGEXOFFSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_PAGEXOFFSET, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          int32 temp;
          if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
            prop = (PRInt32)temp;
          }
          else {
            rv = NS_ERROR_DOM_NOT_NUMBER_ERR;
            break;
          }
      
          nsIDOMWindowInternal *b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            b->SetPageXOffset(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWINTERNAL_PAGEYOFFSET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_PAGEYOFFSET, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          PRInt32 prop;
          int32 temp;
          if (JSVAL_IS_NUMBER(*vp) && JS_ValueToInt32(cx, *vp, &temp)) {
            prop = (PRInt32)temp;
          }
          else {
            rv = NS_ERROR_DOM_NOT_NUMBER_ERR;
            break;
          }
      
          nsIDOMWindowInternal *b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            b->SetPageYOffset(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONMOUSEDOWN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONMOUSEDOWN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnmousedown(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONMOUSEUP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONMOUSEUP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnmouseup(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONCLICK:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONCLICK, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnclick(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONMOUSEOVER:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONMOUSEOVER, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnmouseover(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONMOUSEOUT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONMOUSEOUT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnmouseout(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONKEYDOWN:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONKEYDOWN, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnkeydown(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONKEYUP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONKEYUP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnkeyup(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONKEYPRESS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONKEYPRESS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnkeypress(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONMOUSEMOVE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONMOUSEMOVE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnmousemove(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONFOCUS:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONFOCUS, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnfocus(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONBLUR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONBLUR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnblur(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONSUBMIT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONSUBMIT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnsubmit(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONRESET:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONRESET, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnreset(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONCHANGE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONCHANGE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnchange(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONSELECT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONSELECT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnselect(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONLOAD:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONLOAD, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnload(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONUNLOAD:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONUNLOAD, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnunload(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONCLOSE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONCLOSE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnclose(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONABORT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONABORT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnabort(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONERROR:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONERROR, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnerror(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONPAINT:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONPAINT, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnpaint(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONDRAGDROP:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONDRAGDROP, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOndragdrop(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONRESIZE:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONRESIZE, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnresize(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      case WINDOWEVENTOWNER_ONSCROLL:
      {
        rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWEVENTOWNER_ONSCROLL, PR_TRUE);
        if (NS_SUCCEEDED(rv)) {
          jsval prop;
         prop = *vp;
      
          nsIDOMWindowEventOwner *b;
          if (NS_OK == a->QueryInterface(kIWindowEventOwnerIID, (void **)&b)) {
            b->SetOnscroll(prop);
            NS_RELEASE(b);
          }
          else {
             
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }
          
        }
        break;
      }
      default:
      {
        JSObject* global = JS_GetGlobalObject(cx);
        if (global != obj) {
          nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
          rv = secMan->CheckScriptAccess(cx, obj,
                                         NS_DOM_PROP_WINDOW_SCRIPTGLOBALS,
                                         PR_TRUE);
        }
      }
    }
  }
  else {
    JSObject* global = JS_GetGlobalObject(cx);
    if (global != obj) {
      nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
      rv = secMan->CheckScriptAccess(cx, obj,
                                     NS_DOM_PROP_WINDOW_SCRIPTGLOBALS,
                                     PR_TRUE);
    }
  }

  if (NS_FAILED(rv))
      return nsJSUtils::nsReportError(cx, obj, rv);
  return PR_TRUE;
}

/***********************************************************************/
//
// top Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowtopGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_TOP, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIDOMWindow* prop;
          rv = a->GetTop(&prop);
          if (NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// top Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowtopSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindow *a = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_TOP, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "top", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// _content Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternal_contentGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL__CONTENT, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIDOMWindowInternal* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->Get_content(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// _content Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternal_contentSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL__CONTENT, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "_content", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// sidebar Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalsidebarGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_SIDEBAR, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsISidebar* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetSidebar(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object; n.b., this will do a release on 'prop'
            nsJSUtils::nsConvertXPCObjectToJSVal(prop, NS_GET_IID(nsISidebar), cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// sidebar Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalsidebarSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_SIDEBAR, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "sidebar", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// prompter Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalprompterGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_PROMPTER, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIPrompt* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetPrompter(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object; n.b., this will do a release on 'prop'
            nsJSUtils::nsConvertXPCObjectToJSVal(prop, NS_GET_IID(nsIPrompt), cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// prompter Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalprompterSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_PROMPTER, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "prompter", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// menubar Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalmenubarGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_MENUBAR, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIDOMBarProp* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetMenubar(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// menubar Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalmenubarSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_MENUBAR, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "menubar", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// toolbar Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternaltoolbarGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_TOOLBAR, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIDOMBarProp* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetToolbar(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// toolbar Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternaltoolbarSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_TOOLBAR, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "toolbar", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// locationbar Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternallocationbarGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_LOCATIONBAR, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIDOMBarProp* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetLocationbar(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// locationbar Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternallocationbarSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_LOCATIONBAR, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "locationbar", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// personalbar Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalpersonalbarGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_PERSONALBAR, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIDOMBarProp* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetPersonalbar(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object
            nsJSUtils::nsConvertObjectToJSVal((nsISupports *)prop, cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// personalbar Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalpersonalbarSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_PERSONALBAR, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "personalbar", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}

/***********************************************************************/
//
// controllers Property Getter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalcontrollersGetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_CONTROLLERS, PR_FALSE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }

          nsIControllers* prop;
          nsIDOMWindowInternal* b;
          if (NS_OK == a->QueryInterface(kIWindowInternalIID, (void **)&b)) {
            rv = b->GetControllers(&prop);
            if(NS_SUCCEEDED(rv)) {
            // get the js object; n.b., this will do a release on 'prop'
            nsJSUtils::nsConvertXPCObjectToJSVal(prop, NS_GET_IID(nsIControllers), cx, obj, vp);
            }
            NS_RELEASE(b);
          }
          else {
            rv = NS_ERROR_DOM_WRONG_TYPE_ERR;
          }

  return PR_TRUE;
}

/***********************************************************************/
//
// controllers Property Setter
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalcontrollersSetter(JSContext *cx, JSObject *obj, jsval id, jsval *vp)
{
  nsIDOMWindowInternal *a = (nsIDOMWindowInternal*)nsJSUtils::nsGetNativeThis(cx, obj);

  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == a) {
    return JS_TRUE;
  }

  nsresult rv;
  nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
  if (!secMan)
      return PR_FALSE;
  rv = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_CONTROLLERS, PR_TRUE);
  if (NS_FAILED(rv)) {
    return nsJSUtils::nsReportError(cx, obj, rv);
  }


  JS_DefineProperty(cx, obj, "controllers", *vp, nsnull, nsnull, JSPROP_ENUMERATE);
  return PR_TRUE;
}


//
// Window finalizer
//
PR_STATIC_CALLBACK(void)
FinalizeWindow(JSContext *cx, JSObject *obj)
{
  nsJSUtils::nsGenericFinalize(cx, obj);
}


//
// Window enumerate
//
PR_STATIC_CALLBACK(JSBool)
EnumerateWindow(JSContext *cx, JSObject *obj)
{
  return nsJSUtils::nsGenericEnumerate(cx, obj);
}


//
// Window resolve
//
PR_STATIC_CALLBACK(JSBool)
ResolveWindow(JSContext *cx, JSObject *obj, jsval id)
{
  return nsJSUtils::nsGlobalResolve(cx, obj, id);
}


//
// Native method ScrollTo
//
PR_STATIC_CALLBACK(JSBool)
WindowScrollTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCROLLTO, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ScrollTo(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ScrollBy
//
PR_STATIC_CALLBACK(JSBool)
WindowScrollBy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCROLLBY, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ScrollBy(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method GetSelection
//
PR_STATIC_CALLBACK(JSBool)
WindowGetSelection(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  nsISelection* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_GETSELECTION, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->GetSelection(&nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    // n.b., this will release nativeRet
    nsJSUtils::nsConvertXPCObjectToJSVal(nativeRet, NS_GET_IID(nsISelection), cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method ScrollByLines
//
PR_STATIC_CALLBACK(JSBool)
WindowScrollByLines(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCROLLBYLINES, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ScrollByLines(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ScrollByPages
//
PR_STATIC_CALLBACK(JSBool)
WindowScrollByPages(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *nativeThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsresult result = NS_OK;
  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (nsnull == nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOW_SCROLLBYPAGES, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ScrollByPages(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Dump
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalDump(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsAutoString b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_DUMP, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->Dump(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Alert
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalAlert(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_ALERT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Alert(cx, argv+0, argc-0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Confirm
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalConfirm(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRBool nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_CONFIRM, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Confirm(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = BOOLEAN_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method Prompt
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalPrompt(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  jsval nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_PROMPT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Prompt(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = nativeRet;
  }

  return JS_TRUE;
}


//
// Native method Focus
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalFocus(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_FOCUS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Focus();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Blur
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalBlur(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_BLUR, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Blur();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Back
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalBack(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_BACK, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Back();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Forward
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalForward(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_FORWARD, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Forward();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Home
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalHome(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_HOME, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Home();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Stop
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalStop(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_STOP, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Stop();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Print
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalPrint(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_PRINT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Print();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method MoveTo
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalMoveTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_MOVETO, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->MoveTo(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method MoveBy
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalMoveBy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_MOVEBY, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->MoveBy(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ResizeTo
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalResizeTo(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_RESIZETO, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ResizeTo(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ResizeBy
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalResizeBy(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_RESIZEBY, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ResizeBy(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SizeToContent
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalSizeToContent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_SIZETOCONTENT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->SizeToContent();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method GetAttention
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalGetAttention(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_GETATTENTION, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->GetAttention();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Scroll
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalScroll(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt32 b0;
  PRInt32 b1;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_SCROLL, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }
    if (!JS_ValueToInt32(cx, argv[1], (int32 *)&b1)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->Scroll(b0, b1);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ClearTimeout
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalClearTimeout(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_CLEARTIMEOUT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ClearTimeout(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ClearInterval
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalClearInterval(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_CLEARINTERVAL, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ClearInterval(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SetTimeout
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalSetTimeout(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt32 nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_SETTIMEOUT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->SetTimeout(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method SetInterval
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalSetInterval(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt32 nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_SETINTERVAL, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->SetInterval(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = INT_TO_JSVAL(nativeRet);
  }

  return JS_TRUE;
}


//
// Native method CaptureEvents
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalCaptureEvents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_CAPTUREEVENTS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->CaptureEvents(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method ReleaseEvents
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalReleaseEvents(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  PRInt32 b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_RELEASEEVENTS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (!JS_ValueToInt32(cx, argv[0], (int32 *)&b0)) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_NUMBER_ERR);
    }

    result = nativeThis->ReleaseEvents(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method RouteEvent
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalRouteEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsCOMPtr<nsIDOMEvent> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_ROUTEEVENT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIEventIID,
                                           NS_ConvertASCIItoUCS2("Event"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->RouteEvent(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method EnableExternalCapture
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalEnableExternalCapture(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_ENABLEEXTERNALCAPTURE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->EnableExternalCapture();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method DisableExternalCapture
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalDisableExternalCapture(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_DISABLEEXTERNALCAPTURE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->DisableExternalCapture();
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method SetCursor
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalSetCursor(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsAutoString b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_SETCURSOR, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->SetCursor(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Open
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalOpen(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsIDOMWindowInternal* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_OPEN, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Open(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method OpenDialog
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalOpenDialog(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsIDOMWindowInternal* nativeRet;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_OPENDIALOG, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->OpenDialog(cx, argv+0, argc-0, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


//
// Native method Close
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalClose(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_CLOSE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    result = nativeThis->Close(cx, argv+0, argc-0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method UpdateCommands
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalUpdateCommands(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsAutoString b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_UPDATECOMMANDS, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->UpdateCommands(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method Escape
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalEscape(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsAutoString nativeRet;
  nsAutoString b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_ESCAPE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->Escape(b0, nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method Unescape
//
PR_STATIC_CALLBACK(JSBool)
WindowInternalUnescape(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMWindowInternal> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIWindowInternalIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsAutoString nativeRet;
  nsAutoString b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_WINDOWINTERNAL_UNESCAPE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);

    result = nativeThis->Unescape(b0, nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertStringToJSVal(nativeRet, cx, rval);
  }

  return JS_TRUE;
}


//
// Native method AddEventListener
//
PR_STATIC_CALLBACK(JSBool)
EventTargetAddEventListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMEventTarget> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIEventTargetIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsAutoString b0;
  nsCOMPtr<nsIDOMEventListener> b1;
  PRBool b2;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENTTARGET_ADDEVENTLISTENER, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 3) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    if (!nsJSUtils::nsConvertJSValToFunc(getter_AddRefs(b1),
                                         cx,
                                         obj,
                                         argv[1])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_FUNCTION_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b2, cx, argv[2])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }

    result = nativeThis->AddEventListener(b0, b1, b2);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method RemoveEventListener
//
PR_STATIC_CALLBACK(JSBool)
EventTargetRemoveEventListener(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMEventTarget> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIEventTargetIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsAutoString b0;
  nsCOMPtr<nsIDOMEventListener> b1;
  PRBool b2;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENTTARGET_REMOVEEVENTLISTENER, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 3) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    nsJSUtils::nsConvertJSValToString(b0, cx, argv[0]);
    if (!nsJSUtils::nsConvertJSValToFunc(getter_AddRefs(b1),
                                         cx,
                                         obj,
                                         argv[1])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_FUNCTION_ERR);
    }
    if (!nsJSUtils::nsConvertJSValToBool(&b2, cx, argv[2])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_BOOLEAN_ERR);
    }

    result = nativeThis->RemoveEventListener(b0, b1, b2);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method DispatchEvent
//
PR_STATIC_CALLBACK(JSBool)
EventTargetDispatchEvent(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMEventTarget> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIEventTargetIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsCOMPtr<nsIDOMEvent> b0;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_EVENTTARGET_DISPATCHEVENT, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 1) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIEventIID,
                                           NS_ConvertASCIItoUCS2("Event"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }

    result = nativeThis->DispatchEvent(b0);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    *rval = JSVAL_VOID;
  }

  return JS_TRUE;
}


//
// Native method GetComputedStyle
//
PR_STATIC_CALLBACK(JSBool)
ViewCSSGetComputedStyle(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  nsIDOMWindow *privateThis = (nsIDOMWindow*)nsJSUtils::nsGetNativeThis(cx, obj);
  nsCOMPtr<nsIDOMViewCSS> nativeThis;
  nsresult result = NS_OK;
  if (NS_OK != privateThis->QueryInterface(kIViewCSSIID, getter_AddRefs(nativeThis))) {
    return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_WRONG_TYPE_ERR);
  }

  nsIDOMCSSStyleDeclaration* nativeRet;
  nsCOMPtr<nsIDOMElement> b0;
  nsAutoString b1;
  // If there's no private data, this must be the prototype, so ignore
  if (!nativeThis) {
    return JS_TRUE;
  }

  {
    *rval = JSVAL_NULL;
    nsIScriptSecurityManager *secMan = nsJSUtils::nsGetSecurityManager(cx, obj);
    if (!secMan)
        return PR_FALSE;
    result = secMan->CheckScriptAccess(cx, obj, NS_DOM_PROP_VIEWCSS_GETCOMPUTEDSTYLE, PR_FALSE);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }
    if (argc < 2) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR);
    }

    if (JS_FALSE == nsJSUtils::nsConvertJSValToObject((nsISupports **)(void**)getter_AddRefs(b0),
                                           kIElementIID,
                                           NS_ConvertASCIItoUCS2("Element"),
                                           cx,
                                           argv[0])) {
      return nsJSUtils::nsReportError(cx, obj, NS_ERROR_DOM_NOT_OBJECT_ERR);
    }
    nsJSUtils::nsConvertJSValToString(b1, cx, argv[1]);

    result = nativeThis->GetComputedStyle(b0, b1, &nativeRet);
    if (NS_FAILED(result)) {
      return nsJSUtils::nsReportError(cx, obj, result);
    }

    nsJSUtils::nsConvertObjectToJSVal(nativeRet, cx, obj, rval);
  }

  return JS_TRUE;
}


/***********************************************************************/
//
// class for Window
//
JSClass WindowClass = {
  "Window", 
  JSCLASS_HAS_PRIVATE | JSCLASS_PRIVATE_IS_NSISUPPORTS,
  JS_PropertyStub,
  JS_PropertyStub,
  GetWindowProperty,
  SetWindowProperty,
  EnumerateWindow,
  ResolveWindow,
  JS_ConvertStub,
  FinalizeWindow,
  nsnull,
  nsJSUtils::nsCheckAccess
};


//
// Window class properties
//
static JSPropertySpec WindowProperties[] =
{
  {"document",    WINDOW_DOCUMENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"parent",    WINDOW_PARENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"top",    WINDOW_TOP,    JSPROP_ENUMERATE, WindowtopGetter, WindowtopSetter},
  {"scrollbars",    WINDOW_SCROLLBARS,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"frames",    WINDOW_FRAMES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"name",    WINDOW_NAME,    JSPROP_ENUMERATE},
  {"scrollX",    WINDOW_SCROLLX,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"scrollY",    WINDOW_SCROLLY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"window",    WINDOWINTERNAL_WINDOW,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"self",    WINDOWINTERNAL_SELF,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"navigator",    WINDOWINTERNAL_NAVIGATOR,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"screen",    WINDOWINTERNAL_SCREEN,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"history",    WINDOWINTERNAL_HISTORY,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"_content",    WINDOWINTERNAL__CONTENT,    JSPROP_ENUMERATE, WindowInternal_contentGetter, WindowInternal_contentSetter},
  {"sidebar",    WINDOWINTERNAL_SIDEBAR,    JSPROP_ENUMERATE, WindowInternalsidebarGetter, WindowInternalsidebarSetter},
  {"prompter",    WINDOWINTERNAL_PROMPTER,    JSPROP_ENUMERATE, WindowInternalprompterGetter, WindowInternalprompterSetter},
  {"menubar",    WINDOWINTERNAL_MENUBAR,    JSPROP_ENUMERATE, WindowInternalmenubarGetter, WindowInternalmenubarSetter},
  {"toolbar",    WINDOWINTERNAL_TOOLBAR,    JSPROP_ENUMERATE, WindowInternaltoolbarGetter, WindowInternaltoolbarSetter},
  {"locationbar",    WINDOWINTERNAL_LOCATIONBAR,    JSPROP_ENUMERATE, WindowInternallocationbarGetter, WindowInternallocationbarSetter},
  {"personalbar",    WINDOWINTERNAL_PERSONALBAR,    JSPROP_ENUMERATE, WindowInternalpersonalbarGetter, WindowInternalpersonalbarSetter},
  {"statusbar",    WINDOWINTERNAL_STATUSBAR,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"directories",    WINDOWINTERNAL_DIRECTORIES,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"closed",    WINDOWINTERNAL_CLOSED,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"crypto",    WINDOWINTERNAL_CRYPTO,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"pkcs11",    WINDOWINTERNAL_PKCS11,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"controllers",    WINDOWINTERNAL_CONTROLLERS,    JSPROP_ENUMERATE, WindowInternalcontrollersGetter, WindowInternalcontrollersSetter},
  {"opener",    WINDOWINTERNAL_OPENER,    JSPROP_ENUMERATE},
  {"status",    WINDOWINTERNAL_STATUS,    JSPROP_ENUMERATE},
  {"defaultStatus",    WINDOWINTERNAL_DEFAULTSTATUS,    JSPROP_ENUMERATE},
  {"location",    WINDOWINTERNAL_LOCATION,    JSPROP_ENUMERATE},
  {"title",    WINDOWINTERNAL_TITLE,    JSPROP_ENUMERATE},
  {"innerWidth",    WINDOWINTERNAL_INNERWIDTH,    JSPROP_ENUMERATE},
  {"innerHeight",    WINDOWINTERNAL_INNERHEIGHT,    JSPROP_ENUMERATE},
  {"outerWidth",    WINDOWINTERNAL_OUTERWIDTH,    JSPROP_ENUMERATE},
  {"outerHeight",    WINDOWINTERNAL_OUTERHEIGHT,    JSPROP_ENUMERATE},
  {"screenX",    WINDOWINTERNAL_SCREENX,    JSPROP_ENUMERATE},
  {"screenY",    WINDOWINTERNAL_SCREENY,    JSPROP_ENUMERATE},
  {"pageXOffset",    WINDOWINTERNAL_PAGEXOFFSET,    JSPROP_ENUMERATE},
  {"pageYOffset",    WINDOWINTERNAL_PAGEYOFFSET,    JSPROP_ENUMERATE},
  {"length",    WINDOWINTERNAL_LENGTH,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {"onmousedown",    WINDOWEVENTOWNER_ONMOUSEDOWN,    JSPROP_ENUMERATE},
  {"onmouseup",    WINDOWEVENTOWNER_ONMOUSEUP,    JSPROP_ENUMERATE},
  {"onclick",    WINDOWEVENTOWNER_ONCLICK,    JSPROP_ENUMERATE},
  {"onmouseover",    WINDOWEVENTOWNER_ONMOUSEOVER,    JSPROP_ENUMERATE},
  {"onmouseout",    WINDOWEVENTOWNER_ONMOUSEOUT,    JSPROP_ENUMERATE},
  {"onkeydown",    WINDOWEVENTOWNER_ONKEYDOWN,    JSPROP_ENUMERATE},
  {"onkeyup",    WINDOWEVENTOWNER_ONKEYUP,    JSPROP_ENUMERATE},
  {"onkeypress",    WINDOWEVENTOWNER_ONKEYPRESS,    JSPROP_ENUMERATE},
  {"onmousemove",    WINDOWEVENTOWNER_ONMOUSEMOVE,    JSPROP_ENUMERATE},
  {"onfocus",    WINDOWEVENTOWNER_ONFOCUS,    JSPROP_ENUMERATE},
  {"onblur",    WINDOWEVENTOWNER_ONBLUR,    JSPROP_ENUMERATE},
  {"onsubmit",    WINDOWEVENTOWNER_ONSUBMIT,    JSPROP_ENUMERATE},
  {"onreset",    WINDOWEVENTOWNER_ONRESET,    JSPROP_ENUMERATE},
  {"onchange",    WINDOWEVENTOWNER_ONCHANGE,    JSPROP_ENUMERATE},
  {"onselect",    WINDOWEVENTOWNER_ONSELECT,    JSPROP_ENUMERATE},
  {"onload",    WINDOWEVENTOWNER_ONLOAD,    JSPROP_ENUMERATE},
  {"onunload",    WINDOWEVENTOWNER_ONUNLOAD,    JSPROP_ENUMERATE},
  {"onclose",    WINDOWEVENTOWNER_ONCLOSE,    JSPROP_ENUMERATE},
  {"onabort",    WINDOWEVENTOWNER_ONABORT,    JSPROP_ENUMERATE},
  {"onerror",    WINDOWEVENTOWNER_ONERROR,    JSPROP_ENUMERATE},
  {"onpaint",    WINDOWEVENTOWNER_ONPAINT,    JSPROP_ENUMERATE},
  {"ondragdrop",    WINDOWEVENTOWNER_ONDRAGDROP,    JSPROP_ENUMERATE},
  {"onresize",    WINDOWEVENTOWNER_ONRESIZE,    JSPROP_ENUMERATE},
  {"onscroll",    WINDOWEVENTOWNER_ONSCROLL,    JSPROP_ENUMERATE},
  {"document",    ABSTRACTVIEW_DOCUMENT,    JSPROP_ENUMERATE | JSPROP_READONLY},
  {0}
};


//
// Window class methods
//
static JSFunctionSpec WindowMethods[] = 
{
  {"scrollTo",          WindowScrollTo,     2},
  {"scrollBy",          WindowScrollBy,     2},
  {"getSelection",          WindowGetSelection,     0},
  {"scrollByLines",          WindowScrollByLines,     1},
  {"scrollByPages",          WindowScrollByPages,     1},
  {"dump",          WindowInternalDump,     1},
  {"alert",          WindowInternalAlert,     0},
  {"confirm",          WindowInternalConfirm,     0},
  {"prompt",          WindowInternalPrompt,     0},
  {"focus",          WindowInternalFocus,     0},
  {"blur",          WindowInternalBlur,     0},
  {"back",          WindowInternalBack,     0},
  {"forward",          WindowInternalForward,     0},
  {"home",          WindowInternalHome,     0},
  {"stop",          WindowInternalStop,     0},
  {"print",          WindowInternalPrint,     0},
  {"moveTo",          WindowInternalMoveTo,     2},
  {"moveBy",          WindowInternalMoveBy,     2},
  {"resizeTo",          WindowInternalResizeTo,     2},
  {"resizeBy",          WindowInternalResizeBy,     2},
  {"sizeToContent",          WindowInternalSizeToContent,     0},
  {"GetAttention",          WindowInternalGetAttention,     0},
  {"scroll",          WindowInternalScroll,     2},
  {"clearTimeout",          WindowInternalClearTimeout,     1},
  {"clearInterval",          WindowInternalClearInterval,     1},
  {"setTimeout",          WindowInternalSetTimeout,     0},
  {"setInterval",          WindowInternalSetInterval,     0},
  {"captureEvents",          WindowInternalCaptureEvents,     1},
  {"releaseEvents",          WindowInternalReleaseEvents,     1},
  {"routeEvent",          WindowInternalRouteEvent,     1},
  {"enableExternalCapture",          WindowInternalEnableExternalCapture,     0},
  {"disableExternalCapture",          WindowInternalDisableExternalCapture,     0},
  {"setCursor",          WindowInternalSetCursor,     1},
  {"open",          WindowInternalOpen,     0},
  {"openDialog",          WindowInternalOpenDialog,     0},
  {"close",          WindowInternalClose,     0},
  {"updateCommands",          WindowInternalUpdateCommands,     1},
  {"escape",          WindowInternalEscape,     1},
  {"unescape",          WindowInternalUnescape,     1},
  {"addEventListener",          EventTargetAddEventListener,     3},
  {"removeEventListener",          EventTargetRemoveEventListener,     3},
  {"dispatchEvent",          EventTargetDispatchEvent,     1},
  {"getComputedStyle",          ViewCSSGetComputedStyle,     2},
  {0}
};


//
// Window class initialization
//
nsresult NS_InitWindowClass(nsIScriptContext *aContext, 
                        nsIScriptGlobalObject *aGlobal)
{
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();
  JSObject *global = JS_GetGlobalObject(jscontext);

  JS_DefineProperties(jscontext, global, WindowProperties);
  JS_DefineFunctions(jscontext, global, WindowMethods);

  return NS_OK;
}


//
// Method for creating a new Window JavaScript object
//
extern "C" NS_DOM nsresult NS_NewScriptWindow(nsIScriptContext *aContext, nsISupports *aSupports, nsISupports *aParent, void **aReturn)
{
  NS_PRECONDITION(nsnull != aContext && nsnull != aSupports && nsnull != aReturn, "null arg");
  JSContext *jscontext = (JSContext *)aContext->GetNativeContext();

  JSObject *global = ::JS_NewObject(jscontext, &WindowClass, NULL, NULL);
  if (global) {
    // The global object has a to be defined in two step:
    // 1- create a generic object, with no prototype and no parent which
    //    will be passed to JS_InitStandardClasses. JS_InitStandardClasses 
    //    will make it the global object
    // 2- define the global object to be what you really want it to be.
    //
    // The js runtime is not fully initialized before JS_InitStandardClasses
    // is called, so part of the global object initialization has to be moved 
    // after JS_InitStandardClasses

    // assign "this" to the js object
    ::JS_SetPrivate(jscontext, global, aSupports);
    NS_ADDREF(aSupports);

    JS_DefineProperties(jscontext, global, WindowProperties);
    JS_DefineFunctions(jscontext, global, WindowMethods);

    *aReturn = (void*)global;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}
