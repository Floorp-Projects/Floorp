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
#include "prtime.h"

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
PR_EXTERN(void)
TimingWriteMessage(const char* fmtstr, ...);

/**
 * Queries whether the timing log is currently enabled.
 *
 * @return <tt>PR_TRUE</tt> if the timing log is currently enabled.
 */
PR_EXTERN(PRBool)
TimingIsEnabled(void);

/**
 * Sets the timing log's state.
 *
 * @param enabled <tt>PR_TRUE</tt> to enable, <tt>PR_FALSE</tt> to
 * disable.
 */
PR_EXTERN(void)
TimingSetEnabled(PRBool enabled);


/**
 * Start a unique "clock" with the given name. If the clock already
 * exists, this call will <i>not</i> re-start it.
 *
 * @param clock A C-string name for the clock.
 */
PR_EXTERN(void)
TimingStartClock(const char* clock);

/**
 * Stop the "clock" with the given name, returning the elapsed
 * time in <tt>result</tt>. This destroys the clock.
 * <p>
 * If the clock has already been stopped, this call will have
 * no effect.
 *
 * @param result If successful, returns the time recorded on
 * the clock (in microseconds).
 * @param clock The C-string name of the clock to stop.
 * @return <tt>PR_TRUE</tt> if the clock exists, was running,
 * and was successfully stopped.
 */
PR_EXTERN(PRBool)
TimingStopClock(PRTime* result, const char* clock);

/**
 * Return <tt>PR_TRUE</tt> if the clock with the specified
 * name exists and is running.
 */
PR_EXTERN(PRBool)
TimingIsClockRunning(const char* clock);

/**
 * Convert an elapsed time into a human-readable string.
 *
 * @param time An elapsed <tt>PRTime</tt> value.
 * @param buffer The buffer to use for conversion.
 * @param size The size of <tt>buffer</tt>.
 * @return A pointer to <tt>buffer</tt>.
 */
PR_EXTERN(char*)
TimingElapsedTimeToString(PRTime time, char* buffer, PRUint32 size);

PR_END_EXTERN_C

#if defined(NO_TIMING)

#define TIMING_MESSAGE(args)                       ((void) 0)
#define TIMING_STARTCLOCK_NAME(op, name)           ((void) 0)
#define TIMING_STOPCLOCK_NAME(op, name, cx, msg)   ((void) 0)
#define TIMING_STARTCLOCK_OBJECT(op, obj)          ((void) 0)
#define TIMING_STOPCLOCK_OBJECT(op, obj, cx, msg)  ((void) 0)

#else /* !defined(NO_TIMING) */

/**
 * Use this macro to log timing information. It uses the
 * "double-parens" hack to allow you to pass arbitrarily formatted
 * strings; e.g.,
 *
 * <pre>
 * TIMING_MESSAGE(("cache,%s,not found", url->address));
 * </pre>
 */
#define TIMING_MESSAGE(args)                 TimingWriteMessage args


/**
 * Use this macro to start a "clock" on an object using a pointer
 * to the object; e.g.,
 *
 * <PRE>
 * TIMING_STARTCLOCK_OBJECT("http:request", URL_s);
 * </PRE>
 *
 * The clock should be uniquely identified by the <tt>op</tt> and
 * <tt>obj</tt> parameters.
 *
 * @param op A C-string that is the "operation" that is being
 * performed.
 * @param obj A pointer to an object.
 */
#define TIMING_STARTCLOCK_OBJECT(op, obj) \
do {\
    char buf[256];\
    PR_snprintf(buf, sizeof(buf), "%s,%08x", op, obj);\
    TimingStartClock(buf);\
} while (0)


/**
 * Use this macro to stop a "clock" on an object and print out
 * a message that indicates the total elapsed time on the clock;
 * e.g.,
 *
 * <PRE>
 * TIMING_STOPCLOCK_OBJECT("http:request", URL_s, "ok");
 * </PRE>
 *
 * The <tt>op</tt> and <tt>obj</tt> parameters are used to
 * identify the clock to stop.
 *
 * @param op A C-string that is the "operation" that is being
 * performed.
 * @param obj A pointer to an object.
 * @param cx A pointer to the MWContext.
 * @param msg A message to include in the log entry.
 */
#define TIMING_STOPCLOCK_OBJECT(op, obj, cx, msg) \
do {\
    char buf[256];\
    PR_snprintf(buf, sizeof(buf), "%s,%08x", op, obj);\
    if (TimingIsClockRunning(buf)) {\
        PRTime tmElapsed;\
        PRUint32 nElapsed;\
        TimingStopClock(&tmElapsed, buf);\
        LL_L2UI(nElapsed, tmElapsed);\
        TimingWriteMessage("clock,%s,%ld,%08x,%s",\
                           buf, nElapsed, cx, msg);\
    }\
} while (0)


/**
 * Use this macro to start a "clock" on a "named" operation; e.g.,
 *
 * <PRE>
 * TIMING_STARTCLOCK_NAME("http:request", "http://www.cnn.com");
 * </PRE>
 *
 * The clock should be uniquely identified by the <tt>op</tt> and
 * <tt>name</tt> parameters.
 *
 * @param op A C-string identifying the operation that is being
 * performed.
 * @param name A C-string that is the name for the timer.
 */
#define TIMING_STARTCLOCK_NAME(op, name) \
do {\
    char buf[256];\
    PR_snprintf(buf, sizeof(buf), "%s,%.64s", op, name);\
    TimingStartClock(buf);\
} while (0)


/**
 * Use this macro to stop a "clock" on a "named operation" and print out
 * a message that indicates the total elapsed time on the clock;
 * e.g.,
 *
 * <PRE>
 * TIMING_STOPCLOCK_NAME("http:request", "http://www.cnn.com", "ok");
 * </PRE>
 *
 * The <tt>op</tt> and <tt>name</tt> parameters are used to
 * identify the clock to stop.
 *
 * @param op A C-string that is the "operation" that is being
 * performed.
 * @param name A C-string that is the name for the timer.
 * @param msg A message to include in the log entry.
 */
#define TIMING_STOPCLOCK_NAME(op, name, cx, msg) \
do {\
    char buf[256];\
    PR_snprintf(buf, sizeof(buf), "%s,%.64s", op, name);\
    if (TimingIsClockRunning(buf)) {\
        PRTime tmElapsed;\
        PRUint32 nElapsed;\
        TimingStopClock(&tmElapsed, buf);\
        LL_L2UI(nElapsed, tmElapsed);\
        TimingWriteMessage("clock,%s,%ld,%08x,%s",\
                           buf, nElapsed, cx, msg);\
    }\
} while (0)

#endif /* !defined(NO_TIMING) */

#endif /* timing_h__ */

