/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RegistrationAnnotator.h"

#include "mozilla/JSONWriter.h"
#include "mozilla/mscom/Utils.h"
#include "mozilla/NotNull.h"
#include "nsExceptionHandler.h"
#include "nsPrintfCString.h"
#include "nsWindowsHelpers.h"
#include "nsXULAppAPI.h"

#include <oleauto.h>

namespace {

class CStringWriter final : public mozilla::JSONWriteFunc {
 public:
  void Write(const char* aStr) override { mBuf += aStr; }
  void Write(const char* aStr, size_t aLen) override {
    mBuf.Append(aStr, aLen);
  }

  const nsCString& Get() const { return mBuf; }

 private:
  nsCString mBuf;
};

}  // anonymous namespace

namespace mozilla {
namespace mscom {

static const char16_t kSoftwareClasses[] = u"SOFTWARE\\Classes";
static const char16_t kInterface[] = u"\\Interface\\";
static const char16_t kDefaultValue[] = u"";
static const char16_t kThreadingModel[] = u"ThreadingModel";
static const char16_t kBackslash[] = u"\\";
static const char16_t kFlags[] = u"FLAGS";
static const char16_t kProxyStubClsid32[] = u"\\ProxyStubClsid32";
static const char16_t kClsid[] = u"\\CLSID\\";
static const char16_t kInprocServer32[] = u"\\InprocServer32";
static const char16_t kInprocHandler32[] = u"\\InprocHandler32";
static const char16_t kTypeLib[] = u"\\TypeLib";
static const char16_t kVersion[] = u"Version";
static const char16_t kWin32[] = u"Win32";
static const char16_t kWin64[] = u"Win64";

static bool GetStringValue(HKEY aBaseKey, const nsAString& aStrSubKey,
                           const nsAString& aValueName, nsAString& aOutput) {
  const nsString& flatSubKey = PromiseFlatString(aStrSubKey);
  const nsString& flatValueName = PromiseFlatString(aValueName);
  LPCWSTR valueName = aValueName.IsEmpty() ? nullptr : flatValueName.get();

  DWORD type = 0;
  DWORD numBytes = 0;
  LONG result = RegGetValue(aBaseKey, flatSubKey.get(), valueName, RRF_RT_ANY,
                            &type, nullptr, &numBytes);
  if (result != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) {
    return false;
  }

  int numChars = (numBytes + 1) / sizeof(wchar_t);
  aOutput.SetLength(numChars);

  DWORD acceptFlag = type == REG_SZ ? RRF_RT_REG_SZ : RRF_RT_REG_EXPAND_SZ;

  result = RegGetValue(aBaseKey, flatSubKey.get(), valueName, acceptFlag,
                       nullptr, aOutput.BeginWriting(), &numBytes);
  if (result == ERROR_SUCCESS) {
    // Truncate null terminator
    aOutput.SetLength(((numBytes + 1) / sizeof(wchar_t)) - 1);
  }

  return result == ERROR_SUCCESS;
}

template <size_t N>
inline static bool GetStringValue(HKEY aBaseKey, const nsAString& aStrSubKey,
                                  const char16_t (&aValueName)[N],
                                  nsAString& aOutput) {
  return GetStringValue(aBaseKey, aStrSubKey, nsLiteralString(aValueName),
                        aOutput);
}

/**
 * This function fails unless the entire string has been converted.
 * (eg, the string "FLAGS" will convert to 0xF but we will return false)
 */
static bool ConvertLCID(const wchar_t* aStr, NotNull<unsigned long*> aOutLcid) {
  wchar_t* endChar;
  *aOutLcid = wcstoul(aStr, &endChar, 16);
  return *endChar == 0;
}

static bool GetLoadedPath(nsAString& aPath) {
  // These paths may be REG_EXPAND_SZ, so we expand any environment strings
  DWORD bufCharLen =
      ExpandEnvironmentStrings(PromiseFlatString(aPath).get(), nullptr, 0);

  auto buf = MakeUnique<WCHAR[]>(bufCharLen);

  if (!ExpandEnvironmentStrings(PromiseFlatString(aPath).get(), buf.get(),
                                bufCharLen)) {
    return false;
  }

  // Use LoadLibrary so that the DLL is resolved using the loader's DLL search
  // rules
  nsModuleHandle mod(LoadLibrary(buf.get()));
  if (!mod) {
    return false;
  }

  WCHAR finalPath[MAX_PATH + 1] = {};
  DWORD result = GetModuleFileNameW(mod, finalPath, ArrayLength(finalPath));
  if (!result || (result == ArrayLength(finalPath) &&
                  GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
    return false;
  }

  aPath = nsDependentString(finalPath, result);
  return true;
}

static void AnnotateClsidRegistrationForHive(
    JSONWriter& aJson, HKEY aHive, const nsAString& aClsid,
    const JSONWriter::CollectionStyle aStyle) {
  nsAutoString clsidSubkey;
  clsidSubkey.AppendLiteral(kSoftwareClasses);
  clsidSubkey.AppendLiteral(kClsid);
  clsidSubkey.Append(aClsid);

  nsAutoString className;
  if (GetStringValue(aHive, clsidSubkey, kDefaultValue, className)) {
    aJson.StringProperty("ClassName", NS_ConvertUTF16toUTF8(className).get());
  }

  nsAutoString inprocServerSubkey(clsidSubkey);
  inprocServerSubkey.AppendLiteral(kInprocServer32);

  nsAutoString pathToServerDll;
  if (GetStringValue(aHive, inprocServerSubkey, kDefaultValue,
                     pathToServerDll)) {
    aJson.StringProperty("Path", NS_ConvertUTF16toUTF8(pathToServerDll).get());
    if (GetLoadedPath(pathToServerDll)) {
      aJson.StringProperty("LoadedPath",
                           NS_ConvertUTF16toUTF8(pathToServerDll).get());
    }
  }

  nsAutoString apartment;
  if (GetStringValue(aHive, inprocServerSubkey, kThreadingModel, apartment)) {
    aJson.StringProperty("ThreadingModel",
                         NS_ConvertUTF16toUTF8(apartment).get());
  }

  nsAutoString inprocHandlerSubkey(clsidSubkey);
  inprocHandlerSubkey.AppendLiteral(kInprocHandler32);
  nsAutoString pathToHandlerDll;
  if (GetStringValue(aHive, inprocHandlerSubkey, kDefaultValue,
                     pathToHandlerDll)) {
    aJson.StringProperty("HandlerPath",
                         NS_ConvertUTF16toUTF8(pathToHandlerDll).get());
    if (GetLoadedPath(pathToHandlerDll)) {
      aJson.StringProperty("LoadedHandlerPath",
                           NS_ConvertUTF16toUTF8(pathToHandlerDll).get());
    }
  }

  nsAutoString handlerApartment;
  if (GetStringValue(aHive, inprocHandlerSubkey, kThreadingModel,
                     handlerApartment)) {
    aJson.StringProperty("HandlerThreadingModel",
                         NS_ConvertUTF16toUTF8(handlerApartment).get());
  }
}

static void CheckTlbPath(JSONWriter& aJson, const nsAString& aTypelibPath) {
  const nsString& flatPath = PromiseFlatString(aTypelibPath);
  DWORD bufCharLen = ExpandEnvironmentStrings(flatPath.get(), nullptr, 0);

  auto buf = MakeUnique<WCHAR[]>(bufCharLen);

  if (!ExpandEnvironmentStrings(flatPath.get(), buf.get(), bufCharLen)) {
    return;
  }

  // See whether this tlb can actually be loaded
  RefPtr<ITypeLib> typeLib;
  HRESULT hr = LoadTypeLibEx(buf.get(), REGKIND_NONE, getter_AddRefs(typeLib));

  nsPrintfCString loadResult("0x%08X", hr);
  aJson.StringProperty("LoadResult", loadResult.get());
}

template <size_t N>
static void AnnotateTypelibPlatform(JSONWriter& aJson, HKEY aBaseKey,
                                    const nsAString& aLcidSubkey,
                                    const char16_t (&aPlatform)[N],
                                    const JSONWriter::CollectionStyle aStyle) {
  nsLiteralString platform(aPlatform);

  nsAutoString fullSubkey(aLcidSubkey);
  fullSubkey.AppendLiteral(kBackslash);
  fullSubkey.Append(platform);

  nsAutoString tlbPath;
  if (GetStringValue(aBaseKey, fullSubkey, kDefaultValue, tlbPath)) {
    aJson.StartObjectProperty(NS_ConvertUTF16toUTF8(platform).get(), aStyle);
    aJson.StringProperty("Path", NS_ConvertUTF16toUTF8(tlbPath).get());
    CheckTlbPath(aJson, tlbPath);
    aJson.EndObject();
  }
}

static void AnnotateTypelibRegistrationForHive(
    JSONWriter& aJson, HKEY aHive, const nsAString& aTypelibId,
    const nsAString& aTypelibVersion,
    const JSONWriter::CollectionStyle aStyle) {
  nsAutoString typelibSubKey;
  typelibSubKey.AppendLiteral(kSoftwareClasses);
  typelibSubKey.AppendLiteral(kTypeLib);
  typelibSubKey.AppendLiteral(kBackslash);
  typelibSubKey.Append(aTypelibId);
  typelibSubKey.AppendLiteral(kBackslash);
  typelibSubKey.Append(aTypelibVersion);

  nsAutoString typelibDesc;
  if (GetStringValue(aHive, typelibSubKey, kDefaultValue, typelibDesc)) {
    aJson.StringProperty("Description",
                         NS_ConvertUTF16toUTF8(typelibDesc).get());
  }

  nsAutoString flagsSubKey(typelibSubKey);
  flagsSubKey.AppendLiteral(kBackslash);
  flagsSubKey.AppendLiteral(kFlags);

  nsAutoString typelibFlags;
  if (GetStringValue(aHive, flagsSubKey, kDefaultValue, typelibFlags)) {
    aJson.StringProperty("Flags", NS_ConvertUTF16toUTF8(typelibFlags).get());
  }

  HKEY rawTypelibKey;
  LONG result =
      RegOpenKeyEx(aHive, typelibSubKey.get(), 0, KEY_READ, &rawTypelibKey);
  if (result != ERROR_SUCCESS) {
    return;
  }
  nsAutoRegKey typelibKey(rawTypelibKey);

  const size_t kMaxLcidCharLen = 9;
  WCHAR keyName[kMaxLcidCharLen];

  for (DWORD index = 0; result == ERROR_SUCCESS; ++index) {
    DWORD keyNameLength = ArrayLength(keyName);
    result = RegEnumKeyEx(typelibKey, index, keyName, &keyNameLength, nullptr,
                          nullptr, nullptr, nullptr);

    unsigned long lcid;
    if (result == ERROR_SUCCESS && ConvertLCID(keyName, WrapNotNull(&lcid))) {
      nsDependentString strLcid(keyName, keyNameLength);
      aJson.StartObjectProperty(NS_ConvertUTF16toUTF8(strLcid).get(), aStyle);
      AnnotateTypelibPlatform(aJson, typelibKey, strLcid, kWin32, aStyle);
#if defined(HAVE_64BIT_BUILD)
      AnnotateTypelibPlatform(aJson, typelibKey, strLcid, kWin64, aStyle);
#endif
      aJson.EndObject();
    }
  }
}

static void AnnotateInterfaceRegistrationForHive(
    JSONWriter& aJson, HKEY aHive, REFIID aIid,
    const JSONWriter::CollectionStyle aStyle) {
  nsAutoString interfaceSubKey;
  interfaceSubKey.AppendLiteral(kSoftwareClasses);
  interfaceSubKey.AppendLiteral(kInterface);
  nsAutoString iid;
  GUIDToString(aIid, iid);
  interfaceSubKey.Append(iid);

  nsAutoString interfaceName;
  if (GetStringValue(aHive, interfaceSubKey, kDefaultValue, interfaceName)) {
    aJson.StringProperty("InterfaceName",
                         NS_ConvertUTF16toUTF8(interfaceName).get());
  }

  nsAutoString psSubKey(interfaceSubKey);
  psSubKey.AppendLiteral(kProxyStubClsid32);

  nsAutoString psClsid;
  if (GetStringValue(aHive, psSubKey, kDefaultValue, psClsid)) {
    aJson.StartObjectProperty("ProxyStub", aStyle);
    aJson.StringProperty("CLSID", NS_ConvertUTF16toUTF8(psClsid).get());
    AnnotateClsidRegistrationForHive(aJson, aHive, psClsid, aStyle);
    aJson.EndObject();
  }

  nsAutoString typelibSubKey(interfaceSubKey);
  typelibSubKey.AppendLiteral(kTypeLib);

  nsAutoString typelibId;
  bool haveTypelibId =
      GetStringValue(aHive, typelibSubKey, kDefaultValue, typelibId);

  nsAutoString typelibVersion;
  bool haveTypelibVersion =
      GetStringValue(aHive, typelibSubKey, kVersion, typelibVersion);

  if (haveTypelibId || haveTypelibVersion) {
    aJson.StartObjectProperty("TypeLib", aStyle);
  }

  if (haveTypelibId) {
    aJson.StringProperty("ID", NS_ConvertUTF16toUTF8(typelibId).get());
  }

  if (haveTypelibVersion) {
    aJson.StringProperty("Version",
                         NS_ConvertUTF16toUTF8(typelibVersion).get());
  }

  if (haveTypelibId && haveTypelibVersion) {
    AnnotateTypelibRegistrationForHive(aJson, aHive, typelibId, typelibVersion,
                                       aStyle);
  }

  if (haveTypelibId || haveTypelibVersion) {
    aJson.EndObject();
  }
}

void AnnotateInterfaceRegistration(REFIID aIid) {
#if defined(DEBUG)
  const JSONWriter::CollectionStyle style = JSONWriter::MultiLineStyle;
#else
  const JSONWriter::CollectionStyle style = JSONWriter::SingleLineStyle;
#endif

  JSONWriter json(MakeUnique<CStringWriter>());

  json.Start(style);

  json.StartObjectProperty("HKLM", style);
  AnnotateInterfaceRegistrationForHive(json, HKEY_LOCAL_MACHINE, aIid, style);
  json.EndObject();

  json.StartObjectProperty("HKCU", style);
  AnnotateInterfaceRegistrationForHive(json, HKEY_CURRENT_USER, aIid, style);
  json.EndObject();

  json.End();

  CrashReporter::Annotation annotationKey;
  if (XRE_IsParentProcess()) {
    annotationKey = CrashReporter::Annotation::InterfaceRegistrationInfoParent;
  } else {
    annotationKey = CrashReporter::Annotation::InterfaceRegistrationInfoChild;
  }
  CrashReporter::AnnotateCrashReport(
      annotationKey, static_cast<CStringWriter*>(json.WriteFunc())->Get());
}

void AnnotateClassRegistration(REFCLSID aClsid) {
#if defined(DEBUG)
  const JSONWriter::CollectionStyle style = JSONWriter::MultiLineStyle;
#else
  const JSONWriter::CollectionStyle style = JSONWriter::SingleLineStyle;
#endif

  nsAutoString strClsid;
  GUIDToString(aClsid, strClsid);

  JSONWriter json(MakeUnique<CStringWriter>());

  json.Start(style);

  json.StartObjectProperty("HKLM", style);
  AnnotateClsidRegistrationForHive(json, HKEY_LOCAL_MACHINE, strClsid, style);
  json.EndObject();

  json.StartObjectProperty("HKCU", style);
  AnnotateClsidRegistrationForHive(json, HKEY_CURRENT_USER, strClsid, style);
  json.EndObject();

  json.End();

  CrashReporter::Annotation annotationKey;
  if (XRE_IsParentProcess()) {
    annotationKey = CrashReporter::Annotation::ClassRegistrationInfoParent;
  } else {
    annotationKey = CrashReporter::Annotation::ClassRegistrationInfoChild;
  }

  CrashReporter::AnnotateCrashReport(
      annotationKey, static_cast<CStringWriter*>(json.WriteFunc())->Get());
}

}  // namespace mscom
}  // namespace mozilla
