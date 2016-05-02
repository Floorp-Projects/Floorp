/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary InstallParameters {
  sequence<DOMString> receipts = [];
  sequence<DOMString> categories = [];
};

dictionary LanguageDesc {
  DOMString target;
  long revision;
  DOMString name;
};

enum LocaleResourceType {
  "binary",
  "json",
  "text"
};

[NoInterfaceObject, NavigatorProperty="mozApps",
 JSImplementation="@mozilla.org/webapps;1"]
interface DOMApplicationsRegistry {
  [CheckAnyPermissions="webapps-manage homescreen-webapps-manage"]
  readonly attribute DOMApplicationsManager mgmt;
  DOMRequest install(DOMString url, optional InstallParameters params);
  DOMRequest installPackage(DOMString url, optional InstallParameters params);
  DOMRequest getSelf();
  DOMRequest getInstalled();
  DOMRequest checkInstalled(DOMString manifestUrl);

  // Language pack API.
  // These promises will be rejected if the page is not in an app context,
  // i.e. they are implicitely acting on getSelf().
  Promise<MozMap<sequence<LanguageDesc>>> getAdditionalLanguages();
  // Resolves to a different object depending on the dataType value.
  Promise<any>
    getLocalizationResource(DOMString language,
                            DOMString version,
                            DOMString path,
                            LocaleResourceType dataType);
};

[JSImplementation="@mozilla.org/webapps/application;1", ChromeOnly]
interface DOMApplication : EventTarget {
  // manifest and updateManifest will be turned into dictionaries once
  // in bug 1053033 once bug 963382 is fixed.
  readonly attribute any manifest;
  readonly attribute any updateManifest;
  readonly attribute DOMString manifestURL;
  readonly attribute DOMString origin;
  readonly attribute DOMString installOrigin;
  readonly attribute DOMTimeStamp installTime;
  readonly attribute boolean removable;
  readonly attribute boolean enabled;

  [Cached, Pure]
  readonly attribute sequence<DOMString> receipts;

  readonly attribute double progress;

  readonly attribute DOMString installState;

  readonly attribute DOMTimeStamp lastUpdateCheck;
  readonly attribute DOMTimeStamp updateTime;

  readonly attribute boolean downloadAvailable;
  readonly attribute boolean downloading;
  readonly attribute boolean readyToApplyDownload;
  readonly attribute long downloadSize;

  readonly attribute DOMError? downloadError;

  attribute EventHandler onprogress;
  attribute EventHandler ondownloadsuccess;
  attribute EventHandler ondownloaderror;
  attribute EventHandler ondownloadavailable;
  attribute EventHandler ondownloadapplied;

  void download();
  void cancelDownload();

  DOMRequest launch(optional DOMString? url);

  DOMRequest clearBrowserData();
  DOMRequest checkForUpdate();

  // Receipts handling functions.
  DOMRequest addReceipt(optional DOMString receipt);
  DOMRequest removeReceipt(optional DOMString receipt);
  DOMRequest replaceReceipt(optional DOMString oldReceipt,
                            optional DOMString newReceipt);

  // Export this app as a shareable Blob.
  Promise<Blob> export();

  // Returns the localized value of a property, using either the manifest or
  // a langpack if one is available.
  Promise<DOMString> getLocalizedValue(DOMString property,
                                       DOMString locale,
                                       optional DOMString entryPoint);
};

[JSImplementation="@mozilla.org/webapps/manager;1",
 ChromeOnly,
 CheckAnyPermissions="webapps-manage homescreen-webapps-manage"]
interface DOMApplicationsManager : EventTarget {
  DOMRequest getAll();

  [CheckAnyPermissions="webapps-manage"]
  void applyDownload(DOMApplication app);
  DOMRequest uninstall(DOMApplication app);

  [CheckAnyPermissions="webapps-manage"]
  Promise<DOMApplication> import(Blob blob);
  [CheckAnyPermissions="webapps-manage"]
  Promise<any> extractManifest(Blob blob);

  [CheckAnyPermissions="webapps-manage"]
  void setEnabled(DOMApplication app, boolean state);
  Promise<Blob> getIcon(DOMApplication app, DOMString iconID,
                        optional DOMString entryPoint);

  attribute EventHandler oninstall;
  attribute EventHandler onuninstall;
  attribute EventHandler onenabledstatechange;
};
