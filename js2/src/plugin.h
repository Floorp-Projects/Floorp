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

#ifndef __PLUGIN_H__
#define __PLUGIN_H__

#include "pluginbase.h"
#include "nsScriptablePeer.h"



class nsPluginInstance : public nsPluginInstanceBase
{
public:
  nsPluginInstance(NPP aInstance);
  ~nsPluginInstance();

  NPBool init(NPWindow* aWindow);
  void shut();
  NPBool isInitialized();

  // we need to provide implementation of this method as it will be
  // used by Mozilla to retrive the scriptable peer
  NPError	GetValue(NPPVariable variable, void *value);


    JavaScript::World *world;
    JavaScript::MetaData::JS2Metadata *metadata;
    JavaScript::MetaData::JS2Class *spiderMonkeyClass;

  // locals
  void invoke(const char *target, char **_retval);
  void GetDocument(nsISupports * *aDocument);
  void SetDocument(nsISupports *aDocument);

  
bool convertJSValueToJS2Value(JSContext *cx, jsval v, js2val *rval);
bool convertJS2ValueToJSValue(JSContext *cx, js2val v, jsval *rval);
  
  
  nsScriptablePeer* getScriptablePeer();


  virtual NPError NewStream(NPMIMEType type, NPStream* stream, 
                            NPBool seekable, uint16* stype);
  virtual void StreamAsFile(NPStream* stream, const char* fname);


private:
  NPP mInstance;
  NPBool mInitialized;
  HWND mhWnd;
  nsScriptablePeer * mScriptablePeer;

  nsISupports *doc;

public:
  char mString[128];
};

class JS2SpiderMonkeyClass : public JavaScript::MetaData::JS2Class {
public:
    JS2SpiderMonkeyClass(const StringAtom &name)
        : JS2Class(NULL, JS2VAL_VOID, NULL, false, true, name) { }

    virtual bool Read(JS2Metadata *meta, js2val *base, Multiname *multiname, Environment *env, Phase phase, js2val *rval);
    virtual bool BracketRead(JS2Metadata *meta, js2val *base, js2val indexVal, Phase phase, js2val *rval);
    virtual bool Write(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool createIfMissing, js2val newValue, bool initFlag);
    virtual bool BracketWrite(JS2Metadata *meta, js2val base, js2val indexVal, js2val newValue);
    virtual bool Delete(JS2Metadata *meta, js2val base, Multiname *multiname, Environment *env, bool *result);
    virtual bool BracketDelete(JS2Metadata *meta, js2val base, js2val indexVal, bool *result);

};

class SpiderMonkeyInstance : public JavaScript::MetaData::SimpleInstance {
public:
    SpiderMonkeyInstance(JS2Metadata *meta, js2val parent, JS2Class *type) : SimpleInstance(meta, parent, type) { }

    JSObject *jsObject;
    nsPluginInstance *pluginInstance;

    virtual ~SpiderMonkeyInstance()    { }
};

class SpiderMonkeyFunction : public JavaScript::MetaData::FunctionInstance {
public:
    SpiderMonkeyFunction(JS2Metadata *meta) : FunctionInstance(meta, meta->functionClass->prototype, meta->functionClass) { }

    JSObject *jsObject;
    nsPluginInstance *pluginInstance;

    virtual ~SpiderMonkeyFunction()    { }
};


#endif // __PLUGIN_H__
