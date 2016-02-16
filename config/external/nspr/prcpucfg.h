/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSPR_PRCPUCFG_H_
#define NSPR_PRCPUCFG_H_

/*
 * Need to support conditionals that are defined in both the top-level build
 * system as well as NSS' build system for now.
 */
#if defined(XP_DARWIN) || defined(DARWIN)
#include "md/_darwin.cfg"
#elif defined(XP_WIN) || defined(_WINDOWS)
#include "md/_win95.cfg"
#elif defined(__FreeBSD__)
#include "md/_freebsd.cfg"
#elif defined(__NetBSD__)
#include "md/_netbsd.cfg"
#elif defined(__OpenBSD__)
#include "md/_openbsd.cfg"
#elif defined(__linux__)
#include "md/_linux.cfg"
#else
#error "Unsupported platform!"
#endif

#endif /* NSPR_PRCPUCFG_H_ */
