/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsIForm_h___
#define nsIForm_h___

#include "nsISupports.h"
#include "nsAString.h"

class nsIFormControl;

#define NS_FORM_METHOD_GET  0
#define NS_FORM_METHOD_POST 1
#define NS_FORM_ENCTYPE_URLENCODED 0
#define NS_FORM_ENCTYPE_MULTIPART  1
#define NS_FORM_ENCTYPE_TEXTPLAIN  2

// IID for the nsIForm interface
#define NS_IFORM_IID \
{ 0x5e8464c8, 0x015d, 0x4cf9, \
  { 0x92, 0xc9, 0xa6, 0xb3, 0x30, 0x8f, 0x60, 0x9d } }

/**
 * This interface provides some methods that can be used to access the
 * guts of a form.  It's being slowly phased out.
 */

class nsIForm : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFORM_IID)

  /**
   * Get the element at a specified index position in form.elements
   *
   * @param aIndex the index
   * @param aElement the element at that index
   * @return NS_OK if there was an element at that position, -1 otherwise
   */
  NS_IMETHOD_(nsIFormControl*) GetElementAt(int32_t aIndex) const = 0;

  /**
   * Get the number of elements in form.elements
   *
   * @param aCount the number of elements
   * @return NS_OK if there was an element at that position, -1 otherwise
   */
  NS_IMETHOD_(uint32_t) GetElementCount() const = 0;

  /**
   * Get the index of the given control within form.elements.
   * @param aControl the control to find the index of
   * @param aIndex the index [OUT]
   */
  NS_IMETHOD_(int32_t) IndexOfControl(nsIFormControl* aControl) = 0;

  /**
   * Get the default submit element. If there's no default submit element,
   * return null.
   */
   NS_IMETHOD_(nsIFormControl*) GetDefaultSubmitElement() const = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIForm, NS_IFORM_IID)

#endif /* nsIForm_h___ */
