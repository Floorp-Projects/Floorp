/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.javascript;

import java.util.Date;
import java.util.TimeZone;
import java.util.Locale;
import java.text.NumberFormat;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.lang.reflect.Method;

/**
 * This class implements the Date native object.
 * See ECMA 15.9.
 * @author Mike McCabe
 */
public class NativeDate extends ScriptableObject {

    public static void finishInit(Scriptable scope, FunctionObject ctor,
                                  Scriptable proto)
        throws PropertyException
    {
        // initialization of cached values moved to static initializers.

        // alias toUTCString to toGMTString
        ((ScriptableObject) proto).defineProperty("toGMTString",
            proto.get("toUTCString", proto),
            ScriptableObject.DONTENUM);

        // Set some method length values.
        // These functions need to be varargs, because their behavior
        // depends on the number of arguments they are called with.
        String[] specialLengthNames = { "setSeconds", "setUTCSeconds",
                                        "setMinutes", "setUTCMinutes",
                                        "setHours", "setUTCHours",
                                        "setMonth", "setUTCMonth",
                                        "setFullYear", "setUTCFullYear",
                                      };

        short[] specialLengthValues = { 2, 2,
                                        3, 3,
                                        4, 4,
                                        2, 2,
                                        3, 3,
                                      };

        for (int i=0; i < specialLengthNames.length; i++) {
            Object obj = proto.get(specialLengthNames[i], proto);
            ((FunctionObject) obj).setLength(specialLengthValues[i]);
        }

        // Set the value of the prototype Date to NaN ('invalid date');
        ((NativeDate)proto).date = ScriptRuntime.NaN;
    }

    public NativeDate() {
    }

    public String getClassName() {
        return "Date";
    }

    public Object getDefaultValue(Class typeHint) {
        if (typeHint == null)
            typeHint = ScriptRuntime.StringClass;
        return super.getDefaultValue(typeHint);
    }

    /* ECMA helper functions */

    private static boolean isFinite(double d) {
        if (d != d ||
            d == Double.POSITIVE_INFINITY ||
            d == Double.NEGATIVE_INFINITY)
            return false;
        else
            return true;
    }

    private static final double HalfTimeDomain = 8.64e15;
    private static final double HoursPerDay    = 24.0;
    private static final double MinutesPerHour = 60.0;
    private static final double SecondsPerMinute = 60.0;
    private static final double msPerSecond    = 1000.0;
    private static final double MinutesPerDay  = (HoursPerDay * MinutesPerHour);
    private static final double SecondsPerDay  = (MinutesPerDay * SecondsPerMinute);
    private static final double SecondsPerHour = (MinutesPerHour * SecondsPerMinute);
    private static final double msPerDay       = (SecondsPerDay * msPerSecond);
    private static final double msPerHour      = (SecondsPerHour * msPerSecond);
    private static final double msPerMinute    = (SecondsPerMinute * msPerSecond);

    private static double Day(double t) {
        return Math.floor(t / msPerDay);
    }

    private static double TimeWithinDay(double t) {
        double result;
        result = t % msPerDay;
        if (result < 0)
            result += msPerDay;
        return result;
    }

    private static int DaysInYear(int y) {
        if (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0))
            return 366;
        else
            return 365;
    }


    /* math here has to be f.p, because we need
     *  floor((1968 - 1969) / 4) == -1
     */
    private static double DayFromYear(double y) {
        return ((365 * ((y)-1970) + Math.floor(((y)-1969)/4.0)
                 - Math.floor(((y)-1901)/100.0) + Math.floor(((y)-1601)/400.0)));
    }

    private static double TimeFromYear(double y) {
        return DayFromYear(y) * msPerDay;
    }

    private static int YearFromTime(double t) {
        int lo = (int) Math.floor((t / msPerDay) / 366) + 1970;
        int hi = (int) Math.floor((t / msPerDay) / 365) + 1970;
        int mid;

        /* above doesn't work for negative dates... */
        if (hi < lo) {
            int temp = lo;
            lo = hi;
            hi = temp;
        }

        /* Use a simple binary search algorithm to find the right
           year.  This seems like brute force... but the computation
           of hi and lo years above lands within one year of the
           correct answer for years within a thousand years of
           1970; the loop below only requires six iterations
           for year 270000. */
        while (hi > lo) {
            mid = (hi + lo) / 2;
            if (TimeFromYear(mid) > t) {
                hi = mid - 1;
            } else {
                if (TimeFromYear(mid) <= t) {
                    int temp = mid + 1;
                    if (TimeFromYear(temp) > t) {
                        return mid;
                    }
                    lo = mid + 1;
                }
            }
        }
        return lo;
    }

    private static boolean InLeapYear(double t) {
        return DaysInYear(YearFromTime(t)) == 366;
    }

    private static int DayWithinYear(double t) {
        int year = YearFromTime(t);
        return (int) (Day(t) - DayFromYear(year));
    }
    /*
     * The following array contains the day of year for the first day of
     * each month, where index 0 is January, and day 0 is January 1.
     */

    private static double DayFromMonth(int m, boolean leap) {
        double firstDay[] = {0.0, 31.0, 59.0, 90.0, 120.0, 151.0,
                             181.0, 212.0, 243.0, 273.0, 304.0, 334.0};
        double leapFirstDay[] = {0.0, 31.0, 60.0, 91.0, 121.0, 152.0,
                                 182.0, 213.0, 244.0, 274.0, 305.0, 335.0};
        if (leap)
            return leapFirstDay[m];
        else
            return firstDay[m];
    }

    private static int MonthFromTime(double t) {
        int d, step;

        d = DayWithinYear(t);

        if (d < (step = 31))
            return 0;
        step += (InLeapYear(t) ? 29 : 28);
        if (d < step)
            return 1;
        if (d < (step += 31))
            return 2;
        if (d < (step += 30))
            return 3;
        if (d < (step += 31))
            return 4;
        if (d < (step += 30))
            return 5;
        if (d < (step += 31))
            return 6;
        if (d < (step += 31))
            return 7;
        if (d < (step += 30))
            return 8;
        if (d < (step += 31))
            return 9;
        if (d < (step += 30))
            return 10;
        return 11;
    }

    private static int DateFromTime(double t) {
        int d, step, next;

        d = DayWithinYear(t);

        if (d <= (next = 30))
            return d + 1;
        step = next;
        next += (InLeapYear(t) ? 29 : 28);
        if (d <= next)
            return d - step;
        step = next;
        if (d <= (next += 31))
            return d - step;
        step = next;
        if (d <= (next += 30))
            return d - step;
        step = next;
        if (d <= (next += 31))
            return d - step;
        step = next;
        if (d <= (next += 30))
            return d - step;
        step = next;
        if (d <= (next += 31))
            return d - step;
        step = next;
        if (d <= (next += 31))
            return d - step;
        step = next;
        if (d <= (next += 30))
            return d - step;
        step = next;
        if (d <= (next += 31))
            return d - step;
        step = next;
        if (d <= (next += 30))
            return d - step;
        step = next;
        return d - step;
    }

    private static int WeekDay(double t) {
        double result;
        result = Day(t) + 4;
        result = result % 7;
        if (result < 0)
            result += 7;
        return (int) result;
    }

    private static double Now() {
        return (double) System.currentTimeMillis();
    }

    /* Should be possible to determine the need for this dynamically
     * if we go with the workaround... I'm not using it now, because I
     * can't think of any clean way to make toLocaleString() and the
     * time zone (comment) in toString match the generated string
     * values.  Currently it's wrong-but-consistent in all but the
     * most recent betas of the JRE - seems to work in 1.1.7.
     */
    private final static boolean TZO_WORKAROUND = false;
    private static double DaylightSavingTA(double t) {
        if (!TZO_WORKAROUND) {
            Date date = new Date((long) t);
            if (thisTimeZone.inDaylightTime(date))
                return msPerHour;
            else
                return 0;
        } else {
            /* Use getOffset if inDaylightTime() is broken, because it
             * seems to work acceptably.  We don't switch over to it
             * entirely, because it requires (expensive) exploded date arguments,
             * and the api makes it impossible to handle dst
             * changeovers cleanly.
             */

            // Hardcode the assumption that the changeover always
            // happens at 2:00 AM:
            t += LocalTZA + (HourFromTime(t) <= 2 ? msPerHour : 0);
            
            int year = YearFromTime(t);
            double offset = thisTimeZone.getOffset(year > 0 ? 1 : 0,
                                                   year,
                                                   MonthFromTime(t),
                                                   DateFromTime(t),
                                                   WeekDay(t),
                                                   (int)TimeWithinDay(t));
            
            if ((offset - LocalTZA) != 0)
                return msPerHour;
            else
                return 0;
            //         return offset - LocalTZA; 
        }
    }

    private static double LocalTime(double t) {
        return t + LocalTZA + DaylightSavingTA(t);
    }

    private static double internalUTC(double t) {
        return t - LocalTZA - DaylightSavingTA(t - LocalTZA);
    }

    private static int HourFromTime(double t) {
        double result;
        result = Math.floor(t / msPerHour) % HoursPerDay;
        if (result < 0)
            result += HoursPerDay;
        return (int) result;
    }

    private static int MinFromTime(double t) {
        double result;
        result = Math.floor(t / msPerMinute) % MinutesPerHour;
        if (result < 0)
            result += MinutesPerHour;
        return (int) result;
    }

    private static int SecFromTime(double t) {
        double result;
        result = Math.floor(t / msPerSecond) % SecondsPerMinute;
        if (result < 0)
            result += SecondsPerMinute;
        return (int) result;
    }

    private static int msFromTime(double t) {
        double result;
        result =  t % msPerSecond;
        if (result < 0)
            result += msPerSecond;
        return (int) result;
    }

    private static double MakeTime(double hour, double min,
                                   double sec, double ms)
    {
        return ((hour * MinutesPerHour + min) * SecondsPerMinute + sec)
            * msPerSecond + ms;
    }

    private static double MakeDay(double year, double month, double date) {
        double result;
        boolean leap;
        double yearday;
        double monthday;

        year += Math.floor(month / 12);

        month = month % 12;
        if (month < 0)
            month += 12;

        leap = (DaysInYear((int) year) == 366);

        yearday = Math.floor(TimeFromYear(year) / msPerDay);
        monthday = DayFromMonth((int) month, leap);

        result = yearday
            + monthday
            + date - 1;
        return result;
    }

    private static double MakeDate(double day, double time) {
        return day * msPerDay + time;
    }

    private static double TimeClip(double d) {
        if (d != d ||
            d == Double.POSITIVE_INFINITY ||
            d == Double.NEGATIVE_INFINITY ||
            Math.abs(d) > HalfTimeDomain)
        {
            return ScriptRuntime.NaN;
        }
        if (d > 0.0)
            return Math.floor(d + 0.);
        else
            return Math.ceil(d + 0.);
    }

    /* end of ECMA helper functions */

    /* find UTC time from given date... no 1900 correction! */
    private static double date_msecFromDate(double year, double mon,
                                            double mday, double hour,
                                            double min, double sec,
                                            double msec)
    {
        double day;
        double time;
        double result;

        day = MakeDay(year, mon, mday);
        time = MakeTime(hour, min, sec, msec);
        result = MakeDate(day, time);
        return result;
    }


    private static final int MAXARGS = 7;
    public static double jsStaticFunction_UTC(Context cx, Scriptable thisObj,
                                              Object[] args, Function funObj)
    {
        double array[] = new double[MAXARGS];
        int loop;
        double d;

        for (loop = 0; loop < MAXARGS; loop++) {
            if (loop < args.length) {
                d = ScriptRuntime.toNumber(args[loop]);
                if (d != d || Double.isInfinite(d)) {
                    return ScriptRuntime.NaN;
                }
                array[loop] = ScriptRuntime.toInteger(args[loop]);
            } else {
                array[loop] = 0;
            }
        }

        /* adjust 2-digit years into the 20th century */
        if (array[0] >= 0 && array[0] <= 99)
            array[0] += 1900;

            /* if we got a 0 for 'date' (which is out of range)
             * pretend it's a 1.  (So Date.UTC(1972, 5) works) */
        if (array[2] < 1)
            array[2] = 1;

        d = date_msecFromDate(array[0], array[1], array[2],
                              array[3], array[4], array[5], array[6]);
        d = TimeClip(d);
        return d;
        //        return new Double(d);
    }

    /*
     * Use ported code from jsdate.c rather than the locale-specific
     * date-parsing code from Java, to keep js and rhino consistent.
     * Is this the right strategy?
     */

    /* for use by date_parse */

    /* replace this with byte arrays?  Cheaper? */
    private static String wtb[] = {
        "am", "pm",
        "monday", "tuesday", "wednesday", "thursday", "friday",
        "saturday", "sunday",
        "january", "february", "march", "april", "may", "june",
        "july", "august", "september", "october", "november", "december",
        "gmt", "ut", "utc", "est", "edt", "cst", "cdt",
        "mst", "mdt", "pst", "pdt"
        /* time zone table needs to be expanded */
    };

    private static int ttb[] = {
        -1, -2, 0, 0, 0, 0, 0, 0, 0,     /* AM/PM */
        2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
        10000 + 0, 10000 + 0, 10000 + 0, /* UT/UTC */
        10000 + 5 * 60, 10000 + 4 * 60,  /* EDT */
        10000 + 6 * 60, 10000 + 5 * 60,
        10000 + 7 * 60, 10000 + 6 * 60,
        10000 + 8 * 60, 10000 + 7 * 60
    };

    /* helper for date_parse */
    private static boolean date_regionMatches(String s1, int s1off,
                                              String s2, int s2off,
                                              int count)
    {
        boolean result = false;
        /* return true if matches, otherwise, false */
        int s1len = s1.length();
        int s2len = s2.length();

        while (count > 0 && s1off < s1len && s2off < s2len) {
            if (Character.toLowerCase(s1.charAt(s1off)) !=
                Character.toLowerCase(s2.charAt(s2off)))
                break;
            s1off++;
            s2off++;
            count--;
        }

        if (count == 0) {
            result = true;
        }
        return result;
    }

    private static double date_parseString(String s) {
        double msec;

        int year = -1;
        int mon = -1;
        int mday = -1;
        int hour = -1;
        int min = -1;
        int sec = -1;
        char c = 0;
        char si = 0;
        int i = 0;
        int n = -1;
        double tzoffset = -1;
        char prevc = 0;
        int limit = 0;
        boolean seenplusminus = false;

        if (s == null)  // ??? Will s be null?
            return ScriptRuntime.NaN;
        limit = s.length();
        while (i < limit) {
            c = s.charAt(i);
            i++;
            if (c <= ' ' || c == ',' || c == '-') {
                if (i < limit) {
                    si = s.charAt(i);
                    if (c == '-' && '0' <= si && si <= '9') {
                        prevc = c;
                    }
                }
                continue;
            }
            if (c == '(') { /* comments) */
                int depth = 1;
                while (i < limit) {
                    c = s.charAt(i);
                    i++;
                    if (c == '(')
                        depth++;
                    else if (c == ')')
                        if (--depth <= 0)
                            break;
                }
                continue;
            }
            if ('0' <= c && c <= '9') {
                n = c - '0';
                while (i < limit && '0' <= (c = s.charAt(i)) && c <= '9') {
                    n = n * 10 + c - '0';
                    i++;
                }

                /* allow TZA before the year, so
                 * 'Wed Nov 05 21:49:11 GMT-0800 1997'
                 * works */

                /* uses of seenplusminus allow : in TZA, so Java
                 * no-timezone style of GMT+4:30 works
                 */
                if ((prevc == '+' || prevc == '-')/*  && year>=0 */) {
                    /* make ':' case below change tzoffset */
                    seenplusminus = true;

                    /* offset */
                    if (n < 24)
                        n = n * 60; /* EG. "GMT-3" */
                    else
                        n = n % 100 + n / 100 * 60; /* eg "GMT-0430" */
                    if (prevc == '+')       /* plus means east of GMT */
                        n = -n;
                    if (tzoffset != 0 && tzoffset != -1)
                        return ScriptRuntime.NaN;
                    tzoffset = n;
                } else if (n >= 70  ||
                           (prevc == '/' && mon >= 0 && mday >= 0 && year < 0)) {
                    if (year >= 0)
                        return ScriptRuntime.NaN;
                    else if (c <= ' ' || c == ',' || c == '/' || i >= limit)
                        year = n < 100 ? n + 1900 : n;
                    else
                        return ScriptRuntime.NaN;
                } else if (c == ':') {
                    if (hour < 0)
                        hour = /*byte*/ n;
                    else if (min < 0)
                        min = /*byte*/ n;
                    else
                        return ScriptRuntime.NaN;
                } else if (c == '/') {
                    if (mon < 0)
                        mon = /*byte*/ n-1;
                    else if (mday < 0)
                        mday = /*byte*/ n;
                    else
                        return ScriptRuntime.NaN;
                } else if (i < limit && c != ',' && c > ' ' && c != '-') {
                    return ScriptRuntime.NaN;
                } else if (seenplusminus && n < 60) {  /* handle GMT-3:30 */
                    if (tzoffset < 0)
                        tzoffset -= n;
                    else
                        tzoffset += n;
                } else if (hour >= 0 && min < 0) {
                    min = /*byte*/ n;
                } else if (min >= 0 && sec < 0) {
                    sec = /*byte*/ n;
                } else if (mday < 0) {
                    mday = /*byte*/ n;
                } else {
                    return ScriptRuntime.NaN;
                }
                prevc = 0;
            } else if (c == '/' || c == ':' || c == '+' || c == '-') {
                prevc = c;
            } else {
                int st = i - 1;
                int k;
                while (i < limit) {
                    c = s.charAt(i);
                    if (!(('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z')))
                        break;
                    i++;
                }
                if (i <= st + 1)
                    return ScriptRuntime.NaN;
                for (k = wtb.length; --k >= 0;)
                    if (date_regionMatches(wtb[k], 0, s, st, i-st)) {
                        int action = ttb[k];
                        if (action != 0) {
                            if (action < 0) {
                                /*
                                 * AM/PM. Count 12:30 AM as 00:30, 12:30 PM as
                                 * 12:30, instead of blindly adding 12 if PM.
                                 */
                                if (hour > 12 || hour < 0) {
                                    return ScriptRuntime.NaN;
                                } else {
                                    if (action == -1 && hour == 12) { // am
                                        hour = 0;
                                    } else if (action == -2 && hour != 12) {// pm
                                        hour += 12;
                                    }
                                }
                            } else if (action <= 13) { /* month! */
                                if (mon < 0) {
                                    mon = /*byte*/ (action - 2);
                                } else {
                                    return ScriptRuntime.NaN;
                                }
                            } else {
                                tzoffset = action - 10000;
                            }
                        }
                        break;
                    }
                if (k < 0)
                    return ScriptRuntime.NaN;
                prevc = 0;
            }
        }
        if (year < 0 || mon < 0 || mday < 0)
            return ScriptRuntime.NaN;
        if (sec < 0)
            sec = 0;
        if (min < 0)
            min = 0;
        if (hour < 0)
            hour = 0;
        if (tzoffset == -1) { /* no time zone specified, have to use local */
            double time;
            time = date_msecFromDate(year, mon, mday, hour, min, sec, 0);
            return internalUTC(time);
        }

        msec = date_msecFromDate(year, mon, mday, hour, min, sec, 0);
        msec += tzoffset * msPerMinute;
        return msec;
    }

    public static double jsStaticFunction_parse(String s) {
        return date_parseString(s);
    }

    private static String date_format(double t) {
        StringBuffer result = new StringBuffer(60);
        double local = LocalTime(t);

        /* offset from GMT in minutes.  The offset includes daylight savings,
           if it applies. */
        int minutes = (int) Math.floor((LocalTZA + DaylightSavingTA(t))
                                       / msPerMinute);
        /* map 510 minutes to 0830 hours */
        int offset = (minutes / 60) * 100 + minutes % 60;

        String dateStr = Integer.toString(DateFromTime(local));
        String hourStr = Integer.toString(HourFromTime(local));
        String minStr = Integer.toString(MinFromTime(local));
        String secStr = Integer.toString(SecFromTime(local));
        String offsetStr = Integer.toString(offset > 0 ? offset : -offset);
        int year = YearFromTime(local);
        String yearStr = Integer.toString(year > 0 ? year : -year);

        result.append(days[WeekDay(local)]);
        result.append(" ");
        result.append(months[MonthFromTime(local)]);
        if (dateStr.length() == 1)
            result.append(" 0");
        else
            result.append(" ");
        result.append(dateStr);
        if (hourStr.length() == 1)
            result.append(" 0");
        else
            result.append(" ");
        result.append(hourStr);
        if (minStr.length() == 1)
            result.append(":0");
        else
            result.append(":");
        result.append(minStr);
        if (secStr.length() == 1)
            result.append(":0");
        else
            result.append(":");
        result.append(secStr);
        if (offset > 0)
            result.append(" GMT+");
        else
            result.append(" GMT-");
        int i;
        for (i = offsetStr.length(); i < 4; i++)
            result.append("0");
        result.append(offsetStr);

        // ask Java for a time zone string, if we've been able to find
        // a formatter for it.
        if (timeZoneFormatter != null) {
            result.append(" (");
            java.util.Date date = new Date((long) t);
            result.append(timeZoneFormatter.format(date));
            result.append(") ");
        } else {
            result.append(" ");
        }

        if (year < 0)
            result.append("-");
        for (i = yearStr.length(); i < 4; i++)
            result.append("0");
        result.append(yearStr);
        return result.toString();
    }

    /* the javascript constructor */
    public static Object js_Date(Context cx, Object[] args, Function ctorObj,
                                 boolean inNewExpr)
    {
        // if called as a function, just return a string
        // representing the current time.
        if (!inNewExpr)
            return date_format(Now());

        NativeDate obj = new NativeDate();

        // if called as a constructor with no args,
        // return a new Date with the current time.
        if (args.length == 0) {
            obj.date = Now();
            return obj;
        }

        // if called with just one arg -
        if (args.length == 1) {
            double date;
            // if it's not a string, use it as a millisecond date
            if (!(args[0] instanceof String)) {
                date = ScriptRuntime.toNumber(args[0]);
            } else {
            // it's a string; parse it.
                String str = (String) args[0];
                date = date_parseString(str);
            }
            obj.date = TimeClip(date);
            return obj;
        }

        // multiple arguments; year, month, day etc.
        double array[] = new double[MAXARGS];
        int loop;
        double d;

        for (loop = 0; loop < MAXARGS; loop++) {
            if (loop < args.length) {
                d = ScriptRuntime.toNumber(args[loop]);

                if (d != d || Double.isInfinite(d)) {
                    obj.date = ScriptRuntime.NaN;
                    return obj;
                }
                array[loop] = ScriptRuntime.toInteger(args[loop]);
            } else {
                array[loop] = 0;
            }
        }

        /* adjust 2-digit years into the 20th century */
        if (array[0] >= 0 && array[0] <= 99)
            array[0] += 1900;

        /* if we got a 0 for 'date' (which is out of range)
         * pretend it's a 1 */
        if (array[2] < 1)
            array[2] = 1;

        double day = MakeDay(array[0], array[1], array[2]);
        double time = MakeTime(array[3], array[4], array[5], array[6]);
        time = MakeDate(day, time);
        time = internalUTC(time);
        obj.date = TimeClip(time);

        return obj;
    }

    /* constants for toString, toUTCString */
    private static String js_NaN_date_str = "Invalid Date";

    private static String[] days = {
        "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
    };

    private static String[] months = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    public String js_toString() {
        /* all for want of printf.  All of this garbage seems necessary
         * because Java carefully avoids providing non-localized
         * string formatting.  We need to avoid localization to ensure
         * that we generate parseable output.
         */
        if (this.date != this.date)
            return js_NaN_date_str;

        return date_format(this.date);
    }

    public String js_toLocaleString() {
        if (this.date != this.date)
            return js_NaN_date_str;

        // the string this returns doesn't have much resemblance
        // to what jsdate.c returns... does this matter?

        java.util.Date tempdate = new Date((long) this.date);
        return localeDateFormatter.format(tempdate);
    }

    public String js_toUTCString() {
        if (this.date != this.date)
            return js_NaN_date_str;

        StringBuffer result = new StringBuffer(60);

        String dateStr = Integer.toString(DateFromTime(this.date));
        String hourStr = Integer.toString(HourFromTime(this.date));
        String minStr = Integer.toString(MinFromTime(this.date));
        String secStr = Integer.toString(SecFromTime(this.date));
        int year = YearFromTime(this.date);
        String yearStr = Integer.toString(year > 0 ? year : -year);

        result.append(days[WeekDay(this.date)]);
        result.append(", ");
        if (dateStr.length() == 1)
            result.append("0");
        result.append(dateStr);
        result.append(" ");
        result.append(months[MonthFromTime(this.date)]);
        if (year < 0)
            result.append(" -");
        else
            result.append(" ");
        int i;
        for (i = yearStr.length(); i < 4; i++)
            result.append("0");
        result.append(yearStr);

        if (hourStr.length() == 1)
            result.append(" 0");
        else
            result.append(" ");
        result.append(hourStr);
        if (minStr.length() == 1)
            result.append(":0");
        else
            result.append(":");
        result.append(minStr);
        if (secStr.length() == 1)
            result.append(":0");
        else
            result.append(":");
        result.append(secStr);

        result.append(" GMT");
        return result.toString();
    }

    public double js_valueOf() {
        return this.date;
    }

    public double jsFunction_getTime() {
        return this.date;
    }

    public double jsFunction_getYear() {
        if (this.date != this.date)
            return this.date;

        int result = YearFromTime(LocalTime(this.date));

        /*
         * During the great date rewrite of 1.3, we tried to track the
         * evolving ECMA standard, which then had a definition of
         * getYear which always subtracted 1900.  Which we
         * implemented, not realizing that it was incompatible with
         * the old behavior...  now, rather than thrash the behavior
         * yet again, we've decided to leave it with the - 1900
         * behavior and point people to the getFullYear method.  But
         * we try to protect existing scripts that have specified a
         * version...
         */
        Context cx = Context.getContext();
        int version = cx.getLanguageVersion();
        if (version == Context.VERSION_1_0 ||
            version == Context.VERSION_1_1 ||
            version == Context.VERSION_1_2)
        {
            if (result >= 1900 && result < 2000)
                result -= 1900;
        } else {
            result -= 1900;
        }
        return result;
    }

    public double jsFunction_getFullYear() {
        if (this.date != this.date)
            return this.date;
        return YearFromTime(LocalTime(this.date));
    }

    public double jsFunction_getUTCFullYear() {
        if (this.date != this.date)
            return this.date;
        return YearFromTime(this.date);
    }

    public double jsFunction_getMonth() {
        if (this.date != this.date)
            return this.date;
        return MonthFromTime(LocalTime(this.date));
    }

    public double jsFunction_getUTCMonth() {
        if (this.date != this.date)
            return this.date;
        return MonthFromTime(this.date);
    }

    public double jsFunction_getDate() {
        if (this.date != this.date)
            return this.date;
        return DateFromTime(LocalTime(this.date));
    }

    public double jsFunction_getUTCDate() {
        if (this.date != this.date)
            return this.date;
        return DateFromTime(this.date);
    }

    public double jsFunction_getDay() {
        if (this.date != this.date)
            return this.date;
        return WeekDay(LocalTime(this.date));
    }

    public double jsFunction_getUTCDay() {
        if (this.date != this.date)
            return this.date;
        return WeekDay(this.date);
    }

    public double jsFunction_getHours() {
        if (this.date != this.date)
            return this.date;
        return HourFromTime(LocalTime(this.date));
    }

    public double jsFunction_getUTCHours() {
        if (this.date != this.date)
            return this.date;
        return HourFromTime(this.date);
    }

    public double jsFunction_getMinutes() {
        if (this.date != this.date)
            return this.date;
        return MinFromTime(LocalTime(this.date));
    }

    public double jsFunction_getUTCMinutes() {
        if (this.date != this.date)
            return this.date;
        return MinFromTime(this.date);
    }

    public double jsFunction_getSeconds() {
        if (this.date != this.date)
            return this.date;
        return SecFromTime(LocalTime(this.date));
    }

    public double jsFunction_getUTCSeconds() {
        if (this.date != this.date)
            return this.date;
        return SecFromTime(this.date);
    }

    public double jsFunction_getMilliseconds() {
        if (this.date != this.date)
            return this.date;
        return msFromTime(LocalTime(this.date));
    }

    public double jsFunction_getUTCMilliseconds() {
        if (this.date != this.date)
            return this.date;
        return msFromTime(this.date);
    }

    public double jsFunction_getTimezoneOffset() {
        if (this.date != this.date)
            return this.date;
        return (this.date - LocalTime(this.date)) / msPerMinute;
    }

    public double jsFunction_setTime(double time) {
        this.date = TimeClip(time);
        return this.date;
    }
    
    private static NativeDate checkInstance(Scriptable obj,
                                            Function funObj) {
        if (obj == null || !(obj instanceof NativeDate)) {
            Object[] args = { ((NativeFunction) funObj).names[0] };
            throw Context.reportRuntimeError(
                Context.getMessage("msg.incompat.call", args));
        }
        return (NativeDate) obj;
    }
   
    private static double makeTime(Scriptable dateObj, Object[] args,
                                   int maxargs, boolean local,
                                   Function funObj)
    {
        int i;
        double conv[] = new double[4];
        double hour, min, sec, msec;
        double lorutime; /* Local or UTC version of date */

        double time;
        double result;

        NativeDate d = checkInstance(dateObj, funObj);
        double date = d.date;

        /* just return NaN if the date is already NaN */
        if (date != date)
            return date;

        /* Satisfy the ECMA rule that if a function is called with
         * fewer arguments than the specified formal arguments, the
         * remaining arguments are set to undefined.  Seems like all
         * the Date.setWhatever functions in ECMA are only varargs
         * beyond the first argument; this should be set to undefined
         * if it's not given.  This means that "d = new Date();
         * d.setMilliseconds()" returns NaN.  Blech.
         */
        if (args.length == 0)
            args = ScriptRuntime.padArguments(args, 1);

        for (i = 0; i < args.length && i < maxargs; i++) {
            conv[i] = ScriptRuntime.toNumber(args[i]);

            // limit checks that happen in MakeTime in ECMA.
            if (conv[i] != conv[i] || Double.isInfinite(conv[i])) {
                d.date = ScriptRuntime.NaN;
                return d.date;
            }
            conv[i] = ScriptRuntime.toInteger(conv[i]);
        }

        if (local)
            lorutime = LocalTime(date);
        else
            lorutime = date;

        i = 0;
        int stop = args.length;

        if (maxargs >= 4 && i < stop)
            hour = conv[i++];
        else
            hour = HourFromTime(lorutime);

        if (maxargs >= 3 && i < stop)
            min = conv[i++];
        else
            min = MinFromTime(lorutime);

        if (maxargs >= 2 && i < stop)
            sec = conv[i++];
        else
            sec = SecFromTime(lorutime);

        if (maxargs >= 1 && i < stop)
            msec = conv[i++];
        else
            msec = msFromTime(lorutime);

        time = MakeTime(hour, min, sec, msec);
        result = MakeDate(Day(lorutime), time);

        if (local)
            result = internalUTC(result);
        date = TimeClip(result);

        d.date = date;
        return date;
    }

    public static double jsFunction_setMilliseconds(Context cx,
                                                    Scriptable thisObj,
                                                    Object[] args,
                                                    Function funObj)
    {
        return makeTime(thisObj, args, 1, true, funObj);
    }

    public static double jsFunction_setUTCMilliseconds(Context cx,
                                                       Scriptable thisObj,
                                                       Object[] args,
                                                       Function funObj)
    {
        return makeTime(thisObj, args, 1, false, funObj);
    }

    public static double jsFunction_setSeconds(Context cx,
                                               Scriptable thisObj,
                                               Object[] args,
                                               Function funObj)
    {
        return makeTime(thisObj, args, 2, true, funObj);
    }

    public static double jsFunction_setUTCSeconds(Context cx,
                                                  Scriptable thisObj,
                                                  Object[] args,
                                                  Function funObj)
    {
        return makeTime(thisObj, args, 2, false, funObj);
    }

    public static double jsFunction_setMinutes(Context cx,
                                               Scriptable thisObj,
                                               Object[] args,
                                               Function funObj)
    {
        return makeTime(thisObj, args, 3, true, funObj);
    }

    public static double jsFunction_setUTCMinutes(Context cx,
                                                  Scriptable thisObj,
                                                  Object[] args,
                                                  Function funObj)
    {
        return makeTime(thisObj, args, 3, false, funObj);
    }

    public static double jsFunction_setHours(Context cx,
                                             Scriptable thisObj,
                                             Object[] args,
                                             Function funObj)
    {
        return makeTime(thisObj, args, 4, true, funObj);
    }

    public static double jsFunction_setUTCHours(Context cx,
                                                Scriptable thisObj,
                                                Object[] args,
                                                Function funObj)
    {
        return makeTime(thisObj, args, 4, false, funObj);
    }

    private static double makeDate(Scriptable dateObj, Object[] args,
                                   int maxargs, boolean local,
                                   Function funObj)
    {
        int i;
        double conv[] = new double[3];
        double year, month, day;
        double lorutime; /* local or UTC version of date */
        double result;

        NativeDate d = checkInstance(dateObj, funObj);
        double date = d.date;

        /* See arg padding comment in makeTime.*/
        if (args.length == 0)
            args = ScriptRuntime.padArguments(args, 1);

        for (i = 0; i < args.length && i < maxargs; i++) {
            conv[i] = ScriptRuntime.toNumber(args[i]);

            // limit checks that happen in MakeDate in ECMA.
            if (conv[i] != conv[i] || Double.isInfinite(conv[i])) {
                d.date = ScriptRuntime.NaN;
                return d.date;
            }
            conv[i] = ScriptRuntime.toInteger(conv[i]);
        }

        /* return NaN if date is NaN and we're not setting the year,
         * If we are, use 0 as the time. */
        if (date != date) {
            if (args.length < 3) {
                return ScriptRuntime.NaN;
            } else {
                lorutime = 0;
            }
        } else {
            if (local)
                lorutime = LocalTime(date);
            else
                lorutime = date;
        }

        i = 0;
        int stop = args.length;

        if (maxargs >= 3 && i < stop)
            year = conv[i++];
        else
            year = YearFromTime(lorutime);

        if (maxargs >= 2 && i < stop)
            month = conv[i++];
        else
            month = MonthFromTime(lorutime);

        if (maxargs >= 1 && i < stop)
            day = conv[i++];
        else
            day = DateFromTime(lorutime);

        day = MakeDay(year, month, day); /* day within year */
        result = MakeDate(day, TimeWithinDay(lorutime));

        if (local)
            result = internalUTC(result);

        date = TimeClip(result);

        d.date = date;
        return date;
    }

    public static double jsFunction_setDate(Context cx,
                                            Scriptable thisObj,
                                            Object[] args,
                                            Function funObj)
    {
        return makeDate(thisObj, args, 1, true, funObj);
    }

    public static double jsFunction_setUTCDate(Context cx,
                                               Scriptable thisObj,
                                               Object[] args,
                                               Function funObj)
    {
        return makeDate(thisObj, args, 1, false, funObj);
    }

    public static double jsFunction_setMonth(Context cx,
                                             Scriptable thisObj,
                                             Object[] args,
                                             Function funObj)
    {
        return makeDate(thisObj, args, 2, true, funObj);
    }

    public static double jsFunction_setUTCMonth(Context cx,
                                                Scriptable thisObj,
                                                Object[] args,
                                                Function funObj)
    {
        return makeDate(thisObj, args, 2, false, funObj);
    }

    public static double jsFunction_setFullYear(Context cx,
                                                Scriptable thisObj,
                                                Object[] args,
                                                Function funObj)
    {
        return makeDate(thisObj, args, 3, true, funObj);
    }

    public static double jsFunction_setUTCFullYear(Context cx,
                                                   Scriptable thisObj,
                                                   Object[] args,
                                                   Function funObj)
    {
        return makeDate(thisObj, args, 3, false, funObj);
    }

    public double jsFunction_setYear(double year) {
        double day, result;
        if (year != year || Double.isInfinite(year)) {
            this.date = ScriptRuntime.NaN;
            return this.date;
        }

        if (this.date != this.date)
            this.date = 0;

        if (year >= 0 && year <= 99)
            year += 1900;

        day = MakeDay(year, MonthFromTime(this.date), DateFromTime(this.date));
        result = MakeDate(day, TimeWithinDay(this.date));
        result = internalUTC(result);

        this.date = TimeClip(result);
        return this.date;
    }

    /* cached values */
    private static final java.util.TimeZone thisTimeZone
        = java.util.TimeZone.getDefault();
    private static final double LocalTZA
        = thisTimeZone.getRawOffset();
    private static java.text.DateFormat timeZoneFormatter;
    static {
        // Support Personal Java, which lacks java.text.SimpleDateFormat.
        try {
            Class paramz[] = { String.class };
            Object argz[] = { "zzz" };
            Class clazz = Class.forName("java.text.SimpleDateFormat");
            java.lang.reflect.Constructor conztruct =
                clazz.getDeclaredConstructor(paramz);
            timeZoneFormatter =
                (java.text.DateFormat) conztruct.newInstance(argz);
        } catch (Exception e) {
            // Do it this (slightly unsafe) way for brevity rather
            // than catching against 7 (!) different (and disjoint)
            // exception types.
            timeZoneFormatter = null;
        }
    }

    private static final java.text.DateFormat localeDateFormatter
        = DateFormat.getDateTimeInstance(DateFormat.LONG,
                                         DateFormat.LONG);

    private double date;
}


