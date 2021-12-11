/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// IWYU pragma: private, include "mozilla/dom/Document.h"

/*
 * This file contains the list of document DOM operations warnings.  It is
 * designed to be used as input to the C preprocessor *only*.
 */

DOCUMENT_WARNING(IgnoringWillChangeOverBudget)
DOCUMENT_WARNING(PreventDefaultFromPassiveListener)
DOCUMENT_WARNING(SVGRefLoop)
DOCUMENT_WARNING(SVGRefChainLengthExceeded)
DOCUMENT_WARNING(NotificationsRequireUserGestureDeprecation)
DOCUMENT_WARNING(DocumentSetDomainNotAllowed)
