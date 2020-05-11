/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_HTMLFormSubmissionConstants_h
#define mozilla_dom_HTMLFormSubmissionConstants_h

#include "nsIForm.h"

static const nsAttrValue::EnumTable kFormMethodTable[] = {
    {"get", NS_FORM_METHOD_GET}, {"post", NS_FORM_METHOD_POST}, {nullptr, 0}};

static const nsAttrValue::EnumTable kFormMethodTableDialogEnabled[] = {
    {"get", NS_FORM_METHOD_GET},
    {"post", NS_FORM_METHOD_POST},
    {"dialog", NS_FORM_METHOD_DIALOG},
    {nullptr, 0}};

// Default method is 'get'.
static const nsAttrValue::EnumTable* kFormDefaultMethod = &kFormMethodTable[0];

static const nsAttrValue::EnumTable kFormEnctypeTable[] = {
    {"multipart/form-data", NS_FORM_ENCTYPE_MULTIPART},
    {"application/x-www-form-urlencoded", NS_FORM_ENCTYPE_URLENCODED},
    {"text/plain", NS_FORM_ENCTYPE_TEXTPLAIN},
    {nullptr, 0}};

// Default method is 'application/x-www-form-urlencoded'.
static const nsAttrValue::EnumTable* kFormDefaultEnctype =
    &kFormEnctypeTable[1];

#endif  // mozilla_dom_HTMLFormSubmissionConstants_h
