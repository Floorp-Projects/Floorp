/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef timing_h__
#define timing_h__

#include "prtypes.h"

PR_BEGIN_EXTERN_C

/**
 * Write a timing message to the timing log module, using
 * <tt>printf()</tt> formatting. The message will be pre-pended with
 * thread and timing information. Do not use this function directly;
 * rather, use the <tt>TIMING_MSG()</tt> macro, below.
 * <p>
 * This is based on NSPR2.0 logging, so it is not necessary to
 * terminate messages with a newline.
 *
 * @fmtstr The <tt>printf()</tt>-style format string.
 */
extern void
TimingWriteMessage(const char* fmtstr, ...);

/**
 * Queries whether the timing log is currently enabled.
 *
 * @return <tt>PR_TRUE</tt> if the timing log is currently enabled.
 */
extern PRBool
TimingIsEnabled(void);

/**
 * Sets the timing log's state.
 *
 * @param enabled <tt>PR_TRUE</tt> to enable, <tt>PR_FALSE</tt> to
 * disable.
 */
extern void
TimingSetEnabled(PRBool enabled);

PR_END_EXTERN_C

/**
 * Use this macros to log timing information. It uses the
 * "double-parens" hack to allow you to pass arbitrarily formatted
 * strings; e.g.,
 *
 * <pre>
 * TIMING_MSG(("netlib: NET_ProcessNet: wasted call"));
 * </pre>
 */
#define TIMING_MSG(x) TimingWriteMessage x

#endif /* timing_h__ */

