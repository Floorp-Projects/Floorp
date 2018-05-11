/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * nsStubDocumentObserver is an implementation of the nsIDocumentObserver
 * interface (except for the methods on nsISupports) that is intended to be
 * used as a base class within the content/layout library.  All methods do
 * nothing.
 */

#include "nsStubDocumentObserver.h"

NS_IMPL_NSIDOCUMENTOBSERVER_CORE_STUB(nsStubDocumentObserver)
NS_IMPL_NSIDOCUMENTOBSERVER_LOAD_STUB(nsStubDocumentObserver)
NS_IMPL_NSIDOCUMENTOBSERVER_STATE_STUB(nsStubDocumentObserver)
NS_IMPL_NSIDOCUMENTOBSERVER_CONTENT(nsStubDocumentObserver)
