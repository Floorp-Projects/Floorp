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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#include "mozilla-config.h"

#include <windows.h>
#include <windowsx.h>

#undef GetNextSibling           // is defined in VC98/Include/Windowsx.

#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include <nsXPCOM.h>
#include <nsIXPConnect.h>
#include <nsIServiceManager.h>
#include "xpcprivate.h"

#include "epimetheus.h"
#include "plugin.h"
#include "nsIServiceManager.h"
#include "nsISupportsUtils.h" // some usefule macros are defined here
#include "nsGlobalWindow.h"

//////////////////////////////////////
//
// general initialization and shutdown
//
NPError NS_PluginInitialize()
{
    FILE *log = fopen("pandora_log.txt", "a");
    fprintf(log, "NS_PluginInitialize\n");
    fclose(log);
  return NPERR_NO_ERROR;
}

void NS_PluginShutdown()
{
}

//NP_Initialize

/////////////////////////////////////////////////////////////
//
// construction and destruction of our plugin instance object
//
nsPluginInstanceBase * NS_NewPluginInstance(nsPluginCreateData * aCreateDataStruct)
{
    FILE *log = fopen("pandora_log.txt", "a");
    fprintf(log, "NS_NewPluginInstance\n");
    if(!aCreateDataStruct) {
        fclose(log);
        return NULL;
    }

    nsPluginInstance * plugin = new nsPluginInstance(aCreateDataStruct->instance);
    fclose(log);
    return plugin;
}

void NS_DestroyPluginInstance(nsPluginInstanceBase * aPlugin)
{
  if(aPlugin)
    delete (nsPluginInstance *)aPlugin;
}

////////////////////////////////////////
//
// nsPluginInstance class implementation
//
nsPluginInstance::nsPluginInstance(NPP aInstance) : nsPluginInstanceBase(),
  mInstance(aInstance),
  mInitialized(FALSE),
  mScriptablePeer(NULL)
{
    FILE *log = fopen("pandora_log.txt", "a");
    fprintf(log, "nsPluginInstance constructor\n");
    fclose(log);
  mhWnd = NULL;
  mString[0] = '\0';
}

nsPluginInstance::~nsPluginInstance()
{
  // mScriptablePeer may be also held by the browser 
  // so releasing it here does not guarantee that it is over
  // we should take precaution in case it will be called later
  // and zero its mPlugin member
    if (mScriptablePeer) {
      mScriptablePeer->SetInstance(NULL);
      NS_IF_RELEASE(mScriptablePeer);
    }
}

static LRESULT CALLBACK PluginWinProc(HWND, UINT, WPARAM, LPARAM);
static WNDPROC lpOldProc = NULL;


using namespace JavaScript;
using namespace MetaData;

void report(Exception x)
{
    String s = x.fullMessage();
	std::string str(s.length(), char());
	std::transform(s.begin(), s.end(), str.begin(), narrow);
    printf(str.c_str());
}

NPBool nsPluginInstance::init(NPWindow* aWindow)
{
    FILE *log = fopen("pandora_log.txt", "a");
  if(aWindow == NULL)
    return FALSE;

  mhWnd = (HWND)aWindow->window;
  if(mhWnd == NULL)
    return FALSE;

  // subclass window so we can intercept window messages and
  // do our drawing to it
  lpOldProc = SubclassWindow(mhWnd, (WNDPROC)PluginWinProc);

  // associate window with our nsPluginInstance object so we can access 
  // it in the window procedure
  SetWindowLong(mhWnd, GWL_USERDATA, (LONG)this);


  fprintf(log, "Pandora: starting up\n");
  world = new World();
  metadata = new JavaScript::MetaData::JS2Metadata(*world);
  spiderMonkeyClass = new (metadata) JS2SpiderMonkeyClass(world->identifiers[widenCString("SpiderMonkey")]);
  fprintf(log, "Pandora: done starting up\n");

  fclose(log);
 
  mInitialized = TRUE;

  return TRUE;
}

void nsPluginInstance::shut()
{
  // subclass it back
  SubclassWindow(mhWnd, lpOldProc);
  mhWnd = NULL;
  mInitialized = FALSE;

  try {
      printf("Pandora: shutting down\n");
      if (metadata) {
        delete metadata;
        metadata = NULL;
      }
      if (world) {
        world->identifiers.clear();
        delete world;
        world = NULL;
      }
      printf("Pandora: done shutting down\n");
  }
  catch (Exception x) {
    report(x);
  }
}

NPBool nsPluginInstance::isInitialized()
{
  return mInitialized;
}


void nsPluginInstance::invoke(const char *target, char **_retval)
{
    js2val rval = metadata->invokeFunction(target);
    const JavaScript::String *valstr = metadata->toString(rval);
    std::string str(valstr->length(), char());
    std::transform(valstr->begin(), valstr->end(), str.begin(), narrow);

    *_retval = (char *)NPN_MemAlloc(strlen(str.c_str()) + 1);
    strcpy(*_retval, str.c_str());
}




void nsPluginInstance::GetDocument(nsISupports * *aDocument)
{
    *aDocument = doc;
}

void nsPluginInstance::SetDocument(nsISupports *aDocument)
{
    printf("Pandora: setting document\n");

    NS_ADDREF(aDocument);
    doc = aDocument;

    // stash the wrapped JSObject from the document as
    // a SpiderMonkeyInstance named 'document' in the (JS2) global object

    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
    if (xpc) {
        nsIXPCNativeCallContext *aCurrentNativeCallContext;
        if (NS_SUCCEEDED(xpc->GetCurrentNativeCallContext(&aCurrentNativeCallContext))) {
            JSContext *aJSContext;
            if (NS_SUCCEEDED(aCurrentNativeCallContext->GetJSContext(&aJSContext))) {
                nsIXPConnectJSObjectHolder *wrappedDoc;
                if (NS_SUCCEEDED(xpc->WrapNative(aJSContext, aJSContext->globalObject, doc, nsIDOMDocument::GetIID(), &wrappedDoc))) {
                    JSObject *obj;
                    if (NS_SUCCEEDED(wrappedDoc->GetJSObject(&obj))) {
                        SpiderMonkeyInstance *smInst = new (metadata) SpiderMonkeyInstance(metadata, metadata->objectClass->prototype, spiderMonkeyClass);
                        smInst->jsObject = obj;
                        smInst->pluginInstance = this;
                        metadata->createDynamicProperty(metadata->glob, "document", OBJECT_TO_JS2VAL(smInst), ReadWriteAccess, true, false);
                        printf("Pandora: done setting document\n");
                    }
                }
                NS_RELEASE(aCurrentNativeCallContext);
            }
        }
    }
}




NPError nsPluginInstance::NewStream(NPMIMEType type, NPStream* stream, 
                            NPBool seekable, uint16* stype)

{
    *stype = NP_ASFILEONLY;
    return NPERR_NO_ERROR; 
}

void nsPluginInstance::StreamAsFile(NPStream* stream, const char* fname)
{
    printf("Pandora: compiling source\n");
    // compile the file, error reports need to go to the JS console
    // if succesful, script level functions get proxies generated
    try {
        metadata->readEvalFile(fname);    
        for (LocalBindingIterator bi = metadata->glob->localBindings.begin(), 
				    bend = metadata->glob->localBindings.end(); (bi != bend); bi++) {
            LocalBindingEntry *lbe = *bi;
            for (LocalBindingEntry::NS_Iterator i = lbe->begin(), end = lbe->end(); (i != end); i++) {
                LocalBindingEntry::NamespaceBinding ns = *i;
                LocalMember *m = checked_cast<LocalMember *>(ns.second->content);
                switch (m->memberKind) {
                case Member::FrameVariableMember:
                    {
                        FrameVariable *fv = checked_cast<FrameVariable *>(m);
					    js2val v = (*metadata->glob->frameSlots)[fv->frameSlot];
					    if (JS2VAL_IS_OBJECT(v) 
							    && (JS2VAL_TO_OBJECT(v)->kind == SimpleInstanceKind)
							    && (checked_cast<SimpleInstance *>(JS2VAL_TO_OBJECT(v))->type == metadata->functionClass)
							    ) {
						    FunctionInstance *fnInst = checked_cast<FunctionInstance *>(JS2VAL_TO_OBJECT(v));

						    String fnName = lbe->name;
                    
						    std::string fstr(fnName.length(), char());
						    std::transform(fnName.begin(), fnName.end(), fstr.begin(), narrow);

                            printf("Pandora: adding wrapper for \"%s\"\n", fstr.c_str());
                
						    StringFormatter sf;
						    printFormat(sf, "JavaScript:function %s() { return document.js2.invoke(\"%s\"); }", fstr.c_str(), fstr.c_str());
						    std::string str(sf.getString().length(), char());
						    std::transform(sf.getString().begin(), sf.getString().end(), str.begin(), narrow);
						    NPN_GetURL(mInstance, str.c_str(), NULL);
					    }
					    
                    }
                    break;
			    }

            }		    
        }
        printf("Pandora: done compiling source\n");
    }
    catch (Exception x) {
        report(x);
    }
}


// ==============================
// ! Scriptability related code !
// ==============================
//
// here the plugin is asked by Mozilla to tell if it is scriptable
// we should return a valid interface id and a pointer to 
// nsScriptablePeer interface which we should have implemented
// and which should be defined in the corressponding *.xpt file
// in the bin/components folder
NPError	nsPluginInstance::GetValue(NPPVariable aVariable, void *aValue)
{
  NPError rv = NPERR_NO_ERROR;

  if (aVariable == NPPVpluginScriptableInstance) {
    // addref happens in getter, so we don't addref here
    nsIPandora * scriptablePeer = getScriptablePeer();
    if (scriptablePeer) {
      *(nsISupports **)aValue = scriptablePeer;
    } else
      rv = NPERR_OUT_OF_MEMORY_ERROR;
  }
  else if (aVariable == NPPVpluginScriptableIID) {
    static nsIID scriptableIID = NS_IPANDORA_IID;
    nsIID* ptr = (nsIID *)NPN_MemAlloc(sizeof(nsIID));
    if (ptr) {
        *ptr = scriptableIID;
        *(nsIID **)aValue = ptr;
    } else
      rv = NPERR_OUT_OF_MEMORY_ERROR;
  }

  return rv;
}

// ==============================
// ! Scriptability related code !
// ==============================
//
// this method will return the scriptable object (and create it if necessary)
nsScriptablePeer* nsPluginInstance::getScriptablePeer()
{
  if (!mScriptablePeer) {
    mScriptablePeer = new nsScriptablePeer(this);
    if(!mScriptablePeer)
      return NULL;

    NS_ADDREF(mScriptablePeer);
  }

  // add reference for the caller requesting the object
  NS_ADDREF(mScriptablePeer);
  return mScriptablePeer;
}

static LRESULT CALLBACK PluginWinProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg) {
    case WM_PAINT:
      {
/*
        // draw a frame and display the string
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        FrameRect(hdc, &rc, GetStockBrush(BLACK_BRUSH));

        // get our plugin instance object and ask it for the version string
        nsPluginInstance *plugin = (nsPluginInstance *)GetWindowLong(hWnd, GWL_USERDATA);
        if (plugin)
          DrawText(hdc, plugin->mString, strlen(plugin->mString), &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        else {
          char string[] = "Error occured";
          DrawText(hdc, string, strlen(string), &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        }

        EndPaint(hWnd, &ps);
*/
      }
      break;
    default:
      break;
  }

  return DefWindowProc(hWnd, msg, wParam, lParam);
}

using namespace JavaScript;
using namespace MetaData;

js2val callJSFunction(JS2Metadata *meta, FunctionInstance *fnInst, const js2val thisValue, js2val *argv, uint32 argc)
{
    printf("JS2SpiderMonkeyClass: callJSFunction \n");
    
    SpiderMonkeyFunction *smFun = checked_cast<SpiderMonkeyFunction *>(fnInst);
    nsPluginInstance *plug = smFun->pluginInstance;

    jsval *outgoingArgs = NULL;
    js2val result = JS2VAL_VOID;
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
    if (xpc) {
        nsIXPCNativeCallContext *aCurrentNativeCallContext;
        if (NS_SUCCEEDED(xpc->GetCurrentNativeCallContext(&aCurrentNativeCallContext))) {
            JSContext *aJSContext;
            if (NS_SUCCEEDED(aCurrentNativeCallContext->GetJSContext(&aJSContext))) {
                if (argc) {
                    outgoingArgs = new jsval[argc];
                    for (uint32 i = 0; i < argc; i++) {
                        if (!plug->convertJS2ValueToJSValue(aJSContext, argv[i], &outgoingArgs[i]))
                            goto out;
                    }
                }
                jsval resultVal;
                jsval thisVal;
                if (!plug->convertJS2ValueToJSValue(aJSContext, thisValue, &thisVal))
                    goto out;
                ASSERT(JSVAL_IS_OBJECT(thisVal));
                if (JS_CallFunctionValue(aJSContext, JSVAL_TO_OBJECT(thisVal), OBJECT_TO_JSVAL(smFun->jsObject), argc, outgoingArgs, &resultVal))    
                    plug->convertJSValueToJS2Value(aJSContext, resultVal, &result);
                NS_RELEASE(aCurrentNativeCallContext);
            }
        }
    }
out:
    if (outgoingArgs)
        delete [] outgoingArgs;
    return result;
}

bool nsPluginInstance::convertJSValueToJS2Value(JSContext *cx, jsval v, js2val *rval)
{
    bool result = true;
    if (JSVAL_IS_INT(v))
        *rval = INT_TO_JS2VAL(JSVAL_TO_INT(v));
    else
    if (JSVAL_IS_NUMBER(v))
        *rval = metadata->engine->allocNumber(*JSVAL_TO_DOUBLE(v));
    else
    if (JSVAL_IS_STRING(v))
        *rval = metadata->engine->allocString(JS_GetStringChars(JSVAL_TO_STRING(v)), JS_GetStringLength(JSVAL_TO_STRING(v)));
    else
    if (JSVAL_IS_BOOLEAN(v))
        *rval = BOOLEAN_TO_JS2VAL(JSVAL_TO_BOOLEAN(v));
    else
    if (JSVAL_IS_NULL(v))
        *rval = JS2VAL_NULL;
    else
    if (JSVAL_IS_VOID(v))
        *rval = JS2VAL_VOID;
    else
    if (JSVAL_IS_OBJECT(v)) {
        JSObject *jsObj = JSVAL_TO_OBJECT(v);
        if (JS_ObjectIsFunction(cx, jsObj)) {
            SpiderMonkeyFunction *smFun = new (metadata) SpiderMonkeyFunction(metadata);
            JS2Object *rooter = smFun;
            DEFINE_ROOTKEEPER(metadata, rk, rooter);
            smFun->fWrap = new FunctionWrapper(metadata, true, new (metadata) ParameterFrame(JS2VAL_VOID, true), callJSFunction, metadata->env);  
            smFun->fWrap->length = 0;
            smFun->jsObject = jsObj;
            smFun->pluginInstance = this;
            *rval = OBJECT_TO_JS2VAL(smFun);
        }
        else {
            SpiderMonkeyInstance *smInst = new (metadata) SpiderMonkeyInstance(metadata, metadata->objectClass->prototype, spiderMonkeyClass);
            smInst->jsObject = jsObj;
            smInst->pluginInstance = this;
            *rval = OBJECT_TO_JS2VAL(smInst);
        }
    }
    else
        result = false;
    return result;
}

bool nsPluginInstance::convertJS2ValueToJSValue(JSContext *cx, js2val v, jsval *rval)
{
    bool result = true;
    if (JS2VAL_IS_INT(v))
        *rval = INT_TO_JSVAL(JS2VAL_TO_INT(v));
    else
    if (JS2VAL_IS_NUMBER(v))
        *rval = DOUBLE_TO_JSVAL(JS_NewDouble(cx, *JS2VAL_TO_DOUBLE(v)));
    else
    if (JS2VAL_IS_STRING(v)) {
        String *str = JS2VAL_TO_STRING(v);
        *rval = STRING_TO_JSVAL(JS_NewUCStringCopyN(cx, str->data(), str->length()));
    }
    else
    if (JS2VAL_IS_BOOLEAN(v))
        *rval = BOOLEAN_TO_JSVAL(JS2VAL_TO_BOOLEAN(v));
    else
    if (JS2VAL_IS_NULL(v))
        *rval = JSVAL_NULL;
    else
    if (JS2VAL_IS_VOID(v))
        *rval = JSVAL_VOID;
    else
    if (JS2VAL_IS_OBJECT(v)) {
        JS2Object *js2Obj = JS2VAL_TO_OBJECT(v);
        ASSERT(js2Obj->kind == SimpleInstanceKind);
        ASSERT(checked_cast<SimpleInstance *>(js2Obj)->type == spiderMonkeyClass);
        SpiderMonkeyInstance *smInst = checked_cast<SpiderMonkeyInstance *>(js2Obj);
        *rval = OBJECT_TO_JSVAL(smInst->jsObject);
    }
    else
        result = false;
    return result;
}

bool JS2SpiderMonkeyClass::Read(JS2Metadata *meta, js2val *base, Multiname *multiname, Environment *env, Phase phase, js2val *rval)
{   
    std::string str(multiname->name.length(), char());
    std::transform(multiname->name.begin(), multiname->name.end(), str.begin(), narrow);

    printf("JS2SpiderMonkeyClass: Reading property \"%s\"\n", str.c_str());
    

    ASSERT(JS2VAL_IS_OBJECT(*base) && !JS2VAL_IS_NULL(*base));
    JS2Object *obj = JS2VAL_TO_OBJECT(*base);
    ASSERT(obj->kind == SimpleInstanceKind);
    SpiderMonkeyInstance *smInst = checked_cast<SpiderMonkeyInstance *>(obj);
    nsPluginInstance *plug = smInst->pluginInstance;

    bool result = false;
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
    if (xpc) {
        nsIXPCNativeCallContext *aCurrentNativeCallContext;
        if (NS_SUCCEEDED(xpc->GetCurrentNativeCallContext(&aCurrentNativeCallContext))) {
            JSContext *aJSContext;
            if (NS_SUCCEEDED(aCurrentNativeCallContext->GetJSContext(&aJSContext))) {
                jsval v;
                if (JS_GetProperty(aJSContext, smInst->jsObject, str.c_str(), &v))
                    result = plug->convertJSValueToJS2Value(aJSContext, v, rval);
                NS_RELEASE(aCurrentNativeCallContext);
            }
        }
    }
    return result;
}

bool JS2SpiderMonkeyClass::BracketRead(JS2Metadata *meta, js2val *base, js2val indexVal, Phase phase, js2val *rval)
{
    return false;
}
bool JS2SpiderMonkeyClass::Write(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool createIfMissing, js2val newValue, bool initFlag)
{
    std::string str(multiname->name.length(), char());
    std::transform(multiname->name.begin(), multiname->name.end(), str.begin(), narrow);

    printf("JS2SpiderMonkeyClass: Writing property \"%s\"\n", str.c_str());
    

    ASSERT(JS2VAL_IS_OBJECT(base) && !JS2VAL_IS_NULL(base));
    JS2Object *obj = JS2VAL_TO_OBJECT(base);
    ASSERT(obj->kind == SimpleInstanceKind);
    SpiderMonkeyInstance *smInst = checked_cast<SpiderMonkeyInstance *>(obj);
    nsPluginInstance *plug = smInst->pluginInstance;

    bool result = false;
    nsCOMPtr<nsIXPConnect> xpc = do_GetService(nsIXPConnect::GetCID());
    if (xpc) {
        nsIXPCNativeCallContext *aCurrentNativeCallContext;
        if (NS_SUCCEEDED(xpc->GetCurrentNativeCallContext(&aCurrentNativeCallContext))) {
            JSContext *aJSContext;
            if (NS_SUCCEEDED(aCurrentNativeCallContext->GetJSContext(&aJSContext))) {
                jsval v;
                if (plug->convertJS2ValueToJSValue(aJSContext, newValue, &v)) {
                    if (JS_SetProperty(aJSContext, smInst->jsObject, str.c_str(), &v))
                        result = true;
                }
                NS_RELEASE(aCurrentNativeCallContext);
            }
        }
    }
    return result;
}
bool JS2SpiderMonkeyClass::BracketWrite(JS2Metadata *meta, js2val base, js2val indexVal, js2val newValue)
{
    return false;
}
bool JS2SpiderMonkeyClass::Delete(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool *result)
{
    return false;
}
bool JS2SpiderMonkeyClass::BracketDelete(JS2Metadata *meta, js2val base, js2val indexVal, bool *result)
{
    return false;
}

