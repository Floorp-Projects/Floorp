/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIRadioVisitor_h___
#define nsIRadioVisitor_h___

#include "nsISupports.h"
class nsIFormControl;

// IID for the nsIRadioControl interface
#define NS_IRADIOVISITOR_IID \
{ 0xc6bed232, 0x1181, 0x4ab2, \
  { 0xa1, 0xda, 0x55, 0xc2, 0x13, 0x6d, 0xea, 0x3d } }

/**
 * This interface is used for the text control frame to store its value away
 * into the content.
 */
class nsIRadioVisitor : public nsISupports {
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IRADIOVISITOR_IID)

  /**
   * Visit a node in the tree.  This is meant to be called on all radios in a
   * group, sequentially.  (Each radio group implementor may define
   * sequentially in their own way, it just has to be the same every time.)
   * Currently all radio groups are ordered in the order they appear in the
   * document.  Radio group implementors should honor the return value of the
   * method and stop iterating if the return value is false.
   *
   * @param aRadio the radio button in question (must be nsnull and QI'able to
   *               nsIRadioControlElement)
   */
  virtual bool Visit(nsIFormControl* aRadio) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIRadioVisitor, NS_IRADIOVISITOR_IID)

#endif // nsIRadioVisitor_h___
