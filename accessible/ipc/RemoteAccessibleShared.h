/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_RemoteAccessibleShared_h
#define mozilla_a11y_RemoteAccessibleShared_h

/**
 * These are function declarations shared between win/RemoteAccessible.h and
 * other/RemoteAccessible.h.
 */

void ScrollToPoint(uint32_t aScrollType, int32_t aX, int32_t aY);

void Announce(const nsString& aAnnouncement, uint16_t aPriority);

void ScrollSubstringToPoint(int32_t aStartOffset, int32_t aEndOffset,
                            uint32_t aCoordinateType, int32_t aX, int32_t aY);

#endif
