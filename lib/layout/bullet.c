/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "xp.h"
#include "layout.h"

#ifdef PROFILE
#pragma profile on
#endif


/*
 * For an ordered list, convert a numeric value to alphabetical.
 *
 * Values between 1 and 26 are handled in the obvious way (a..z).
 * Higher values are shown as aa, ab, ac, ..., az, ba, bb, ..., bz, ...,
 *    za, zb, ..., zz, aaa, aab, aac, ...
 *
 * Algorithm:
 *   1. Convert the value to a little-endian base 26 bignum in the
 *      str[] array;
 *   2. Create str2[] by reversing str[] and adding 'a' or 'A' to each
 *      element, and appending '.' to the resulting string.
 *
 * Parameters:
 *   value   The numeric value to be converted;
 *   large   FALSE for lower case; TRUE for upper case.
 *   len_ptr Result parameter to receive the length of the returned string.
 *
 * Returns:
 *   PA_Block containing the converted string.
 *
 * Documented by:
 *   Pete Bevin <moose@bestiary.com>
 */
PA_Block
lo_ValueToAlpha(int32 value, Bool large, intn *len_ptr)
{
    int i;                      /* ACME integers, inc. */
    char str[20];               /* Little-endian base 26 bignum */
    char str2[22];              /* Temp array for  converted string */
    intn pos;                   /* Current position during increment loop */
    intn cnt;                   /* Current digit value during inc. loop*/
    PA_Block buff;              /* PA_Block for return value */
	char *bptr;
	char base;

	*len_ptr = 0;


    if (large != FALSE)         /* Pick 'a' or 'A' as the base value */
	{
		base = 'A';
	}
	else
	{
		base = 'a';
	}

    for (i=0; i<20; i++)        /* Base 26 number is initially 0 */
	{
		str[i] = (char)0;
	}

    /*
     * As long as value > 26, keep adding 26 to the bignum, and subtracting
     * it from value.  At the end, just write the remainder into the first
     * element.
     *
     * To add 26 to a base-26 number, just ignore the low-order digit and
     * add 1 to the rest.
     *
     * Yes, this is inefficient.  No, it doesn't matter.  In fact, the
     * contents of this loop are hardly ever executed anyway.
     */
	while (value > 26)
	{
        pos = 1;                /* Start at the second digit... */
        str[pos] = (char)((int)str[pos] + 1); /* Add one. */
		cnt = (int)str[pos];
        /*
         * Now handle the carry digits.  If you're looking at a 27, you
         * have to change it to 0 and tick the next digit.  Keep going
         * until you don't need to carry any more.
         */
		while ((cnt > 26)&&(pos < 19))
		{
			str[pos] = (char)0;
			pos++;
			str[pos] = (char)((int)str[pos] + 1);
			cnt = (int)str[pos];
		}
        /*
         * This is the case you'll never reach, where the counter wraps
         * around.
         */
		if ((pos == 20)&&(cnt > 26))
		{
			str[pos] = (char)0;
		}
		value -= 26;
	}
    /* Add on the remainder. */
	str[0] = (char)value;

    /* How many digits are used? */
	pos = 0;
	while ((int)str[pos] != 0)
	{
		pos++;
	}

    /*
     * Special case: value was equal to zero.  Not sure this can ever
     * happen -- <ol start=0> won't make it so, since lo_setup_list
     * corrects it to start=1.
     */
	if (pos == 0)
	{
		XP_STRCPY(str2, " .");
	}
	else
	{
        /*
         * Reverse the list into str2[], and add base (either 'a' or 'A')
         * to each element.
         */
		cnt = 0;
		for (i=pos; i>0; i--)
		{
			str2[cnt] = (char)(base + (int)str[i - 1] - 1);
			cnt++;
		}
        str2[cnt] = '.';        /* Add a period... */
        str2[cnt + 1] = '\0';   /* ...and null terminate. */
	}

    /* Set the len_ptr result parameter */
	*len_ptr = XP_STRLEN(str2);

    /* Copy out the string. */
	buff = PA_ALLOC(*len_ptr + 1);
	if (buff != NULL)
	{
		PA_LOCK(bptr, char *, buff);
		XP_STRCPY(bptr, str2);
		PA_UNLOCK(buff);
	}

	return(buff);
}


static char Fives[2][5] = {"vld ", "VLD "};
static char Ones[2][5] = {"ixcm", "IXCM"};

/*
 * For an ordered list, convert a numeric value to roman numerals.
 * This works for numbers between 1 and 3,999 only -- 4,000 would be
 * impossible because there doesn't appear to be a numeral for 5,000.
 *
 * Algorithm:
 *   1. Split the number into digits -- e.g., 1982 => 1,9,8,2.
 *   2. Convert each digit according to its position:
 *      a. 0 becomes " ";
 *      b. Numbers 5..8 are a digit from Fives[] followed by (number-5);
 *      c. Numbers 1..3 become that many digits from Ones[];
 *      d. Number 4 becomes a digit from Ones[] followed by one from Fives[];
 *      e. Number 9 becomes the current Ones[] digit followed by the
 *         next Ones[] digit.
 *   3. Collate all the converted digits together.
 *
 * Example:
 *   1. 1982 => 1,9,8,2
 *   2.     1 => m (rule c)
 *          9 => cm (rule e)
 *          8 => lxxx (rule b)
 *          2 => ii (rule c)
 *   3. Resulting string is mcmlxxxii
 *
 *   In reality, the conversion is done the other way: 1982 => 2,8,9,1.
 *
 * Parameters:
 *   value   The numeric value to be converted;
 *   large   FALSE for lower case; TRUE for upper case.
 *   len_ptr Result parameter to receive the length of the returned string.
 *
 * Returns:
 *   PA_Block containing the converted string.
 *
 * Documented by:
 *   Pete Bevin <moose@bestiary.com>
 */
PA_Block
lo_ValueToRoman(int32 value, Bool large, intn *len_ptr)
{
    int i, j;                   /* ACME integers, inc. */
    int indx[4];                /* Digits of value, in little-endian order */
    char str[4][6];             /* Up to 6 numerals for each digit. */
    char *fives;                /* Which Fives[] (upper or lower) to use */
    char *ones;                 /* Which Ones[] (upper or lower) to use */
    char str2[22];              /* Collated string */
    char *ptr;                  /* Temporary string pointer */
	PA_Block buff;
	char *bptr;

    *len_ptr = 0;               /* ??? */

    /* Select upper- or lower-case */
	if (large != FALSE)
	{
		fives = Fives[1];
		ones = Ones[1];
	}
	else
	{
		fives = Fives[0];
		ones = Ones[0];
	}

    /*
     * Wrap around if the list is very long.  Not pretty, but you're unlikely
     * to hit this...
     */
	if (value >= 4000)
	{
		value = value % 3999;
		value++;
	}

    /* Split 1928 into 8,2,9,1 in indx[] array */
	for (i=0; i<4; i++)
	{
		indx[i] = (int) value % 10;
		value  = value / 10;
	}

    /* Convert each digit to Roman numerals. */
	for (i=0; i<4; i++)
	{
        if (indx[i] >= 5)       /* Cases 5..9 */
		{
			indx[i] -= 5;
			str[i][0] = fives[i];
		}
		else
		{
			str[i][0] = ' ';
		}

		if (indx[i] == 4)
		{
            if (str[i][0] == ' ') /* Case 4 */
			{
				str[i][1] = fives[i];
			}
			else
			{
                str[i][1] = ones[i + 1]; /* Case 9 */
			}
			str[i][0] = ones[i];
			str[i][2] = '\0';
		}
        else                    /* Cases 0,1,2,3,5,6,7,8 */
		{
			for (j=0; j<indx[i]; j++)
			{
                /* Add 0, 1, 2 or 3 ones */
				str[i][j + 1] = ones[i];
			}
			str[i][indx[i] + 1] = '\0';
		}
	}

    /* Now collate the numerals from most- to least- significant. */
	XP_STRCPY(str2, "");
	for (i=3; i>=0; i--)
	{
		ptr = str[i];
		if (*ptr == ' ')
		{
			ptr++;
		}
		XP_STRCAT(str2, ptr);
	}
	XP_STRCAT(str2, ".");

    /* All done.  Set len_ptr and copy out the string. */
	*len_ptr = XP_STRLEN(str2);

	buff = PA_ALLOC(*len_ptr + 1);
	if (buff != NULL)
	{
		PA_LOCK(bptr, char *, buff);
		XP_STRCPY(bptr, str2);
		PA_UNLOCK(buff);
	}

	return(buff);
}

#ifdef PROFILE
#pragma profile off
#endif
