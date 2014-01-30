/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary ContactAddress {
  sequence<DOMString>? type;
  DOMString? streetAddress;
  DOMString? locality;
  DOMString? region;
  DOMString? postalCode;
  DOMString? countryName;
  boolean? pref;
};

dictionary ContactField {
  sequence<DOMString>? type;
  DOMString?           value;
  boolean?             pref;
};

dictionary ContactTelField : ContactField {
  DOMString? carrier;
};

dictionary ContactProperties {
  Date?                          bday;
  Date?                          anniversary;

  DOMString?                     sex;
  DOMString?                     genderIdentity;

  sequence<Blob>?                photo;

  sequence<ContactAddress>?  adr;

  sequence<ContactField>?    email;
  sequence<ContactField>?    url;
  sequence<ContactField>?    impp;

  sequence<ContactTelField>? tel;

  sequence<DOMString>?           name;
  sequence<DOMString>?           honorificPrefix;
  sequence<DOMString>?           givenName;
  sequence<DOMString>?           additionalName;
  sequence<DOMString>?           familyName;
  sequence<DOMString>?           honorificSuffix;
  sequence<DOMString>?           nickname;
  sequence<DOMString>?           category;
  sequence<DOMString>?           org;
  sequence<DOMString>?           jobTitle;
  sequence<DOMString>?           note;
  sequence<DOMString>?           key;
};

[Constructor(optional ContactProperties properties),
 JSImplementation="@mozilla.org/contact;1"]
interface mozContact {
                 attribute DOMString  id;
        readonly attribute Date?      published;
        readonly attribute Date?      updated;

                 attribute Date?      bday;
                 attribute Date?      anniversary;

                 attribute DOMString? sex;
                 attribute DOMString? genderIdentity;

  [Cached, Pure] attribute sequence<Blob>?            photo;

  [Cached, Pure] attribute sequence<ContactAddress>?  adr;

  [Cached, Pure] attribute sequence<ContactField>?    email;
  [Cached, Pure] attribute sequence<ContactField>?    url;
  [Cached, Pure] attribute sequence<ContactField>?    impp;

  [Cached, Pure] attribute sequence<ContactTelField>? tel;

  [Cached, Pure] attribute sequence<DOMString>?       name;
  [Cached, Pure] attribute sequence<DOMString>?       honorificPrefix;
  [Cached, Pure] attribute sequence<DOMString>?       givenName;
  [Cached, Pure] attribute sequence<DOMString>?       additionalName;
  [Cached, Pure] attribute sequence<DOMString>?       familyName;
  [Cached, Pure] attribute sequence<DOMString>?       honorificSuffix;
  [Cached, Pure] attribute sequence<DOMString>?       nickname;
  [Cached, Pure] attribute sequence<DOMString>?       category;
  [Cached, Pure] attribute sequence<DOMString>?       org;
  [Cached, Pure] attribute sequence<DOMString>?       jobTitle;
  [Cached, Pure] attribute sequence<DOMString>?       note;
  [Cached, Pure] attribute sequence<DOMString>?       key;

  void init(optional ContactProperties properties);

  [ChromeOnly]
  void setMetadata(DOMString id, Date? published, Date? updated);

  jsonifier;
};

dictionary ContactFindSortOptions {
  DOMString sortBy;                    // "givenName" or "familyName"
  DOMString sortOrder = "ascending";   // e.g. "descending"
};

dictionary ContactFindOptions : ContactFindSortOptions {
  DOMString      filterValue;  // e.g. "Tom"
  DOMString      filterOp;     // e.g. "startsWith"
  any            filterBy;     // e.g. ["givenName", "nickname"]
  unsigned long  filterLimit = 0;
};

[NoInterfaceObject, NavigatorProperty="mozContacts",
 JSImplementation="@mozilla.org/contactManager;1"]
interface ContactManager : EventTarget {
  DOMRequest find(optional ContactFindOptions options);
  DOMCursor  getAll(optional ContactFindSortOptions options);
  DOMRequest clear();
  DOMRequest save(mozContact contact);
  DOMRequest remove((mozContact or DOMString) contactOrId);
  DOMRequest getRevision();
  DOMRequest getCount();

  attribute  EventHandler oncontactchange;
};
