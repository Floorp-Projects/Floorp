/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _nsRadioVisitor_h__
#define _nsRadioVisitor_h__

#include "mozilla/Attributes.h"
#include "nsIRadioVisitor.h"

using mozilla::dom::HTMLInputElement;

/**
 * nsRadioVisitor is the base class implementing nsIRadioVisitor and inherited
 * by all radio visitors.
 */
class nsRadioVisitor : public nsIRadioVisitor {
 protected:
  virtual ~nsRadioVisitor() = default;

 public:
  nsRadioVisitor() = default;

  NS_DECL_ISUPPORTS

  virtual bool Visit(HTMLInputElement* aRadio) override = 0;
};

/**
 * The following declarations are radio visitors inheriting from nsRadioVisitor.
 */

/**
 * nsRadioSetCheckedChangedVisitor is calling SetCheckedChanged with the given
 * parameter to all radio elements in the group.
 */
class nsRadioSetCheckedChangedVisitor : public nsRadioVisitor {
 public:
  explicit nsRadioSetCheckedChangedVisitor(bool aCheckedChanged)
      : mCheckedChanged(aCheckedChanged) {}

  virtual bool Visit(HTMLInputElement* aRadio) override;

 protected:
  bool mCheckedChanged;
};

/**
 * nsRadioGetCheckedChangedVisitor is getting the current checked changed value.
 * Getting it from one radio element is the group is enough given that all
 * elements should have the same value.
 */
class nsRadioGetCheckedChangedVisitor : public nsRadioVisitor {
 public:
  nsRadioGetCheckedChangedVisitor(bool* aCheckedChanged,
                                  HTMLInputElement* aExcludeElement)
      : mCheckedChanged(aCheckedChanged), mExcludeElement(aExcludeElement) {}

  virtual bool Visit(HTMLInputElement* aRadio) override;

 protected:
  bool* mCheckedChanged;
  HTMLInputElement* mExcludeElement;
};

/**
 * nsRadioSetValueMissingState is calling SetValueMissingState with the given
 * parameter to all radio elements in the group.
 * It is also calling ContentStatesChanged if needed.
 */
class nsRadioSetValueMissingState : public nsRadioVisitor {
 public:
  nsRadioSetValueMissingState(HTMLInputElement* aExcludeElement, bool aValidity)
      : mExcludeElement(aExcludeElement), mValidity(aValidity) {}

  virtual bool Visit(HTMLInputElement* aRadio) override;

 protected:
  HTMLInputElement* mExcludeElement;
  bool mValidity;
};

class nsRadioUpdateStateVisitor : public nsRadioVisitor {
 public:
  explicit nsRadioUpdateStateVisitor(HTMLInputElement* aExcludeElement)
      : mExcludeElement(aExcludeElement) {}

  virtual bool Visit(HTMLInputElement* aRadio) override;

 protected:
  HTMLInputElement* mExcludeElement;
};

#endif  // _nsRadioVisitor_h__
