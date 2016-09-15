/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PresentationTransportBuilderConstructor_h
#define mozilla_dom_PresentationTransportBuilderConstructor_h

#include "nsCOMPtr.h"
#include "nsIPresentationSessionTransportBuilder.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

class DummyPresentationTransportBuilderConstructor :
  public nsIPresentationTransportBuilderConstructor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONTRANSPORTBUILDERCONSTRUCTOR

  DummyPresentationTransportBuilderConstructor() = default;

protected:
  virtual ~DummyPresentationTransportBuilderConstructor() = default;
};

class PresentationTransportBuilderConstructor final :
  public DummyPresentationTransportBuilderConstructor
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIPRESENTATIONTRANSPORTBUILDERCONSTRUCTOR

  static already_AddRefed<nsIPresentationTransportBuilderConstructor>
  Create();

private:
  PresentationTransportBuilderConstructor() = default;
  virtual ~PresentationTransportBuilderConstructor() = default;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PresentationTransportBuilderConstructor_h
