/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly, Constructor, JSImplementation="@mozilla.org/contactAddress;1"]
interface ContactAddress {
  attribute object?    type; // DOMString[]
  attribute DOMString? streetAddress;
  attribute DOMString? locality;
  attribute DOMString? region;
  attribute DOMString? postalCode;
  attribute DOMString? countryName;
  attribute boolean?   pref;

  [ChromeOnly]
  void initialize(optional sequence<DOMString>? type,
                  optional DOMString? streetAddress,
                  optional DOMString? locality,
                  optional DOMString? region,
                  optional DOMString? postalCode,
                  optional DOMString? countryName,
                  optional boolean? pref);

  object toJSON();
};

dictionary ContactAddressInit {
  sequence<DOMString>? type;
  DOMString? streetAddress;
  DOMString? locality;
  DOMString? region;
  DOMString? postalCode;
  DOMString? countryName;
  boolean? pref;
};


[ChromeOnly, Constructor, JSImplementation="@mozilla.org/contactField;1"]
interface ContactField {
  attribute object?    type; // DOMString[]
  attribute DOMString? value;
  attribute boolean?   pref;

  [ChromeOnly]
  void initialize(optional sequence<DOMString>? type,
                  optional DOMString? value,
                  optional boolean? pref);

  object toJSON();
};

dictionary ContactFieldInit {
  sequence<DOMString>? type;
  DOMString?           value;
  boolean?             pref;
};


[ChromeOnly, Constructor, JSImplementation="@mozilla.org/contactTelField;1"]
interface ContactTelField : ContactField {
  attribute DOMString? carrier;

  [ChromeOnly]
  void initialize(optional sequence<DOMString>? type,
                  optional DOMString? value,
                  optional DOMString? carrier,
                  optional boolean? pref);

  object toJSON();
};

dictionary ContactTelFieldInit : ContactFieldInit {
  DOMString? carrier;
};


dictionary ContactProperties {
  Date?                          bday;
  Date?                          anniversary;

  DOMString?                     sex;
  DOMString?                     genderIdentity;

  sequence<Blob>?                photo;

  sequence<ContactAddressInit>?  adr;

  sequence<ContactFieldInit>?    email;
  sequence<ContactFieldInit>?    url;
  sequence<ContactFieldInit>?    impp;

  sequence<ContactTelFieldInit>? tel;

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
           attribute DOMString    id;
  readonly attribute Date?        published;
  readonly attribute Date?        updated;

           attribute Date?        bday;
           attribute Date?        anniversary;

           attribute DOMString?   sex;
           attribute DOMString?   genderIdentity;

           attribute object?      photo;

           attribute object?      adr;

           attribute object?      email;
           attribute object?      url;
           attribute object?      impp;

           attribute object?      tel;

           attribute object?      name;
           attribute object?      honorificPrefix;
           attribute object?      givenName;
           attribute object?      additionalName;
           attribute object?      familyName;
           attribute object?      honorificSuffix;
           attribute object?      nickname;
           attribute object?      category;
           attribute object?      org;
           attribute object?      jobTitle;
           attribute object?      note;
           attribute object?      key;

  [ChromeOnly]
  void setMetadata(DOMString id, Date? published, Date? updated);

  object toJSON();
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
  DOMRequest remove(mozContact contact);
  DOMRequest getRevision();
  DOMRequest getCount();

  attribute  EventHandler oncontactchange;
};
