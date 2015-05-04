/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsStubMutationObserver is an implementation of the nsIMutationObserver
 * interface (except for the methods on nsISupports) that is intended to be
 * used as a base class within the content/layout library.  All methods do
 * nothing.
 */

#include "nsStubMutationObserver.h"

NS_IMPL_NSIMUTATIONOBSERVER_CORE_STUB(nsStubMutationObserver)
NS_IMPL_NSIMUTATIONOBSERVER_CONTENT(nsStubMutationObserver)
