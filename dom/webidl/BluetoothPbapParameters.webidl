/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * This enum holds the parameters to indicate the properties contained in the
 * requested vCard objects.
 */
enum vCardProperties
{
  "version",
  "fn",
  "n",
  "photo",
  "bday",
  "adr",
  "label",
  "tel",
  "email",
  "mailer",
  "tz",
  "geo",
  "title",
  "role",
  "logo",
  "agent",
  "org",
  "note",
  "rev",
  "sound",
  "url",
  "uid",
  "key",
  "nickname",
  "categories",
  "proid",
  "class",
  "sort-string",
  "x-irmc-call-datetime",
  "x-bt-speeddialkey",
  "x-bt-uci",
  "x-bt-uid"
};

/**
 * This enum holds the parameters to indicate the sorting order of vCard
 * objects.
 */
enum vCardOrderType {
  "indexed",  // default
  "alphabetical",
  "phonetical"
};

/**
 * This enum holds the parameters to indicate the search key of the search
 * operation.
 */
enum vCardSearchKeyType {
  "name",  // default
  "number",
  "sound"
};

/**
 * This enum holds the parameters to indicate the vCard version.
 */
enum vCardVersion {
  "vCard21", // default
  "vCard30"
};

/**
 * This enum holds the parameters to indicate the type of vCard selector.
 */
enum vCardSelectorOp {
  "OR", // default
  "AND"
};
