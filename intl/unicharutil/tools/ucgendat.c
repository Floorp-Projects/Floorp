/*
 * Copyright 1996, 1997, 1998 Computing Research Labs,
 * New Mexico State University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COMPUTING RESEARCH LAB OR NEW MEXICO STATE UNIVERSITY BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT
 * OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef lint
#ifdef __GNUC__
static char rcsid[] __attribute__ ((unused)) = "$Id: ucgendat.c,v 1.1 1999/01/08 00:19:21 ftang%netscape.com Exp $";
#else
static char rcsid[] = "$Id: ucgendat.c,v 1.1 1999/01/08 00:19:21 ftang%netscape.com Exp $";
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WIN32
#include <unistd.h>
#endif

#define ishdigit(cc) (((cc) >= '0' && (cc) <= '9') ||\
                      ((cc) >= 'A' && (cc) <= 'F') ||\
                      ((cc) >= 'a' && (cc) <= 'f'))

/*
 * A header written to the output file with the byte-order-mark and the number
 * of property nodes.
 */
static unsigned short hdr[2] = {0xfeff, 0};

#define NUMPROPS 49
#define NEEDPROPS (NUMPROPS + (4 - (NUMPROPS & 3)))

typedef struct {
    char *name;
    int len;
} _prop_t;

/*
 * List of properties expected to be found in the Unicode Character Database
 * including some implementation specific properties.
 *
 * The implementation specific properties are:
 * Cm = Composed (can be decomposed)
 * Nb = Non-breaking
 * Sy = Symmetric (has left and right forms)
 * Hd = Hex digit
 * Qm = Quote marks
 * Mr = Mirroring
 * Ss = Space, other
 * Cp = Defined character
 */
static _prop_t props[NUMPROPS] = {
    {"Mn", 2}, {"Mc", 2}, {"Me", 2}, {"Nd", 2}, {"Nl", 2}, {"No", 2},
    {"Zs", 2}, {"Zl", 2}, {"Zp", 2}, {"Cc", 2}, {"Cf", 2}, {"Cs", 2},
    {"Co", 2}, {"Cn", 2}, {"Lu", 2}, {"Ll", 2}, {"Lt", 2}, {"Lm", 2},
    {"Lo", 2}, {"Pc", 2}, {"Pd", 2}, {"Ps", 2}, {"Pe", 2}, {"Po", 2},
    {"Sm", 2}, {"Sc", 2}, {"Sk", 2}, {"So", 2}, {"L",  1}, {"R",  1},
    {"EN", 2}, {"ES", 2}, {"ET", 2}, {"AN", 2}, {"CS", 2}, {"B",  1},
    {"S",  1}, {"WS", 2}, {"ON", 2},
    {"Cm", 2}, {"Nb", 2}, {"Sy", 2}, {"Hd", 2}, {"Qm", 2}, {"Mr", 2},
    {"Ss", 2}, {"Cp", 2}, {"Pi", 2}, {"Pf", 2}
};

typedef struct {
    unsigned long *ranges;
    unsigned short used;
    unsigned short size;
} _ranges_t;

static _ranges_t proptbl[NUMPROPS];

/*
 * Make sure this array is sized to be on a 4-byte boundary at compile time.
 */
static unsigned short propcnt[NEEDPROPS];

/*
 * Array used to collect a decomposition before adding it to the decomposition
 * table.
 */
static unsigned long dectmp[64];
static unsigned long dectmp_size;

typedef struct {
    unsigned long code;
    unsigned short size;
    unsigned short used;
    unsigned long *decomp;
} _decomp_t;

/*
 * List of decomposition.  Created and expanded in order as the characters are
 * encountered.
 */
static _decomp_t *decomps;
static unsigned long decomps_used;
static unsigned long decomps_size;

/*
 * Types and lists for handling lists of case mappings.
 */
typedef struct {
    unsigned long key;
    unsigned long other1;
    unsigned long other2;
} _case_t;

static _case_t *upper;
static _case_t *lower;
static _case_t *title;
static unsigned long upper_used;
static unsigned long upper_size;
static unsigned long lower_used;
static unsigned long lower_size;
static unsigned long title_used;
static unsigned long title_size;

/*
 * Array used to collect case mappings before adding them to a list.
 */
static unsigned long cases[3];

/*
 * An array to hold ranges for combining classes.
 */
static unsigned long *ccl;
static unsigned long ccl_used;
static unsigned long ccl_size;

/*
 * Structures for handling numbers.
 */
typedef struct {
    unsigned long code;
    unsigned long idx;
} _codeidx_t;

typedef struct {
    short numerator;
    short denominator;
} _num_t;

/*
 * Arrays to hold the mapping of codes to numbers.
 */
static _codeidx_t *ncodes;
static unsigned long ncodes_used;
static unsigned long ncodes_size;

static _num_t *nums;
static unsigned long nums_used;
static unsigned long nums_size;

/*
 * Array for holding numbers.
 */
static _num_t *nums;
static unsigned long nums_used;
static unsigned long nums_size;

static void
#ifdef __STDC__
add_range(unsigned long start, unsigned long end, char *p1, char *p2)
#else
add_range(start, end, p1, p2)
unsigned long start, end;
char *p1, *p2;
#endif
{
    int i, j, k, len;
    _ranges_t *rlp;
    char *name;

    for (k = 0; k < 2; k++) {
        if (k == 0) {
            name = p1;
            len = 2;
        } else {
            if (p2 == 0)
              break;

            name = p2;
            len = 1;
        }

        for (i = 0; i < NUMPROPS; i++) {
            if (props[i].len == len && memcmp(props[i].name, name, len) == 0)
              break;
        }

        if (i == NUMPROPS)
          continue;

        rlp = &proptbl[i];

        /*
         * Resize the range list if necessary.
         */
        if (rlp->used == rlp->size) {
            if (rlp->size == 0)
              rlp->ranges = (unsigned long *)
                  malloc(sizeof(unsigned long) << 3);
            else
              rlp->ranges = (unsigned long *)
                  realloc((char *) rlp->ranges,
                          sizeof(unsigned long) * (rlp->size + 8));
            rlp->size += 8;
        }

        /*
         * If this is the first code for this property list, just add it
         * and return.
         */
        if (rlp->used == 0) {
            rlp->ranges[0] = start;
            rlp->ranges[1] = end;
            rlp->used += 2;
            continue;
        }

        /*
         * Optimize the case of adding the range to the end.
         */
        j = rlp->used - 1;
        if (start > rlp->ranges[j]) {
            j = rlp->used;
            rlp->ranges[j++] = start;
            rlp->ranges[j++] = end;
            rlp->used = j;
            continue;
        }

        /*
         * Need to locate the insertion point.
         */
        for (i = 0;
             i < rlp->used && start > rlp->ranges[i + 1] + 1; i += 2) ;

        /*
         * If the start value lies in the current range, then simply set the
         * new end point of the range to the end value passed as a parameter.
         */
        if (rlp->ranges[i] <= start && start <= rlp->ranges[i + 1] + 1) {
            rlp->ranges[i + 1] = end;
            return;
        }

        /*
         * Shift following values up by two.
         */
        for (j = rlp->used; j > i; j -= 2) {
            rlp->ranges[j] = rlp->ranges[j - 2];
            rlp->ranges[j + 1] = rlp->ranges[j - 1];
        }

        /*
         * Add the new range at the insertion point.
         */
        rlp->ranges[i] = start;
        rlp->ranges[i + 1] = end;
        rlp->used += 2;
    }
}

static void
#ifdef __STDC__
ordered_range_insert(unsigned long c, char *name, int len)
#else
ordered_range_insert(c, name, len)
unsigned long c;
char *name;
int len;
#endif
{
    int i, j;
    unsigned long s, e;
    _ranges_t *rlp;

    if (len == 0)
      return;

    for (i = 0; i < NUMPROPS; i++) {
        if (props[i].len == len && memcmp(props[i].name, name, len) == 0)
          break;
    }

    if (i == NUMPROPS)
      return;

    /*
     * Have a match, so insert the code in order.
     */
    rlp = &proptbl[i];

    /*
     * Resize the range list if necessary.
     */
    if (rlp->used == rlp->size) {
        if (rlp->size == 0)
          rlp->ranges = (unsigned long *)
              malloc(sizeof(unsigned long) << 3);
        else
          rlp->ranges = (unsigned long *)
              realloc((char *) rlp->ranges,
                      sizeof(unsigned long) * (rlp->size + 8));
        rlp->size += 8;
    }

    /*
     * If this is the first code for this property list, just add it
     * and return.
     */
    if (rlp->used == 0) {
        rlp->ranges[0] = rlp->ranges[1] = c;
        rlp->used += 2;
        return;
    }

    /*
     * Optimize the cases of extending the last range and adding new ranges to
     * the end.
     */
    j = rlp->used - 1;
    e = rlp->ranges[j];
    s = rlp->ranges[j - 1];

    if (c == e + 1) {
        /*
         * Extend the last range.
         */
        rlp->ranges[j] = c;
        return;
    }

    if (c > e + 1) {
        /*
         * Start another range on the end.
         */
        j = rlp->used;
        rlp->ranges[j] = rlp->ranges[j + 1] = c;
        rlp->used += 2;
        return;
    }

    if (c >= s)
      /*
       * The code is a duplicate of a code in the last range, so just return.
       */
      return;

    /*
     * The code should be inserted somewhere before the last range in the
     * list.  Locate the insertion point.
     */
    for (i = 0;
         i < rlp->used && c > rlp->ranges[i + 1] + 1; i += 2) ;

    s = rlp->ranges[i];
    e = rlp->ranges[i + 1];

    if (c == e + 1)
      /*
       * Simply extend the current range.
       */
      rlp->ranges[i + 1] = c;
    else if (c < s) {
        /*
         * Add a new entry before the current location.  Shift all entries
         * before the current one up by one to make room.
         */
        for (j = rlp->used; j > i; j -= 2) {
            rlp->ranges[j] = rlp->ranges[j - 2];
            rlp->ranges[j + 1] = rlp->ranges[j - 1];
        }
        rlp->ranges[i] = rlp->ranges[i + 1] = c;

        rlp->used += 2;
    }
}

static void
#ifdef __STDC__
add_decomp(unsigned long code)
#else
add_decomp(code)
unsigned long code;
#endif
{
    unsigned long i, j, size;

    /*
     * Add the code to the composite property.
     */
    ordered_range_insert(code, "Cm", 2);

    /*
     * Locate the insertion point for the code.
     */
    for (i = 0; i < decomps_used && code > decomps[i].code; i++) ;

    /*
     * Allocate space for a new decomposition.
     */
    if (decomps_used == decomps_size) {
        if (decomps_size == 0)
          decomps = (_decomp_t *) malloc(sizeof(_decomp_t) << 3);
        else
          decomps = (_decomp_t *)
              realloc((char *) decomps,
                      sizeof(_decomp_t) * (decomps_size + 8));
        (void) memset((char *) (decomps + decomps_size), 0,
                      sizeof(_decomp_t) << 3);
        decomps_size += 8;
    }

    if (i < decomps_used && code != decomps[i].code) {
        /*
         * Shift the decomps up by one if the codes don't match.
         */
        for (j = decomps_used; j > i; j--)
          (void) memcpy((char *) &decomps[j], (char *) &decomps[j - 1],
                        sizeof(_decomp_t));
    }

    /*
     * Insert or replace a decomposition.
     */
    size = dectmp_size + (4 - (dectmp_size & 3));
    if (decomps[i].size < size) {
        if (decomps[i].size == 0)
          decomps[i].decomp = (unsigned long *)
              malloc(sizeof(unsigned long) * size);
        else
          decomps[i].decomp = (unsigned long *)
              realloc((char *) decomps[i].decomp,
                      sizeof(unsigned long) * size);
        decomps[i].size = size;
    }

    if (decomps[i].code != code)
      decomps_used++;

    decomps[i].code = code;
    decomps[i].used = dectmp_size;
    (void) memcpy((char *) decomps[i].decomp, (char *) dectmp,
                  sizeof(unsigned long) * dectmp_size);

}

static void
#ifdef __STDC__
add_title(unsigned long code)
#else
add_title(code)
unsigned long code;
#endif
{
    unsigned long i, j;

    /*
     * Always map the code to itself.
     */
    cases[2] = code;

    if (title_used == title_size) {
        if (title_size == 0)
          title = (_case_t *) malloc(sizeof(_case_t) << 3);
        else
          title = (_case_t *) realloc((char *) title,
                                      sizeof(_case_t) * (title_size + 8));
        title_size += 8;
    }

    /*
     * Locate the insertion point.
     */
    for (i = 0; i < title_used && code > title[i].key; i++) ;

    if (i < title_used) {
        /*
         * Shift the array up by one.
         */
        for (j = title_used; j > i; j--)
          (void) memcpy((char *) &title[j], (char *) &title[j - 1],
                        sizeof(_case_t));
    }

    title[i].key = cases[2];    /* Title */
    title[i].other1 = cases[0]; /* Upper */
    title[i].other2 = cases[1]; /* Lower */

    title_used++;
}

static void
#ifdef __STDC__
add_upper(unsigned long code)
#else
add_upper(code)
unsigned long code;
#endif
{
    unsigned long i, j;

    /*
     * Always map the code to itself.
     */
    cases[0] = code;

    /*
     * If the title case character is not present, then make it the same as
     * the upper case.
     */
    if (cases[2] == 0)
      cases[2] = code;

    if (upper_used == upper_size) {
        if (upper_size == 0)
          upper = (_case_t *) malloc(sizeof(_case_t) << 3);
        else
          upper = (_case_t *) realloc((char *) upper,
                                      sizeof(_case_t) * (upper_size + 8));
        upper_size += 8;
    }

    /*
     * Locate the insertion point.
     */
    for (i = 0; i < upper_used && code > upper[i].key; i++) ;

    if (i < upper_used) {
        /*
         * Shift the array up by one.
         */
        for (j = upper_used; j > i; j--)
          (void) memcpy((char *) &upper[j], (char *) &upper[j - 1],
                        sizeof(_case_t));
    }

    upper[i].key = cases[0];    /* Upper */
    upper[i].other1 = cases[1]; /* Lower */
    upper[i].other2 = cases[2]; /* Title */

    upper_used++;
}

static void
#ifdef __STDC__
add_lower(unsigned long code)
#else
add_lower(code)
unsigned long code;
#endif
{
    unsigned long i, j;

    /*
     * Always map the code to itself.
     */
    cases[1] = code;

    /*
     * If the title case character is empty, then make it the same as the
     * upper case.
     */
    if (cases[2] == 0)
      cases[2] = cases[0];

    if (lower_used == lower_size) {
        if (lower_size == 0)
          lower = (_case_t *) malloc(sizeof(_case_t) << 3);
        else
          lower = (_case_t *) realloc((char *) lower,
                                      sizeof(_case_t) * (lower_size + 8));
        lower_size += 8;
    }

    /*
     * Locate the insertion point.
     */
    for (i = 0; i < lower_used && code > lower[i].key; i++) ;

    if (i < lower_used) {
        /*
         * Shift the array up by one.
         */
        for (j = lower_used; j > i; j--)
          (void) memcpy((char *) &lower[j], (char *) &lower[j - 1],
                        sizeof(_case_t));
    }

    lower[i].key = cases[1];    /* Lower */
    lower[i].other1 = cases[0]; /* Upper */
    lower[i].other2 = cases[2]; /* Title */

    lower_used++;
}

static void
#ifdef __STDC__
ordered_ccl_insert(unsigned long c, unsigned long ccl_code)
#else
ordered_ccl_insert(c, ccl_code)
unsigned long c, ccl_code;
#endif
{
    unsigned long i, j;

    if (ccl_used == ccl_size) {
        if (ccl_size == 0)
          ccl = (unsigned long *) malloc(sizeof(unsigned long) * 24);
        else
          ccl = (unsigned long *)
              realloc((char *) ccl, sizeof(unsigned long) * (ccl_size + 24));
        ccl_size += 24;
    }

    /*
     * Optimize adding the first item.
     */
    if (ccl_used == 0) {
        ccl[0] = ccl[1] = c;
        ccl[2] = ccl_code;
        ccl_used += 3;
        return;
    }

    /*
     * Handle the special case of extending the range on the end.  This
     * requires that the combining class codes are the same.
     */
    if (ccl_code == ccl[ccl_used - 1] && c == ccl[ccl_used - 2] + 1) {
        ccl[ccl_used - 2] = c;
        return;
    }

    /*
     * Handle the special case of adding another range on the end.
     */
    if (c > ccl[ccl_used - 2] + 1 ||
        (c == ccl[ccl_used - 2] + 1 && ccl_code != ccl[ccl_used - 1])) {
        ccl[ccl_used++] = c;
        ccl[ccl_used++] = c;
        ccl[ccl_used++] = ccl_code;
        return;
    }

    /*
     * Locate either the insertion point or range for the code.
     */
    for (i = 0; i < ccl_used && c > ccl[i + 1] + 1; i += 3) ;

    if (ccl_code == ccl[i + 2] && c == ccl[i + 1] + 1) {
        /*
         * Extend an existing range.
         */
        ccl[i + 1] = c;
        return;
    } else if (c < ccl[i]) {
        /*
         * Start a new range before the current location.
         */
        for (j = ccl_used; j > i; j -= 3) {
            ccl[j] = ccl[j - 3];
            ccl[j - 1] = ccl[j - 4];
            ccl[j - 2] = ccl[j - 5];
        }
        ccl[i] = ccl[i + 1] = c;
        ccl[i + 2] = ccl_code;
    }
}

/*
 * Adds a number if it does not already exist and returns an index value
 * multiplied by 2.
 */
static unsigned long
#ifdef __STDC__
make_number(short num, short denom)
#else
make_number(num, denom)
short num, denom;
#endif
{
    unsigned long n;

    /*
     * Determine if the number already exists.
     */
    for (n = 0; n < nums_used; n++) {
        if (nums[n].numerator == num && nums[n].denominator == denom)
          return n << 1;
    }

    if (nums_used == nums_size) {
        if (nums_size == 0)
          nums = (_num_t *) malloc(sizeof(_num_t) << 3);
        else
          nums = (_num_t *) realloc((char *) nums,
                                    sizeof(_num_t) * (nums_size + 8));
        nums_size += 8;
    }

    n = nums_used++;
    nums[n].numerator = num;
    nums[n].denominator = denom;

    return n << 1;
}

static void
#ifdef __STDC__
add_number(unsigned long code, short num, short denom)
#else
add_number(code, num, denom)
unsigned long code;
short num, denom;
#endif
{
    unsigned long i, j;

    /*
     * Insert the code in order.
     */
    for (i = 0; i < ncodes_used && code > ncodes[i].code; i++) ;

    /*
     * Handle the case of the codes matching and simply replace the number
     * that was there before.
     */
    if (ncodes_used > 0 && code == ncodes[i].code) {
        ncodes[i].idx = make_number(num, denom);
        return;
    }

    /*
     * Resize the array if necessary.
     */
    if (ncodes_used == ncodes_size) {
        if (ncodes_size == 0)
          ncodes = (_codeidx_t *) malloc(sizeof(_codeidx_t) << 3);
        else
          ncodes = (_codeidx_t *)
              realloc((char *) ncodes, sizeof(_codeidx_t) * (ncodes_size + 8));

        ncodes_size += 8;
    }

    /*
     * Shift things around to insert the code if necessary.
     */
    if (i < ncodes_used) {
        for (j = ncodes_used; j > i; j--) {
            ncodes[j].code = ncodes[j - 1].code;
            ncodes[j].idx = ncodes[j - 1].idx;
        }
    }
    ncodes[i].code = code;
    ncodes[i].idx = make_number(num, denom);

    ncodes_used++;
}

/*
 * This routine assumes that the line is a valid Unicode Character Database
 * entry.
 */
static void
#ifdef __STDC__
read_cdata(FILE *in)
#else
read_cdata(in)
FILE *in;
#endif
{
    unsigned long i, lineno, skip, code, ccl_code;
    short wnum, neg, number[2];
    char line[512], *s, *e;

    lineno = skip = 0;
    while (fscanf(in, "%[^\n]\n", line) != EOF) {
        lineno++;

        /*
         * Skip blank lines and lines that start with a '#'.
         */
        if (line[0] == 0 || line[0] == '#')
          continue;

        /*
         * If lines need to be skipped, do it here.
         */
        if (skip) {
            skip--;
            continue;
        }

        /*
         * Collect the code.  The code can be up to 6 hex digits in length to
         * allow surrogates to be specified.
         */
        for (s = line, i = code = 0; *s != ';' && i < 6; i++, s++) {
            code <<= 4;
            if (*s >= '0' && *s <= '9')
              code += *s - '0';
            else if (*s >= 'A' && *s <= 'F')
              code += (*s - 'A') + 10;
            else if (*s >= 'a' && *s <= 'f')
              code += (*s - 'a') + 10;
        }

        /*
         * Handle the following special cases:
         * 1. 4E00-9FA5 CJK Ideographs.
         * 2. AC00-D7A3 Hangul Syllables.
         * 3. D800-DFFF Surrogates.
         * 4. E000-F8FF Private Use Area.
         * 5. F900-FA2D Han compatibility.
         */
        switch (code) {
          case 0x4e00:
            /*
             * The Han ideographs.
             */
            add_range(0x4e00, 0x9fff, "Lo", "L");

            /*
             * Add the characters to the defined category.
             */
            add_range(0x4e00, 0x9fa5, "Cp", 0);

            skip = 1;
            break;
          case 0xac00:
            /*
             * The Hangul syllables.
             */
            add_range(0xac00, 0xd7a3, "Lo", "L");

            /*
             * Add the characters to the defined category.
             */
            add_range(0xac00, 0xd7a3, "Cp", 0);

            skip = 1;
            break;
          case 0xd800:
            /*
             * Make a range of all surrogates and assume some default
             * properties.
             */
            add_range(0x010000, 0x10ffff, "Cs", "L");
            skip = 5;
            break;
          case 0xe000:
            /*
             * The Private Use area.  Add with a default set of properties.
             */
            add_range(0xe000, 0xf8ff, "Co", "L");
            skip = 1;
            break;
          case 0xf900:
            /*
             * The CJK compatibility area.
             */
            add_range(0xf900, 0xfaff, "Lo", "L");

            /*
             * Add the characters to the defined category.
             */
            add_range(0xf900, 0xfaff, "Cp", 0);

            skip = 1;
        }

        if (skip)
          continue;

        /*
         * Add the code to the defined category.
         */
        ordered_range_insert(code, "Cp", 2);

        /*
         * Locate the first character property field.
         */
        for (i = 0; *s != 0 && i < 2; s++) {
            if (*s == ';')
              i++;
        }
        for (e = s; *e && *e != ';'; e++) ;
    
        ordered_range_insert(code, s, e - s);

        /*
         * Locate the combining class code.
         */
        for (s = e; *s != 0 && i < 3; s++) {
            if (*s == ';')
              i++;
        }

        /*
         * Convert the combining class code from decimal.
         */
        for (ccl_code = 0, e = s; *e && *e != ';'; e++)
          ccl_code = (ccl_code * 10) + (*e - '0');

        /*
         * Add the code if it not 0.
         */
        if (ccl_code != 0)
          ordered_ccl_insert(code, ccl_code);

        /*
         * Locate the second character property field.
         */
        for (s = e; *s != 0 && i < 4; s++) {
            if (*s == ';')
              i++;
        }
        for (e = s; *e && *e != ';'; e++) ;

        ordered_range_insert(code, s, e - s);

        /*
         * Check for a decomposition.
         */
        s = ++e;
        if (*s != ';' && *s != '<') {
            /*
             * Collect the codes of the decomposition.
             */
            for (dectmp_size = 0; *s != ';'; ) {
                /*
                 * Skip all leading non-hex digits.
                 */
                while (!ishdigit(*s))
                  s++;

                for (dectmp[dectmp_size] = 0; ishdigit(*s); s++) {
                    dectmp[dectmp_size] <<= 4;
                    if (*s >= '0' && *s <= '9')
                      dectmp[dectmp_size] += *s - '0';
                    else if (*s >= 'A' && *s <= 'F')
                      dectmp[dectmp_size] += (*s - 'A') + 10;
                    else if (*s >= 'a' && *s <= 'f')
                      dectmp[dectmp_size] += (*s - 'a') + 10;
                }
                dectmp_size++;
            }

            /*
             * If there is more than one code in the temporary decomposition
             * array, then add the character with its decomposition.
             */
            if (dectmp_size > 1)
              add_decomp(code);
        }

        /*
         * Skip to the number field.
         */
        for (i = 0; i < 3 && *s; s++) {
            if (*s == ';')
              i++;
        }

        /*
         * Scan the number in.
         */
        number[0] = number[1] = 0;
        for (e = s, neg = wnum = 0; *e && *e != ';'; e++) {
            if (*e == '-') {
                neg = 1;
                continue;
            }

            if (*e == '/') {
                /*
                 * Move the the denominator of the fraction.
                 */
                if (neg)
                  number[wnum] *= -1;
                neg = 0;
                e++;
                wnum++;
            }
            number[wnum] = (number[wnum] * 10) + (*e - '0');
        }

        if (e > s) {
            /*
             * Adjust the denominator in case of integers and add the number.
             */
            if (wnum == 0)
              number[1] = number[0];

            add_number(code, number[0], number[1]);
        }

        /*
         * Skip to the start of the possible case mappings.
         */
        for (s = e, i = 0; i < 4 && *s; s++) {
            if (*s == ';')
              i++;
        }

        /*
         * Collect the case mappings.
         */
        cases[0] = cases[1] = cases[2] = 0;
        for (i = 0; i < 3; i++) {
            while (ishdigit(*s)) {
                cases[i] <<= 4;
                if (*s >= '0' && *s <= '9')
                  cases[i] += *s - '0';
                else if (*s >= 'A' && *s <= 'F')
                  cases[i] += (*s - 'A') + 10;
                else if (*s >= 'a' && *s <= 'f')
                  cases[i] += (*s - 'a') + 10;
                s++;
            }
            if (*s == ';')
              s++;
        }
        if (cases[0] && cases[1])
          /*
           * Add the upper and lower mappings for a title case character.
           */
          add_title(code);
        else if (cases[1])
          /*
           * Add the lower and title case mappings for the upper case
           * character.
           */
          add_upper(code);
        else if (cases[0])
          /*
           * Add the upper and title case mappings for the lower case
           * character.
           */
          add_lower(code);
    }
}

static _decomp_t *
#ifdef __STDC__
find_decomp(unsigned long code)
#else
find_decomp(code)
unsigned long code;
#endif
{
    long l, r, m;

    l = 0;
    r = decomps_used - 1;
    while (l <= r) {
        m = (l + r) >> 1;
        if (code > decomps[m].code)
          l = m + 1;
        else if (code < decomps[m].code)
          r = m - 1;
        else
          return &decomps[m];
    }
    return 0;
}

static void
#ifdef __STDC__
decomp_it(_decomp_t *d)
#else
decomp_it(d)
_decomp_t *d;
#endif
{
    unsigned long i;
    _decomp_t *dp;

    for (i = 0; i < d->used; i++) {
        if ((dp = find_decomp(d->decomp[i])) != 0)
          decomp_it(dp);
        else
          dectmp[dectmp_size++] = d->decomp[i];
    }
}

/*
 * Expand all decompositions by recursively decomposing each character
 * in the decomposition.
 */
static void
#ifdef __STDC__
expand_decomp(void)
#else
expand_decomp()
#endif
{
    unsigned long i;

    for (i = 0; i < decomps_used; i++) {
        dectmp_size = 0;
        decomp_it(&decomps[i]);
        if (dectmp_size > 0)
          add_decomp(decomps[i].code);
    }
}

static void
#ifdef __STDC__
write_cdata(char *opath)
#else
write_cdata(opath)
char *opath;
#endif
{
    FILE *out;
    unsigned long i, idx, bytes, nprops;
    unsigned short casecnt[2];
    char path[BUFSIZ];

    /*****************************************************************
     *
     * Generate the ctype data.
     *
     *****************************************************************/

    /*
     * Open the ctype.dat file.
     */
    sprintf(path, "%s/ctype.dat", opath);
    if ((out = fopen(path, "wb")) == 0)
      return;

    /*
     * Collect the offsets for the properties.  The offsets array is
     * on a 4-byte boundary to keep things efficient for architectures
     * that need such a thing.
     */
    for (i = idx = 0; i < NUMPROPS; i++) {
        propcnt[i] = (proptbl[i].used != 0) ? idx : 0xffff;
        idx += proptbl[i].used;
    }

    /*
     * Add the sentinel index which is used by the binary search as the upper
     * bound for a search.
     */
    propcnt[i] = idx;

    /*
     * Record the actual number of property lists.  This may be different than
     * the number of offsets actually written because of aligning on a 4-byte
     * boundary.
     */
    hdr[1] = NUMPROPS;

    /*
     * Calculate the byte count needed and pad the property counts array to a
     * 4-byte boundary.
     */
    if ((bytes = sizeof(unsigned short) * (NUMPROPS + 1)) & 3)
      bytes += 4 - (bytes & 3);
    nprops = bytes / sizeof(unsigned short);
    bytes += sizeof(unsigned long) * idx;
        
    /*
     * Write the header.
     */
    fwrite((char *) hdr, sizeof(unsigned short), 2, out);

    /*
     * Write the byte count.
     */
    fwrite((char *) &bytes, sizeof(unsigned long), 1, out);

    /*
     * Write the property list counts.
     */
    fwrite((char *) propcnt, sizeof(unsigned short), nprops, out);

    /*
     * Write the property lists.
     */
    for (i = 0; i < NUMPROPS; i++) {
        if (proptbl[i].used > 0)
          fwrite((char *) proptbl[i].ranges, sizeof(unsigned long),
                 proptbl[i].used, out);
    }

    fclose(out);

    /*****************************************************************
     *
     * Generate the case mapping data.
     *
     *****************************************************************/

    /*
     * Open the case.dat file.
     */
    sprintf(path, "%s/case.dat", opath);
    if ((out = fopen(path, "wb")) == 0)
      return;

    /*
     * Write the case mapping tables.
     */
    hdr[1] = upper_used + lower_used + title_used;
    casecnt[0] = upper_used;
    casecnt[1] = lower_used;

    /*
     * Write the header.
     */
    fwrite((char *) hdr, sizeof(unsigned short), 2, out);

    /*
     * Write the upper and lower case table sizes.
     */
    fwrite((char *) casecnt, sizeof(unsigned short), 2, out);

    if (upper_used > 0)
      /*
       * Write the upper case table.
       */
      fwrite((char *) upper, sizeof(_case_t), upper_used, out);

    if (lower_used > 0)
      /*
       * Write the lower case table.
       */
      fwrite((char *) lower, sizeof(_case_t), lower_used, out);

    if (title_used > 0)
      /*
       * Write the title case table.
       */
      fwrite((char *) title, sizeof(_case_t), title_used, out);

    fclose(out);

    /*****************************************************************
     *
     * Generate the decomposition data.
     *
     *****************************************************************/

    /*
     * Fully expand all decompositions before generating the output file.
     */
    expand_decomp();

    /*
     * Open the decomp.dat file.
     */
    sprintf(path, "%s/decomp.dat", opath);
    if ((out = fopen(path, "wb")) == 0)
      return;

    hdr[1] = decomps_used;

    /*
     * Write the header.
     */
    fwrite((char *) hdr, sizeof(unsigned short), 2, out);

    /*
     * Write a temporary byte count which will be calculated as the
     * decompositions are written out.
     */
    bytes = 0;
    fwrite((char *) &bytes, sizeof(unsigned long), 1, out);

    if (decomps_used) {
        /*
         * Write the list of decomp nodes.
         */
        for (i = idx = 0; i < decomps_used; i++) {
            fwrite((char *) &decomps[i].code, sizeof(unsigned long), 1, out);
            fwrite((char *) &idx, sizeof(unsigned long), 1, out);
            idx += decomps[i].used;
        }

        /*
         * Write the sentinel index as the last decomp node.
         */
        fwrite((char *) &idx, sizeof(unsigned long), 1, out);

        /*
         * Write the decompositions themselves.
         */
        for (i = 0; i < decomps_used; i++)
          fwrite((char *) decomps[i].decomp, sizeof(unsigned long),
                 decomps[i].used, out);

        /*
         * Seek back to the beginning and write the byte count.
         */
        bytes = (sizeof(unsigned long) * idx) +
            (sizeof(unsigned long) * ((hdr[1] << 1) + 1));
        fseek(out, sizeof(unsigned short) << 1, 0L);
        fwrite((char *) &bytes, sizeof(unsigned long), 1, out);

        fclose(out);
    }

    /*****************************************************************
     *
     * Generate the combining class data.
     *
     *****************************************************************/

    /*
     * Open the cmbcl.dat file.
     */
    sprintf(path, "%s/cmbcl.dat", opath);
    if ((out = fopen(path, "wb")) == 0)
      return;

    /*
     * Set the number of ranges used.  Each range has a combining class which
     * means each entry is a 3-tuple.
     */
    hdr[1] = ccl_used / 3;

    /*
     * Write the header.
     */
    fwrite((char *) hdr, sizeof(unsigned short), 2, out);

    /*
     * Write out the byte count to maintain header size.
     */
    bytes = ccl_used * sizeof(unsigned long);
    fwrite((char *) &bytes, sizeof(unsigned long), 1, out);

    if (ccl_used > 0)
      /*
       * Write the combining class ranges out.
       */
      fwrite((char *) ccl, sizeof(unsigned long), ccl_used, out);

    fclose(out);

    /*****************************************************************
     *
     * Generate the number data.
     *
     *****************************************************************/

    /*
     * Open the num.dat file.
     */
    sprintf(path, "%s/num.dat", opath);
    if ((out = fopen(path, "wb")) == 0)
      return;

    /*
     * The count part of the header will be the total number of codes that
     * have numbers.
     */
    hdr[1] = (unsigned short) (ncodes_used << 1);
    bytes = (ncodes_used * sizeof(_codeidx_t)) + (nums_used * sizeof(_num_t));

    /*
     * Write the header.
     */
    fwrite((char *) hdr, sizeof(unsigned short), 2, out);

    /*
     * Write out the byte count to maintain header size.
     */
    fwrite((char *) &bytes, sizeof(unsigned long), 1, out);

    /*
     * Now, if number mappings exist, write them out.
     */
    if (ncodes_used > 0) {
        fwrite((char *) ncodes, sizeof(_codeidx_t), ncodes_used, out);
        fwrite((char *) nums, sizeof(_num_t), nums_used, out);
    }

    fclose(out);
}

void
#ifdef __STDC__
main(int argc, char *argv[])
#else
main(argc, argv)
int argc;
char *argv[];
#endif
{
    FILE *in;
    char *prog, *opath;

    if ((prog = strrchr(argv[0], '/')) != 0)
      prog++;
    else
      prog = argv[0];

    opath = 0;
    in = stdin;

    argc--;
    argv++;

    while (argc > 0) {
        if (argv[0][0] == '-' && argv[0][1] == 'o') {
            argc--;
            argv++;
            opath = argv[0];
        } else {
            if (in != stdin)
              fclose(in);
            if ((in = fopen(argv[0], "rb")) == 0)
              fprintf(stderr, "%s: unable to open ctype file %s\n",
                      prog, argv[0]);
            else {
                read_cdata(in);
                fclose(in);
                in = 0;
            }
        }
        argc--;
        argv++;
    }

    if (opath == 0)
      opath = ".";
    write_cdata(opath);

    exit(0);
}
