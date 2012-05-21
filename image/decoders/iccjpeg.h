/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * iccprofile.h
 *
 * This file provides code to read and write International Color Consortium
 * (ICC) device profiles embedded in JFIF JPEG image files.  The ICC has
 * defined a standard format for including such data in JPEG "APP2" markers.
 * The code given here does not know anything about the internal structure
 * of the ICC profile data; it just knows how to put the profile data into
 * a JPEG file being written, or get it back out when reading.
 *
 * This code depends on new features added to the IJG JPEG library as of
 * IJG release 6b; it will not compile or work with older IJG versions.
 *
 * NOTE: this code would need surgery to work on 16-bit-int machines
 * with ICC profiles exceeding 64K bytes in size.  See iccprofile.c
 * for details.
 */

#include <stdio.h>		/* needed to define "FILE", "NULL" */
#include "jpeglib.h"

/*
 * Reading a JPEG file that may contain an ICC profile requires two steps:
 *
 * 1. After jpeg_create_decompress() but before jpeg_read_header(),
 *    call setup_read_icc_profile().  This routine tells the IJG library
 *    to save in memory any APP2 markers it may find in the file.
 *
 * 2. After jpeg_read_header(), call read_icc_profile() to find out
 *    whether there was a profile and obtain it if so.
 */


/*
 * Prepare for reading an ICC profile
 */

extern void setup_read_icc_profile JPP((j_decompress_ptr cinfo));


/*
 * See if there was an ICC profile in the JPEG file being read;
 * if so, reassemble and return the profile data.
 *
 * TRUE is returned if an ICC profile was found, FALSE if not.
 * If TRUE is returned, *icc_data_ptr is set to point to the
 * returned data, and *icc_data_len is set to its length.
 *
 * IMPORTANT: the data at **icc_data_ptr has been allocated with malloc()
 * and must be freed by the caller with free() when the caller no longer
 * needs it.  (Alternatively, we could write this routine to use the
 * IJG library's memory allocator, so that the data would be freed implicitly
 * at jpeg_finish_decompress() time.  But it seems likely that many apps
 * will prefer to have the data stick around after decompression finishes.)
 */

extern boolean read_icc_profile JPP((j_decompress_ptr cinfo,
				     JOCTET **icc_data_ptr,
				     unsigned int *icc_data_len));
