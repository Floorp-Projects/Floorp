/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Matthews <josh@joshmatthews.net> (Initial Developer)
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

#ifndef mozilla_RegistryMessageUtils_h
#define mozilla_RegistryMessageUtils_h

#include "IPC/IPCMessageUtils.h"
#include "nsStringGlue.h"

struct SerializedURI
{
  nsCString spec;
  nsCString charset;
};

struct ChromePackage
{
  nsCString package;
  SerializedURI contentBaseURI;
  SerializedURI localeBaseURI;
  SerializedURI skinBaseURI;
  PRUint32 flags;
};

struct ResourceMapping
{
  nsCString resource;
  SerializedURI resolvedURI;
};

struct OverrideMapping
{
  SerializedURI originalURI;
  SerializedURI overrideURI;
};

namespace IPC {

template<>
struct ParamTraits<SerializedURI>
{
  typedef SerializedURI paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.spec);
    WriteParam(aMsg, aParam.charset);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    nsCString spec, charset;
    if (ReadParam(aMsg, aIter, &spec) &&
        ReadParam(aMsg, aIter, &charset)) {
      aResult->spec = spec;
      aResult->charset = charset;
      return true;
    }
    return false;
  }
};
  
template <>
struct ParamTraits<ChromePackage>
{
  typedef ChromePackage paramType;
  
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.package);
    WriteParam(aMsg, aParam.contentBaseURI);
    WriteParam(aMsg, aParam.localeBaseURI);
    WriteParam(aMsg, aParam.skinBaseURI);
    WriteParam(aMsg, aParam.flags);
  }
  
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    nsCString package;
    SerializedURI contentBaseURI, localeBaseURI, skinBaseURI;
    PRUint32 flags;
    
    if (ReadParam(aMsg, aIter, &package) &&
        ReadParam(aMsg, aIter, &contentBaseURI) &&
        ReadParam(aMsg, aIter, &localeBaseURI) &&
        ReadParam(aMsg, aIter, &skinBaseURI) &&
        ReadParam(aMsg, aIter, &flags)) {
      aResult->package = package;
      aResult->contentBaseURI = contentBaseURI;
      aResult->localeBaseURI = localeBaseURI;
      aResult->skinBaseURI = skinBaseURI;
      aResult->flags = flags;
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%s, %s, %s, %s, %u]", aParam.package.get(),
                             aParam.contentBaseURI.spec.get(),
                             aParam.localeBaseURI.spec.get(),
                             aParam.skinBaseURI.spec.get(), aParam.flags));
  }
};

template <>
struct ParamTraits<ResourceMapping>
{
  typedef ResourceMapping paramType;
  
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.resource);
    WriteParam(aMsg, aParam.resolvedURI);
  }
  
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    nsCString resource;
    SerializedURI resolvedURI;
    
    if (ReadParam(aMsg, aIter, &resource) &&
        ReadParam(aMsg, aIter, &resolvedURI)) {
      aResult->resource = resource;
      aResult->resolvedURI = resolvedURI;
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%s, %s, %u]", aParam.resource.get(),
                             aParam.resolvedURI.spec.get()));
  }
};

template <>
struct ParamTraits<OverrideMapping>
{
  typedef OverrideMapping paramType;
  
  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.originalURI);
    WriteParam(aMsg, aParam.overrideURI);
  }
  
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    SerializedURI originalURI;
    SerializedURI overrideURI;
    
    if (ReadParam(aMsg, aIter, &originalURI) &&
        ReadParam(aMsg, aIter, &overrideURI)) {
      aResult->originalURI = originalURI;
      aResult->overrideURI = overrideURI;
      return true;
    }
    return false;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%s, %s, %u]", aParam.originalURI.spec.get(),
                             aParam.overrideURI.spec.get()));
  }
};

}

#endif // RegistryMessageUtils_h
