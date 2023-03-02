/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_RegistryMessageUtils_h
#define mozilla_RegistryMessageUtils_h

#include "mozilla/ipc/IPDLParamTraits.h"
#include "nsString.h"

struct SerializedURI {
  nsCString spec;

  bool operator==(const SerializedURI& rhs) const {
    return spec.Equals(rhs.spec);
  }
};

struct ChromePackage {
  nsCString package;
  SerializedURI contentBaseURI;
  SerializedURI localeBaseURI;
  SerializedURI skinBaseURI;
  uint32_t flags;

  bool operator==(const ChromePackage& rhs) const {
    return package.Equals(rhs.package) &&
           contentBaseURI == rhs.contentBaseURI &&
           localeBaseURI == rhs.localeBaseURI &&
           skinBaseURI == rhs.skinBaseURI && flags == rhs.flags;
  }
};

struct SubstitutionMapping {
  nsCString scheme;
  nsCString path;
  SerializedURI resolvedURI;
  uint32_t flags;

  bool operator==(const SubstitutionMapping& rhs) const {
    return scheme.Equals(rhs.scheme) && path.Equals(rhs.path) &&
           resolvedURI == rhs.resolvedURI && flags == rhs.flags;
  }
};

struct OverrideMapping {
  SerializedURI originalURI;
  SerializedURI overrideURI;

  bool operator==(const OverrideMapping& rhs) const {
    return originalURI == rhs.originalURI && overrideURI == rhs.overrideURI;
  }
};

namespace IPC {

template <>
struct ParamTraits<SerializedURI> {
  typedef SerializedURI paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.spec);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    nsCString spec;
    if (ReadParam(aReader, &spec)) {
      aResult->spec = spec;
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<ChromePackage> {
  typedef ChromePackage paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.package);
    WriteParam(aWriter, aParam.contentBaseURI);
    WriteParam(aWriter, aParam.localeBaseURI);
    WriteParam(aWriter, aParam.skinBaseURI);
    WriteParam(aWriter, aParam.flags);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    nsCString package;
    SerializedURI contentBaseURI, localeBaseURI, skinBaseURI;
    uint32_t flags;

    if (ReadParam(aReader, &package) && ReadParam(aReader, &contentBaseURI) &&
        ReadParam(aReader, &localeBaseURI) &&
        ReadParam(aReader, &skinBaseURI) && ReadParam(aReader, &flags)) {
      aResult->package = package;
      aResult->contentBaseURI = contentBaseURI;
      aResult->localeBaseURI = localeBaseURI;
      aResult->skinBaseURI = skinBaseURI;
      aResult->flags = flags;
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<SubstitutionMapping> {
  typedef SubstitutionMapping paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.scheme);
    WriteParam(aWriter, aParam.path);
    WriteParam(aWriter, aParam.resolvedURI);
    WriteParam(aWriter, aParam.flags);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    nsCString scheme, path;
    SerializedURI resolvedURI;
    uint32_t flags;

    if (ReadParam(aReader, &scheme) && ReadParam(aReader, &path) &&
        ReadParam(aReader, &resolvedURI) && ReadParam(aReader, &flags)) {
      aResult->scheme = scheme;
      aResult->path = path;
      aResult->resolvedURI = resolvedURI;
      aResult->flags = flags;
      return true;
    }
    return false;
  }
};

template <>
struct ParamTraits<OverrideMapping> {
  typedef OverrideMapping paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.originalURI);
    WriteParam(aWriter, aParam.overrideURI);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    SerializedURI originalURI;
    SerializedURI overrideURI;

    if (ReadParam(aReader, &originalURI) && ReadParam(aReader, &overrideURI)) {
      aResult->originalURI = originalURI;
      aResult->overrideURI = overrideURI;
      return true;
    }
    return false;
  }
};

}  // namespace IPC

#endif  // RegistryMessageUtils_h
